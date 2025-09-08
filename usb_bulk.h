#pragma once
#include "xusbps.h"
#include <stdint.h>


void Ep1_In_Handler(void *CallBackRef, u8 EpNum, u8 EventType, void *Data);

#ifdef __cplusplus
}
#endif
