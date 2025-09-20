#pragma once
#include "xusbps.h"
#include <stdint.h>
#include "xgpiops.h"

#define DMA_CHUNK_BYTES 64
#define USB_CHUNK_MAX  64

void Ep1_In_Handler(void *CallBackRef, u8 EpNum, u8 EventType, void *Data);
void usb_bulk_set_gpio(XGpioPs *gpio, uint32_t pin);

static size_t rb_dma_write(uint8_t *dst, size_t len, uint8_t val);
size_t rb_write(uint8_t val);
