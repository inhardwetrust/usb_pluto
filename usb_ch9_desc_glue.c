// usb_ch9_desc_glue.c — minimal fo descriptors requests
#include <string.h>
#include "xusbps_ch9.h"
#include "usb_descriptors.h"   // DevDesc, DevDescLen, CfgDesc_HS, CfgDesc_HS_Len, Str0/1/2/3

int XUsbPs_Ch9SetupDevDescReply(u8 *BufPtr, int BufLen)
{
    int n = (int)DevDescLen;
    if (n > BufLen) n = BufLen;
    memcpy(BufPtr, DevDesc, n);
    return n;
}

int XUsbPs_Ch9SetupCfgDescReply(u8 *BufPtr, int BufLen)
{
    int n = (int)CfgDesc_HS_Len;
    if (n > BufLen) n = BufLen;
    memcpy(BufPtr, CfgDesc_HS, n);
    return n;
}

int XUsbPs_Ch9SetupStrDescReply(u8 *BufPtr, int BufLen, u8 Index)
{
    // Chose needed string-descriptor
    const u8 *src = 0;
    switch (Index) {
        case 0: src = Str0;      break;  // LANGID
        case 1: src = StrManuf;  break;  // Manufacturer
        case 2: src = StrProd;   break;  // Product
        case 3: src = StrSN;     break;  // Serial
        // <-- MS OS String descriptor (index 0xEE)
        case 0xEE: src = MS_OS_StringDescriptor; break;
        default:
            return 0; // unknown index — return 0 bytes (host will get STALL)
    }

    // First byte of string-descriptor — its length
    int n = src[0];
    if (n > BufLen) n = BufLen;
    memcpy(BufPtr, src, n);
    return n;
}




int XUsbPs_Ch9SetupDevQualifierReply(u8 *BufPtr, int BufLen)
{
    int n = (int)DevQualifierLen;
    if (n > BufLen) n = BufLen;
    memcpy(BufPtr, DevQualifier, n);
    return n;
}

int XUsbPs_Ch9SetupOtherSpeedCfgDescReply(u8 *BufPtr, int BufLen)
{
    int n = (int)OtherSpeedDesc_HS_Len;
    if (n > BufLen) n = BufLen;
    memcpy(BufPtr, OtherSpeedDesc_HS, n);
    return n;
}

