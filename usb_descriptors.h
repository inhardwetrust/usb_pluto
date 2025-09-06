#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#include "xil_types.h"

// --- Device descriptor ---
extern const u8 DevDesc[];
extern const unsigned DevDescLen;

// --- High-Speed configuration (Config + Interface + EP1 IN + EP1 OUT) ---
extern const u8 CfgDesc_HS[];
extern const unsigned CfgDesc_HS_Len;


// --- Device Qualifier (0x06) ---
extern const u8 DevQualifier[];
extern const unsigned DevQualifierLen;


// --- Other-Speed Configuration (0x07) ---
extern const u8 OtherSpeedDesc_HS[];
extern const unsigned OtherSpeedDesc_HS_Len;


// --- String descriptors ---
extern const u8 Str0[];       // LANGID
extern const u8 StrManuf[];   // Manufacturer
extern const u8 StrProd[];    // Product
extern const u8 StrSN[];      // Serial Number

#define USB_STRING_COUNT 4

// Correct descriptors for WIN to detect it as WinUsb...
// --- MS OS 1.0 (WCID) ---
#define MS_OS_20_DISABLED 1  // (leave 1.0 path)
#define MS_OS_10_VENDOR_CODE  0x17  // any byte, but must be the same everywhere

extern const u8 MS_OS_StringDescriptor[];   // index 0xEE
extern const u8 MS_OS_CompatID[];           // wIndex=0x0004
extern const unsigned MS_OS_CompatID_Len;

#endif /* USB_DESCRIPTORS_H */
