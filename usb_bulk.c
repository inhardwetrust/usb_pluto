#include "usb_bulk.h"
#include "xil_types.h"    // UINTPTR, u8/u32 (обычно тянется из xusbps.h, но пусть явно)
#include "xil_cache.h"    // Xil_DCacheFlushRange
#include <string.h>       // memset

#ifndef ALIGNMENT_CACHELINE
#define ALIGNMENT_CACHELINE __attribute__((aligned(32)))
#endif

/* === GPIO context === */
static XGpioPs *g_gpio = NULL;
static uint32_t g_gpio_pin = 0xFFFFFFFF;   // invalid pin -> GPIO не активен

void usb_bulk_set_gpio(XGpioPs *gpio, uint32_t pin)
{
    g_gpio = gpio;
    g_gpio_pin = pin;
}

/* Локальный указатель на контроллер USB, задаётся из main через usb_bulk_set_instance() */
static XUsbPs *g_inst = NULL;

/* Один большой буфер для разовой отправки */
static u8 txbuf[10] ALIGNMENT_CACHELINE;

void usb_bulk_set_instance(XUsbPs *inst)
{
    g_inst = inst;
}

void one_send_prepare(void)
{
    memset(txbuf, 0, sizeof(txbuf));
    txbuf[0] = 0xFF;
    txbuf[1] = 0xFF;
}

int one_send(void)
{

	one_send_prepare();

    /* Перед передачей обязательно флашим кэш */
    Xil_DCacheFlushRange((UINTPTR)txbuf, sizeof(txbuf));


    /* Одна отправка в EP1 IN; драйвер сам порежет на 512-байтные пакеты */
    int status = XUsbPs_EpBufferSend(g_inst, /*ep=*/1, txbuf, sizeof(txbuf));
    XGpioPs_WritePin(g_gpio, g_gpio_pin, 0);
    return status;

}

/* Хэндлер завершения передачи на EP1 IN */
void Ep1_In_Handler(void *CallBackRef, u8 EpNum, u8 EventType, void *Data)
{
    (void)EpNum;
    (void)CallBackRef;

    XGpioPs_WritePin(g_gpio, g_gpio_pin, 1);

    if (EventType == XUSBPS_EP_EVENT_DATA_TX) {
        u32 handle = (u32)(UINTPTR)Data;
        XUsbPs_EpBufferRelease(handle);  // ОБЯЗАТЕЛЬНО освобождаем передачу
        // здесь можно сразу поставить следующую отправку, если нужно
        // one_send();
    }
}
