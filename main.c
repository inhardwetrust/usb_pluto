
#include <stdio.h>
#include "xgpiops.h"

//interrupts
#include "xscugic.h"

// needed for USB
#include "xparameters.h"
#include "xusbps.h"
#include "xil_exception.h"
#include "xil_cache.h"
#include "xusbps_ch9.h"
#include "usb_bulk.h"

/************************** Constant Definitions *****************************/

#define ALIGNMENT_CACHELINE  __attribute__ ((aligned (32)))

#define MEMORY_SIZE (64 * 1024)
#ifdef __ICCARM__
#pragma data_alignment = 32
u8 Buffer[MEMORY_SIZE];
#pragma data_alignment = 4
#else
u8 Buffer[MEMORY_SIZE] ALIGNMENT_CACHELINE;
#endif

/************************** Function Prototypes ******************************/

static int UsbIntrInit(XUsbPs *UsbInstancePtr, u16 UsbDeviceId, u16 UsbIntrId);

static void UsbIntrHandler(void *CallBackRef, u32 Mask);
static void XUsbPs_Ep0EventHandler(void *CallBackRef, u8 EpNum, u8 EventType,
		void *Data);
static void XUsbPs_Ep1EventHandler(void *CallBackRef, u8 EpNum, u8 EventType,
		void *Data);

static void UsbDisableIntrSystem(XScuGic *IntcInstancePtr, u16 UsbIntrId);

/************************** Variable Definitions  *****************************/

static XGpioPs Gpio; /* GPIO object */
static XScuGic Gic; /* Interrupt controller */

static XUsbPs UsbInstance; /* USB Controller */
static XUsbPs_Local UsbLocal;

static volatile int NumIrqs = 0;

#define LED_MIO        0 	//GPIO0 - debug pin on Pluto

#define GPIOPS0_DEV_ID		XPAR_XGPIOPS_0_DEVICE_ID
#define GIC_DEV_ID     		XPAR_SCUGIC_0_DEVICE_ID
#define FINTR_ID        	XPS_FPGA0_INT_ID

#define USB_DEV_ID          XPAR_XUSBPS_0_DEVICE_ID
#define USB_INTR_ID         XPAR_XUSBPS_0_INTR

int gpio_init(void) {
	XGpioPs_Config *GpioCfg;

	GpioCfg = XGpioPs_LookupConfig(GPIOPS0_DEV_ID);
	if (GpioCfg == NULL) {
		return XST_FAILURE;
	}

	if (XGpioPs_CfgInitialize(&Gpio, GpioCfg, GpioCfg->BaseAddr) != XST_SUCCESS) {
		return XST_FAILURE;
	}

	// LED_MIO Debug output
	XGpioPs_SetDirectionPin(&Gpio, LED_MIO, 1);
	XGpioPs_SetOutputEnablePin(&Gpio, LED_MIO, 1);
	XGpioPs_WritePin(&Gpio, LED_MIO, 1);   // Initial value

	return XST_SUCCESS;
}

int blink_led_toggle_20(void) {

	int val = XGpioPs_ReadPin(&Gpio, LED_MIO) ? 1 : 0;
	for (int i = 0; i < 20; ++i) {
		val ^= 1;
		XGpioPs_WritePin(&Gpio, LED_MIO, val);
		usleep(500000);
	}

	return XST_SUCCESS;
}

static int GicInitOnce(u16 GicDevId) {
	XScuGic_Config *Cfg = XScuGic_LookupConfig(GicDevId);
	if (!Cfg)
		return XST_FAILURE;

	int Status = XScuGic_CfgInitialize(&Gic, Cfg, Cfg->CpuBaseAddress);
	if (Status != XST_SUCCESS)
		return Status;

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
			(Xil_ExceptionHandler) XScuGic_InterruptHandler, &Gic);
	Xil_ExceptionEnableMask(XIL_EXCEPTION_IRQ);
	return XST_SUCCESS;
}

static void IrqHandler(void *Ref) {
	static int led = 0;
	led ^= 1;
	XGpioPs_WritePin(&Gpio, LED_MIO, led);
}

