
/**
 * @file xusbps_ch9.c
 *
 * This file contains the implementation of the chapter 9 code for the example.
 *
 ******************************************************************************/

/***************************** Include Files *********************************/


#include "xparameters.h"	/* XPAR parameters */
#include "xusbps.h"		/* USB controller driver */
#include "xusbps_hw.h"		/* USB controller driver */

#include "xusbps_ch9.h"
#include "xil_printf.h"
#include "xil_cache.h"



/*default class is storage class */
#include "usb_descriptors.h"
#include "sleep.h"

/* #define CH9_DEBUG */

#ifdef CH9_DEBUG
#include <stdio.h>
#define printf xil_printf
#endif

// temporary GPIO implement
#include "xgpiops.h"
extern XGpioPs Gpio;
#define LED_MIO        0

//bulk read

/************************** Constant Definitions *****************************/
/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

static void XUsbPs_StdDevReq(XUsbPs *InstancePtr,
			      XUsbPs_SetupData *SetupData);

static int XUsbPs_HandleVendorReq(XUsbPs *InstancePtr,
					XUsbPs_SetupData *SetupData);

/************************** Variable Definitions *****************************/

#ifdef __ICCARM__
#pragma data_alignment = 32
static u8 Response;
#pragma data_alignment = 4
#else
static u8 Response ALIGNMENT_CACHELINE;
#endif

/*****************************************************************************/
/**
* This function handles a Setup Data packet from the host.
*
* @param	InstancePtr is a pointer to XUsbPs instance of the controller.
* @param	SetupData is the structure containing the setup request.
*
* @return
*		- XST_SUCCESS if the function is successful.
*		- XST_FAILURE if an Error occured.
*
* @note		None.
*
******************************************************************************/
int XUsbPs_Ch9HandleSetupPacket(XUsbPs *InstancePtr,
				 XUsbPs_SetupData *SetupData)
{
	int Status = XST_SUCCESS;

#ifdef CH9_DEBUG
	printf("Handle setup packet\n");
#endif

	switch (SetupData->bmRequestType & XUSBPS_REQ_TYPE_MASK) {
	case XUSBPS_CMD_STDREQ:
		XUsbPs_StdDevReq(InstancePtr, SetupData);
		break;



	case XUSBPS_CMD_VENDREQ:

#ifdef CH9_DEBUG
		printf("vendor request %x\n", SetupData->bRequest);
#endif
		Status = XUsbPs_HandleVendorReq(InstancePtr, SetupData);
		break;

	default:
		/* Stall on Endpoint 0 */
#ifdef CH9_DEBUG
		printf("unknown class req, stall 0 in out\n");
#endif
		XUsbPs_EpStall(InstancePtr, 0, XUSBPS_EP_DIRECTION_IN |
						XUSBPS_EP_DIRECTION_OUT);
		break;
	}

	return Status;
}

/*****************************************************************************/
/**
* This function handles a standard device request.
*
* @param	InstancePtr is a pointer to XUsbPs instance of the controller.
* @param	SetupData is a pointer to the data structure containing the
*		setup request.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void XUsbPs_StdDevReq(XUsbPs *InstancePtr,
			      XUsbPs_SetupData *SetupData)
{
	int Status;
	int Error = 0;

	XUsbPs_Local	*UsbLocalPtr;

	int ReplyLen;
#ifdef __ICCARM__
#pragma data_alignment = 32
static u8  	Reply[XUSBPS_REQ_REPLY_LEN];
#pragma data_alignment = 4
#else
	static u8  	Reply[XUSBPS_REQ_REPLY_LEN] ALIGNMENT_CACHELINE;
#endif

	/* Check that the requested reply length is not bigger than our reply
	 * buffer. This should never happen...
	 */
	if (SetupData->wLength > XUSBPS_REQ_REPLY_LEN) {
		return;
	}

	UsbLocalPtr = (XUsbPs_Local *) InstancePtr->UserDataPtr;

