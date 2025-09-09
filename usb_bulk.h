#pragma once
#include "xusbps.h"
#include <stdint.h>
#include "xgpiops.h"


void Ep1_In_Handler(void *CallBackRef, u8 EpNum, u8 EventType, void *Data);
void usb_bulk_set_gpio(XGpioPs *gpio, uint32_t pin);

#ifdef __cplusplus
}
#endif