static int ConnectPlIrq(u32 FabricIntrId) {
	XScuGic_SetPriorityTriggerType(&Gic, FabricIntrId, 0xB0, 0x3); // 0x1 = level-high, 0x3 = rising-edge
	int Status = XScuGic_Connect(&Gic, FabricIntrId,
			(Xil_ExceptionHandler) IrqHandler, NULL);
	if (Status != XST_SUCCESS)
		return Status;
	XScuGic_Enable(&Gic, FabricIntrId);
	return XST_SUCCESS;
}

int main() {

	gpio_init();
	blink_led_toggle_20();

	GicInitOnce(GIC_DEV_ID); /* 1) Init GIC once */

	/* 2) Init USB controller + its IRQ */
	UsbLocal.CurrentConfig = 0;
	UsbInstance.UserDataPtr = &UsbLocal;
	UsbIntrInit(&UsbInstance, USB_DEV_ID, USB_INTR_ID);

	/* 3) Connect IRQ from PL to the same GIC */
	ConnectPlIrq(FINTR_ID);

	while (1) {
	}

}

/*****************************************************************************/

static int ConnectUsbIrq(XUsbPs *UsbInstancePtr, u16 UsbIntrId) {
	/* USBPS on Zynq usually level-high */
	XScuGic_SetPriorityTriggerType(&Gic, UsbIntrId, /*priority*/0xA0, /*level*/
			0x1);

	int Status = XScuGic_Connect(&Gic, UsbIntrId,
			(Xil_ExceptionHandler) XUsbPs_IntrHandler,  // ISR from USB driver
			(void*) UsbInstancePtr);
	if (Status != XST_SUCCESS)
		return Status;

	XScuGic_Enable(&Gic, UsbIntrId);
	return XST_SUCCESS;
}

static int UsbIntrInit(XUsbPs *UsbInstancePtr, u16 UsbDeviceId, u16 UsbIntrId) {
	int Status;
	u8 *MemPtr = NULL;
	int ReturnStatus = XST_FAILURE;

	const u8 NumEndpoints = 2;

	XUsbPs_Config *UsbConfigPtr;
	XUsbPs_DeviceConfig DeviceConfig;

	UsbConfigPtr = XUsbPs_LookupConfig(UsbDeviceId);
	if (NULL == UsbConfigPtr) {
		goto out;
	}

	Status = XUsbPs_CfgInitialize(UsbInstancePtr, UsbConfigPtr,
			UsbConfigPtr->BaseAddress);
	if (XST_SUCCESS != Status) {
		goto out;
	}

	Status = ConnectUsbIrq(UsbInstancePtr, UsbIntrId);

	if (XST_SUCCESS != Status) {
		goto out;
	}

	DeviceConfig.EpCfg[0].Out.Type = XUSBPS_EP_TYPE_CONTROL;
	DeviceConfig.EpCfg[0].Out.NumBufs = 2;
	DeviceConfig.EpCfg[0].Out.BufSize = 64;
	DeviceConfig.EpCfg[0].Out.MaxPacketSize = 64;
	DeviceConfig.EpCfg[0].In.Type = XUSBPS_EP_TYPE_CONTROL;
	DeviceConfig.EpCfg[0].In.NumBufs = 2;
	DeviceConfig.EpCfg[0].In.MaxPacketSize = 64;

	DeviceConfig.EpCfg[1].Out.Type = XUSBPS_EP_TYPE_BULK;
	DeviceConfig.EpCfg[1].Out.NumBufs = 16;
	DeviceConfig.EpCfg[1].Out.BufSize = 512;
	DeviceConfig.EpCfg[1].Out.MaxPacketSize = 512;
	DeviceConfig.EpCfg[1].In.Type = XUSBPS_EP_TYPE_BULK;
	DeviceConfig.EpCfg[1].In.NumBufs = 16;
	DeviceConfig.EpCfg[1].In.MaxPacketSize = 512;

	DeviceConfig.NumEndpoints = NumEndpoints;

	MemPtr = (u8 *) &Buffer[0];
	memset(MemPtr, 0, MEMORY_SIZE);
	Xil_DCacheFlushRange((unsigned int) MemPtr, MEMORY_SIZE);

	DeviceConfig.DMAMemPhys = (u32) MemPtr;

	Status = XUsbPs_ConfigureDevice(UsbInstancePtr, &DeviceConfig);
	if (XST_SUCCESS != Status) {
		goto out;
	}

	Status = XUsbPs_IntrSetHandler(UsbInstancePtr, UsbIntrHandler, NULL,
	XUSBPS_IXR_UE_MASK);
	if (XST_SUCCESS != Status) {
		goto out;
	}

	Status = XUsbPs_EpSetHandler(UsbInstancePtr, 0,
	XUSBPS_EP_DIRECTION_OUT, XUsbPs_Ep0EventHandler, UsbInstancePtr);

	Status = XUsbPs_EpSetHandler(UsbInstancePtr, 1,
	XUSBPS_EP_DIRECTION_OUT, XUsbPs_Ep1EventHandler, UsbInstancePtr);

	XUsbPs_IntrEnable(UsbInstancePtr, XUSBPS_IXR_UR_MASK | XUSBPS_IXR_UI_MASK);

	XUsbPs_Start(UsbInstancePtr);

	ReturnStatus = XST_SUCCESS;

	out:

	return ReturnStatus;
}

