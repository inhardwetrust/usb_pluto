#include "usb_descriptors.h"

// ---------------- Device descriptor ----------------
const u8 DevDesc[] = { 18, 0x01,        // bLength, bDescriptorType (Device)
		0x00, 0x02,      // bcdUSB 2.00
		0x00,            // bDeviceClass (per interface)
		0x00,            // bDeviceSubClass
		0x00,            // bDeviceProtocol
		64,              // bMaxPacketSize0 (EP0 = 64)
		0x34, 0x12, // idVendor (example 0x1234) ¸ VID/PID
		0x79, 0x56,      // idProduct (example 0x5678)
		0x00, 0x01,      // bcdDevice 1.00
		1,               // iManufacturer
		2,               // iProduct
		3,               // iSerialNumber
		1                // bNumConfigurations
		};
const unsigned DevDescLen = sizeof(DevDesc);

// ------------- Configuration (HS): 1 IF, 2 Bulk EP (EP1 IN/OUT) -------------
const u8 CfgDesc_HS[] = {
// Configuration descriptor
		9, 0x02,               // bLength, bDescriptorType (Configuration)
		(9 + 9 + 7 + 7), 0x00,       // wTotalLength = 32
		0x01,                  // bNumInterfaces
		0x01,                  // bConfigurationValue
		0x00,                  // iConfiguration
		0x80,                  // bmAttributes = bus powered
		50,                    // bMaxPower = 100 mA

		// Interface descriptor (vendor-specific)
		9, 0x04,               // bLength, bDescriptorType (Interface)
		0x00,                  // bInterfaceNumber
		0x00,                  // bAlternateSetting
		0x02,                  // bNumEndpoints = 2
		0xFF,                  // bInterfaceClass = Vendor Specific
		0x00,                  // bInterfaceSubClass
		0x00,                  // bInterfaceProtocol
		0x00,                  // iInterface

		// Endpoint 1 IN (device -> host), Bulk, 512 bytes (HS)
		7, 0x05,               // bLength, bDescriptorType (Endpoint)
		0x81,                  // bEndpointAddress = IN | EP1
		0x02,                  // bmAttributes = Bulk
		0x00, 0x02,            // wMaxPacketSize = 512
		0x00,                  // bInterval

		// Endpoint 1 OUT (host -> device), Bulk, 512 bytes (HS)
		7, 0x05,               // bLength, bDescriptorType (Endpoint)
		0x01,                  // bEndpointAddress = OUT | EP1
		0x02,                  // bmAttributes = Bulk
		0x00, 0x02,            // wMaxPacketSize = 512
		0x00                   // bInterval
		};
const unsigned CfgDesc_HS_Len = sizeof(CfgDesc_HS);

// Device Qualifier (10 bytes, for HS device — descriptor  FS-features)
const u8 DevQualifier[] = { 10,        // bLength
		0x06,      // bDescriptorType = DEVICE_QUALIFIER
		0x00, 0x02,      // bcdUSB 2.00
		0xFF, // bDeviceClass (vendor-specific; можно 0x00 если "per interface")
		0xFF,      // bDeviceSubClass
		0xFF,      // bDeviceProtocol
		0x40,      // bMaxPacketSize0 = 64   <-- Important
		0x01,      // bNumConfigurations = 1 <-- Important
		0x00       // bReserved
		};
const unsigned DevQualifierLen = sizeof(DevQualifier);

// Other-Speed Configuration, которую хост спросит как 0x07
// For HS-link here is FS-version of configuration (64-bytes bulk EP)
const u8 OtherSpeedDesc_HS[] = { 9, 0x07, (9 + 9 + 7 + 7), 0x00, // bLength, bDescriptorType(OtherSpeed), wTotalLength
		0x01, 0x01, 0x00, 0x80, 50, // bNumInterfaces, bConfigurationValue, iConfiguration, bmAttr, bMaxPower
		9, 0x04, 0x00, 0x00, 0x02, 0xFF, 0xFF, 0xFF, 0x00, // IF (FF/FF/FF convenient for WinUSB)
		7, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00, // EP1 IN bulk 64
		7, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00  // EP1 OUT bulk 64
		};
const unsigned OtherSpeedDesc_HS_Len = sizeof(OtherSpeedDesc_HS);


// ------------- MS OS String (index 0xEE): "MSFT100", bVendorCode, 0x00 -------------
const u8 MS_OS_StringDescriptor[] = {
    18, 0x03,
    'M',0,'S',0,'F',0,'T',0,'1',0,'0',0,'0',0,
    MS_OS_10_VENDOR_CODE, 0x00
};

// Extended Compat ID (wIndex=0x0004).
// One interface (bFirstInterfaceNumber = 0), CompatibleID="WINUSB"
const u8 MS_OS_CompatID[] = {
    // Header (16 bytes)
    0x28,0x00,0x00,0x00,  // dwLength = 40
    0x00,0x01,            // bcdVersion = 1.00
    0x04,0x00,            // wIndex = 0x0004
    0x01,                 // bCount = 1 function
    0x00,0x00,0x00,0x00,0x00,0x00,0x00, // reserved[7]

    // Function section (24 байта)
    0x00,                 // bFirstInterfaceNumber = 0  (if you have other IF# — change!)
    0x01,                 // reserved (allowed 0x01)
    'W','I','N','U','S','B',0x00,0x00,   // CompatibleID[8]
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // SubCompatibleID[8]
    0x00,0x00,0x00,0x00,0x00,0x00        // reserved[6]
};
const unsigned MS_OS_CompatID_Len = sizeof(MS_OS_CompatID);


// ------------------------- String descriptors -------------------------------
// LANGID (en-US)
const u8 Str0[] = { 4, 0x03, 0x09, 0x04 };

// "MyCompany" (UTF-16LE)
const u8 StrManuf[] = { 18, 0x03, 'M', 0, 'y', 0, 'C', 0, 'o', 0, 'm', 0, 'p',
		0, 'a', 0, 'n', 0, 'y', 0 };

// "MyDevice" (UTF-16LE)
const u8 StrProd[] = { 18, 0x03, 'M', 0, 'y', 0, 'D', 0, 'e', 0, 'v', 0, 'i', 0,
		'c', 0, 'e', 0 };

// "0001" (UTF-16LE)
const u8 StrSN[] = { 10, 0x03, '0', 0, '0', 0, '0', 0, '1', 0 };