#ifdef CH9_DEBUG
	printf("std dev req %d\n", SetupData->bRequest);
#endif

	switch (SetupData->bRequest) {

	case XUSBPS_REQ_GET_STATUS:

		switch(SetupData->bmRequestType & XUSBPS_STATUS_MASK) {
		case XUSBPS_STATUS_DEVICE:
			/* It seems we do not have to worry about zeroing out the rest
			 * of the reply buffer even though we are only using the first
			 * two bytes.
			 */
			*((u16 *) &Reply[0]) = 0x0100; /* Self powered */
			break;

		case XUSBPS_STATUS_INTERFACE:
			*((u16 *) &Reply[0]) = 0x0;
			break;

		case XUSBPS_STATUS_ENDPOINT:
			{
			u32 Status;
			int EpNum = SetupData->wIndex;

			Status = XUsbPs_ReadReg(InstancePtr->Config.BaseAddress,
					XUSBPS_EPCRn_OFFSET(EpNum & 0xF));

			if(EpNum & 0x80) { /* In EP */
				if(Status & XUSBPS_EPCR_TXS_MASK) {
					*((u16 *) &Reply[0]) = 0x0100;
				}else {
					*((u16 *) &Reply[0]) = 0x0000;
				}
			} else {	/* Out EP */
				if(Status & XUSBPS_EPCR_RXS_MASK) {
					*((u16 *) &Reply[0]) = 0x0100;
				}else {
					*((u16 *) &Reply[0]) = 0x0000;
				}
			}
			break;
			}

		default:
			;
#ifdef CH9_DEBUG
			printf("unknown request for status %x\n", SetupData->bmRequestType);
#endif
		}
		XUsbPs_EpBufferSend(InstancePtr, 0, Reply, SetupData->wLength);
		break;

	case XUSBPS_REQ_SET_ADDRESS:

		/* With bit 24 set the address value is held in a shadow
		 * register until the status phase is acked. At which point it
		 * address value is written into the address register.
		 */
		XUsbPs_SetDeviceAddress(InstancePtr, SetupData->wValue);
#ifdef CH9_DEBUG
		printf("Set address %d\n", SetupData->wValue);
#endif
		/* There is no data phase so ack the transaction by sending a
		 * zero length packet.
		 */
		XUsbPs_EpBufferSend(InstancePtr, 0, NULL, 0);
		break;

	case XUSBPS_REQ_GET_INTERFACE:
#ifdef CH9_DEBUG
		printf("Get interface %d/%d/%d\n",
			SetupData->wIndex, SetupData->wLength,
			InstancePtr->CurrentAltSetting);
#endif
		Response = (u8)InstancePtr->CurrentAltSetting;

		/* Ack the host */
		XUsbPs_EpBufferSend(InstancePtr, 0, &Response, 1);

		break;

	////
	case XUSBPS_REQ_GET_DESCRIPTOR: {
	    switch ((SetupData->wValue >> 8) & 0xFF) {

	    case XUSBPS_TYPE_DEVICE_DESC:
	        ReplyLen = XUsbPs_Ch9SetupDevDescReply(Reply, XUSBPS_REQ_REPLY_LEN);
	        if (ReplyLen > SetupData->wLength) ReplyLen = SetupData->wLength;
	        Status = XUsbPs_EpBufferSend(InstancePtr, 0, Reply, ReplyLen);
	        if (Status != XST_SUCCESS) for(;;);
	        break;

	    case XUSBPS_TYPE_CONFIG_DESC:
	        ReplyLen = XUsbPs_Ch9SetupCfgDescReply(Reply, XUSBPS_REQ_REPLY_LEN);
	        if (ReplyLen > SetupData->wLength) ReplyLen = SetupData->wLength;
	        Status = XUsbPs_EpBufferSend(InstancePtr, 0, Reply, ReplyLen);
	        if (Status != XST_SUCCESS) for(;;);
	        break;

	    case XUSBPS_TYPE_STRING_DESC:
	        ReplyLen = XUsbPs_Ch9SetupStrDescReply(Reply, XUSBPS_REQ_REPLY_LEN,
	                                               SetupData->wValue & 0xFF);
	        if (ReplyLen > SetupData->wLength) ReplyLen = SetupData->wLength;
	        Status = XUsbPs_EpBufferSend(InstancePtr, 0, Reply, ReplyLen);
	        if (Status != XST_SUCCESS) for(;;);
	        break;

	    case XUSBPS_TYPE_DEVICE_QUALIFIER:           // 0x06
	        ReplyLen = XUsbPs_Ch9SetupDevQualifierReply(Reply, XUSBPS_REQ_REPLY_LEN);
	        if (ReplyLen > SetupData->wLength) ReplyLen = SetupData->wLength;
	        Status = XUsbPs_EpBufferSend(InstancePtr, 0, Reply, ReplyLen);
	        if (Status != XST_SUCCESS) for(;;);
	        break;

	    case XUSBPS_TYPE_OTHER_SPEED_CFG_DESC:       // 0x07
	        ReplyLen = XUsbPs_Ch9SetupOtherSpeedCfgDescReply(Reply, XUSBPS_REQ_REPLY_LEN);
	        if (ReplyLen > SetupData->wLength) ReplyLen = SetupData->wLength;
	        Status = XUsbPs_EpBufferSend(InstancePtr, 0, Reply, ReplyLen);
	        if (Status != XST_SUCCESS) for(;;);
	        break;

	    default:
	        Error = 1;
	        break;
	    }
	    break;
	}

	case XUSBPS_REQ_SET_CONFIGURATION:

		/*
		 * Only allow configuration index 1 as this is the only one we
		 * have.
		 */
		if ((SetupData->wValue & 0xff) != 1) {
			Error = 1;
			break;
		}

		UsbLocalPtr->CurrentConfig = SetupData->wValue & 0xff;




		/* There is no data phase so ack the transaction by sending a
		 * zero length packet.
		 */
		XUsbPs_EpBufferSend(InstancePtr, 0, NULL, 0);
		break;


	case XUSBPS_REQ_GET_CONFIGURATION:
		Response = (u8)InstancePtr->CurrentAltSetting;
		XUsbPs_EpBufferSend(InstancePtr, 0,
					&Response, 1);
		break;


	case XUSBPS_REQ_CLEAR_FEATURE:
		switch(SetupData->bmRequestType & XUSBPS_STATUS_MASK) {
		case XUSBPS_STATUS_ENDPOINT:
			if(SetupData->wValue == XUSBPS_ENDPOINT_HALT) {
				int EpNum = SetupData->wIndex;

				if(EpNum & 0x80) {	/* In ep */
					XUsbPs_ClrBits(InstancePtr,
						XUSBPS_EPCRn_OFFSET(EpNum & 0xF),
						XUSBPS_EPCR_TXS_MASK);
				}else { /* Out ep */
					XUsbPs_ClrBits(InstancePtr,
						XUSBPS_EPCRn_OFFSET(EpNum),
						XUSBPS_EPCR_RXS_MASK);
				}
			}
			/* Ack the host ? */
			XUsbPs_EpBufferSend(InstancePtr, 0, NULL, 0);
			break;

		default:
			Error = 1;
			break;
		}

		break;

	case XUSBPS_REQ_SET_FEATURE:
		switch(SetupData->bmRequestType & XUSBPS_STATUS_MASK) {
		case XUSBPS_STATUS_ENDPOINT:
			if(SetupData->wValue == XUSBPS_ENDPOINT_HALT) {
				int EpNum = SetupData->wIndex;

				if(EpNum & 0x80) {	/* In ep */
					XUsbPs_SetBits(InstancePtr,
						XUSBPS_EPCRn_OFFSET(EpNum & 0xF),
						XUSBPS_EPCR_TXS_MASK);

				}else { /* Out ep */
					XUsbPs_SetBits(InstancePtr,
						XUSBPS_EPCRn_OFFSET(EpNum),
						XUSBPS_EPCR_RXS_MASK);
				}
			}
			/* Ack the host ? */
			XUsbPs_EpBufferSend(InstancePtr, 0, NULL, 0);

			break;
		case XUSBPS_STATUS_DEVICE:
			if (SetupData->wValue == XUSBPS_TEST_MODE) {
				int TestSel = (SetupData->wIndex >> 8) & 0xFF;

				/* Ack the host, the transition must happen
					after status stage and < 3ms */
				XUsbPs_EpBufferSend(InstancePtr, 0, NULL, 0);
				usleep(1000);

				switch (TestSel) {
				case XUSBPS_TEST_J:
				case XUSBPS_TEST_K:
				case XUSBPS_TEST_SE0_NAK:
				case XUSBPS_TEST_PACKET:
				case XUSBPS_TEST_FORCE_ENABLE:
					XUsbPs_SetBits(InstancePtr, \
						XUSBPS_PORTSCR1_OFFSET, \
				 		TestSel << 16);
					break;
				default:
					/* Unsupported test selector */
					break;
				}
				break;
			}

		default:
			Error = 1;
			break;
		}

		break;






	/* For set interface, check the alt setting host wants */
	case XUSBPS_REQ_SET_INTERFACE:

#ifdef CH9_DEBUG
		printf("set interface %d/%d\n", SetupData->wValue, SetupData->wIndex);
#endif
		/* Not supported */
		/* XUsbPs_SetInterface(InstancePtr, SetupData->wValue, SetupData->wIndex); */

		/* Ack the host after device finishes the operation */
		Error = XUsbPs_EpBufferSend(InstancePtr, 0, NULL, 0);
		if(Error) {
#ifdef CH9_DEBUG
			printf("EpBufferSend failed %d\n", Error);
#endif
		}
        break;

	default:
		Error = 1;
		break;
	}

	/* Set the send stall bit if there was an error */
	if (Error) {
#ifdef CH9_DEBUG
		printf("std dev req %d/%d error, stall 0 in out\n",
			SetupData->bRequest, (SetupData->wValue >> 8) & 0xff);
#endif
		XUsbPs_EpStall(InstancePtr, 0, XUSBPS_EP_DIRECTION_IN |
						XUSBPS_EP_DIRECTION_OUT);
	}
}



