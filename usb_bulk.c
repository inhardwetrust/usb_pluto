#include "xusbps.h"
#include "xil_types.h"


 void Ep1_In_Handler(void *CallBackRef, u8 EpNum, u8 EventType, void *Data)
{
    if (EventType == XUSBPS_EP_EVENT_DATA_TX) {
        u32 handle = (u32)(UINTPTR)Data;
        XUsbPs_EpBufferRelease(handle);   // ОБЯЗАТЕЛЬНО!
        // тут можно сразу поставить следующую передачу, если нужно
    }
}
