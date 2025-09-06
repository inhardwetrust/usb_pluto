#pragma once
#include "xusbps.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*usb_bulk_rx_cb_t)(const uint8_t* data, uint32_t len);

/* Инициализация очереди и привязка к контроллеру */
void usb_bulk_init(XUsbPs *ip, uint32_t ep_num_in, uint32_t ep_num_out, uint32_t max_packet);

/* Необязательный приём: установить колбэк для EP OUT */
void usb_bulk_set_rx_handler(usb_bulk_rx_cb_t cb);

/* Поставить данные на отправку в EP IN (неблокирующе).
   Возвращает кол-во реально принятых в очередь байт (0..len). */
uint32_t usb_bulk_write(const uint8_t* data, uint32_t len);

/* Можно ли прямо сейчас отправить без очереди (true/false) */
int usb_bulk_can_write(void);

/* Внутренние вызовы из ISR/handlers */
void usb_bulk_on_out(const uint8_t *buf, uint32_t len);  // EP OUT data RXed
void usb_bulk_on_tx_done(void);                          // EP IN TX complete
void usb_bulk_kick(void);                                // Попытаться отправить следующий