#define REQ_GET_FW_VERSION   0x31  // IN, Device
#define REQ_SET_LED          0x32  // OUT, Device
#define REQ_READ_REG         0x33  // IN,  Interface
#define REQ_WRITE_REG        0x34  // OUT, Interface

static int XUsbPs_HandleVendorReq(XUsbPs *InstancePtr, XUsbPs_SetupData *S)
{
    const u8 Ep0 = 0;
    const int isIn = (S->bmRequestType & 0x80) != 0;

    // 1) MS WCID
    if ((u8)S->bRequest == MS_OS_10_VENDOR_CODE) {
        if (!isIn) { XUsbPs_EpStall(InstancePtr, Ep0, XUSBPS_EP_DIRECTION_OUT); return XST_FAILURE; }
        if (S->wIndex == 0x0004) {
            int len = (int)MS_OS_CompatID_Len;
            if (len > (int)S->wLength) len = (int)S->wLength;
            return XUsbPs_EpBufferSend(InstancePtr, Ep0, (u8*)MS_OS_CompatID, (u32)len);
        }
        XUsbPs_EpStall(InstancePtr, Ep0, XUSBPS_EP_DIRECTION_IN);
        return XST_FAILURE;
    }

    // 2) My requests examples!!!
    switch ((u8)S->bRequest) {
//
//    case REQ_GET_FW_VERSION: {           // IN, return two byted
//        if (!isIn) { XUsbPs_EpStall(InstancePtr, Ep0, XUSBPS_EP_DIRECTION_OUT); return XST_FAILURE; }
//        u8 reply[4] = { 0x01, 0x00, 0x00, 0x00 }; // v1.0
//        u32 n = (S->wLength < sizeof(reply)) ? S->wLength : sizeof(reply);
//        return XUsbPs_EpBufferSend(InstancePtr, Ep0, reply, n);
//    }
//
    case REQ_SET_LED: {                  // OUT, 1 ... payload
        if (isIn || S->wLength == 0) {
            XUsbPs_EpStall(InstancePtr, Ep0, XUSBPS_EP_DIRECTION_IN);
            return XST_FAILURE;
        }

        // 1) Re-prime EP0 OUT ... data phase
        XUsbPs_EpPrime(InstancePtr, Ep0, XUSBPS_EP_DIRECTION_OUT);

        // 2)
        int Timeout = XUSBPS_TIMEOUT_COUNTER;   // . 1000000)
        u32 reg;
        do {
            reg = XUsbPs_ReadReg(InstancePtr->Config.BaseAddress, XUSBPS_EPPRIME_OFFSET);
        } while (((reg & (1u << Ep0)) != 0u) && --Timeout);
        if (!Timeout) return XST_FAILURE;

        // 3)  payload ...EP0 OUT (.... IRQ)
        u8  *buf; u32 len; u32 handle;
        Timeout = XUSBPS_TIMEOUT_COUNTER;
        int st;
        do {
            st = XUsbPs_EpBufferReceive(InstancePtr, Ep0, &buf, &len, &handle);
        } while ((st != XST_SUCCESS) && --Timeout);
        if (!Timeout) return XST_FAILURE;

        // 4)
        Xil_DCacheInvalidateRange((UINTPTR)buf, len);
        if (len >= 1) {
            //XGpioPs_WritePin(&Gpio, LED_MIO, (buf[0] & 1));  // 0/1 Ã�Â¸Ã�Â· payload
            // xil_printf("SET_LED: val=%d len=%lu\r\n", buf[0], (unsigned long)len);
        }

        // 5)  ZLP (status stage)
        XUsbPs_EpBufferRelease(handle);
        return XUsbPs_EpBufferSend(InstancePtr, Ep0, NULL, 0);
    }
//
//    case REQ_READ_REG: {                 // IN, address to wIndex/wValue
//        if (!isIn) { XUsbPs_EpStall(InstancePtr, Ep0, XUSBPS_EP_DIRECTION_OUT); return XST_FAILURE; }
//        u16 addr = S->wIndex; // example
//        u32 val  = /* read32(addr) */ 0xDEADBEEF;
//        u8  reply[4] = { (u8)val, (u8)(val>>8), (u8)(val>>16), (u8)(val>>24) };
//        u32 n = (S->wLength < 4) ? S->wLength : 4;
//        return XUsbPs_EpBufferSend(InstancePtr, Ep0, reply, n);
//    }
//
//    case REQ_WRITE_REG: {                // OUT, address to wIndex, data is to payload
//        if (isIn || S->wLength < 4) { XUsbPs_EpStall(InstancePtr, Ep0, XUSBPS_EP_DIRECTION_IN); return XST_FAILURE; }
//        u8 *buf; u32 len; u32 handle;
//        if (XUsbPs_EpBufferReceive(InstancePtr, Ep0, &buf, &len, &handle) != XST_SUCCESS) return XST_FAILURE;
//        u32 val = (u32)buf[0] | ((u32)buf[1]<<8) | ((u32)buf[2]<<16) | ((u32)buf[3]<<24);
//        u16 addr = S->wIndex;
//        // write32(addr, val);
//        XUsbPs_EpBufferRelease(handle);
//        return XUsbPs_EpBufferSend(InstancePtr, Ep0, NULL, 0);
//    }
//
    default:
        // Unknown vendor-request. STALL to request
        XUsbPs_EpStall(InstancePtr, Ep0, isIn ? XUSBPS_EP_DIRECTION_IN : XUSBPS_EP_DIRECTION_OUT);
        return XST_FAILURE;
    }
}