/*****************************************************************************/
static void UsbIntrHandler(void *CallBackRef, u32 Mask) {
	NumIrqs++;
}

/*****************************************************************************/
static void XUsbPs_Ep0EventHandler(void *CallBackRef, u8 EpNum, u8 EventType,
		void *Data) {
	XUsbPs *InstancePtr;
	int Status;
	XUsbPs_SetupData SetupData;
	u8 *BufferPtr;
	u32 BufferLen;
	u32 Handle;

	Xil_AssertVoid(NULL != CallBackRef);
	InstancePtr = (XUsbPs *) CallBackRef;

	switch (EventType) {
	case XUSBPS_EP_EVENT_SETUP_DATA_RECEIVED:
		Status = XUsbPs_EpGetSetupData(InstancePtr, EpNum, &SetupData);
		if (XST_SUCCESS == Status) {
			(int) XUsbPs_Ch9HandleSetupPacket(InstancePtr, &SetupData);
		}
		break;

	case XUSBPS_EP_EVENT_DATA_RX:
		Status = XUsbPs_EpBufferReceive(InstancePtr, EpNum, &BufferPtr,
				&BufferLen, &Handle);
		if (XST_SUCCESS == Status) {
			XUsbPs_EpBufferRelease(Handle);
		}
		break;

	default:
		/* Unhandled event. Ignore. */
		break;
	}
}

/*****************************************************************************/
static void XUsbPs_Ep1EventHandler(void *CallBackRef, u8 EpNum, u8 EventType,
		void *Data) {
	XUsbPs *InstancePtr;
	int Status;
	u8 *BufferPtr;
	u32 BufferLen;
	u32 Handle;

	Xil_AssertVoid(NULL != CallBackRef);
	InstancePtr = (XUsbPs *) CallBackRef;

	switch (EventType) {
	case XUSBPS_EP_EVENT_DATA_RX: {
		Status = XUsbPs_EpBufferReceive(InstancePtr, EpNum, &BufferPtr,
				&BufferLen, &Handle);
		if (XST_SUCCESS == Status) {

			/* Cache line on Cortex is 32 */
			u32 invLen = (BufferLen + 31u) & ~31u;
			Xil_DCacheInvalidateRange((UINTPTR) BufferPtr, invLen);

			usb_bulk_init(InstancePtr, 1, 1, 512);

			XUsbPs_EpBufferRelease(Handle);
		}
		break;
	}

	default:
		/* Unhandled event. Ignore. */
		break;
	}
}


/*****************************************************************************/
static void UsbDisableIntrSystem(XScuGic *IntcInstancePtr, u16 UsbIntrId) {
	/* Disconnect and disable the interrupt for the USB controller. */
	XScuGic_Disconnect(IntcInstancePtr, UsbIntrId);
}
