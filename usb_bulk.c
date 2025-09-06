#include "usb_bulk.h"
#include "xil_cache.h"
#include "xusbps.h"
#include <string.h>

/* --- настройки очереди --- */
#ifndef USB_BULK_TX_SLOTS
#define USB_BULK_TX_SLOTS   8
#endif

#ifndef USB_BULK_TX_SLOT_SIZE
#define USB_BULK_TX_SLOT_SIZE  4096   /* можно 512/1024/4096 — под свой сценарий */
#endif

#ifndef ALIGNMENT_CACHELINE
#define ALIGNMENT_CACHELINE __attribute__((aligned(32)))
#endif

#ifndef DCACHE_ALIGN_UP
#define DCACHE_ALIGN_UP(n)  (((n) + 31u) & ~31u)
#endif

static XUsbPs *g_ip = NULL;
static uint32_t g_ep_in  = 1;
static uint32_t g_ep_out = 1;
static uint32_t g_maxpkt = 512;

static usb_bulk_rx_cb_t g_rx_cb = NULL;

/* Очередь передаточных слотов */
static uint8_t  tx_buf[USB_BULK_TX_SLOTS][USB_BULK_TX_SLOT_SIZE] ALIGNMENT_CACHELINE;
static uint32_t tx_len[USB_BULK_TX_SLOTS];
static volatile uint32_t q_head = 0; // куда писать
static volatile uint32_t q_tail = 0; // откуда отправлять
static volatile int tx_in_flight = 0;

static inline int q_is_empty(void) { return q_head == q_tail; }
static inline int q_is_full(void)  { return ((q_head + 1u) % USB_BULK_TX_SLOTS) == q_tail; }

void usb_bulk_init(XUsbPs *ip, uint32_t ep_num_in, uint32_t ep_num_out, uint32_t max_packet)
{
    g_ip     = ip;
    g_ep_in  = ep_num_in;
    g_ep_out = ep_num_out;
    g_maxpkt = max_packet ? max_packet : 512;

    q_head = q_tail = 0;
    tx_in_flight = 0;
}

void usb_bulk_set_rx_handler(usb_bulk_rx_cb_t cb)
{
    g_rx_cb = cb;
}

int usb_bulk_can_write(void)
{
    return !q_is_full();
}

uint32_t usb_bulk_write(const uint8_t* data, uint32_t len)
{
    if (!g_ip || len == 0) return 0;

    /* Если нет места — ничего не копируем */
    if (q_is_full()) return 0;

    /* Обрезаем слишком длинное (одним слотом) */
    if (len > USB_BULK_TX_SLOT_SIZE) len = USB_BULK_TX_SLOT_SIZE;

    uint32_t idx = q_head;
    memcpy(tx_buf[idx], data, len);
    tx_len[idx] = len;

    /* Двигаем хвост очереди */
    q_head = (q_head + 1u) % USB_BULK_TX_SLOTS;

    /* Попытаемся сразу пнуть отправку (если простаивает) */
    usb_bulk_kick();
    return len;
}

void usb_bulk_kick(void)
{
    if (!g_ip) return;
    if (tx_in_flight) return;
    if (q_is_empty()) return;

    uint32_t idx = q_tail;
    uint32_t len = tx_len[idx];

    /* flush кэш */
    Xil_DCacheFlushRange((UINTPTR)tx_buf[idx], DCACHE_ALIGN_UP(len));

    /* Важно: если len является точной кратностью max_packet и ты хочешь
       «пометить конец передачи», иногда требуется ZLP. Это зависит от
       протокола хоста. В большинстве случаев хост сам знает ожидаемую длину.
       Базовый путь: просто отправляем len как есть. */

    int st = XUsbPs_EpBufferSend(g_ip, g_ep_in, tx_buf[idx], len);
    if (st == XST_SUCCESS) {
        tx_in_flight = 1; // ждём XUSBPS_EP_EVENT_DATA_TX
    } else {
        /* Если не получилось — не меняем очереди. Можно повторить позже. */
    }
}

void usb_bulk_on_tx_done(void)
{
    /* Освобождаем слот и запускаем следующий */
    if (!q_is_empty()) {
        q_tail = (q_tail + 1u) % USB_BULK_TX_SLOTS;
    }
    tx_in_flight = 0;
    usb_bulk_kick();
}

void usb_bulk_on_out(const uint8_t *buf, uint32_t len)
{
    if (!buf || len == 0) return;
    /* Можно просто вызвать пользовательский колбэк, либо игнорировать */
    if (g_rx_cb) {
        /* Кэш уже инвалидирован на верхнем уровне (см. твой Ep1 OUT handler) */
        g_rx_cb(buf, len);
    }
    /* Ничего не копим — сразу пусть пользователь решает, что делать. */
}
