//usb_bulk.с
#include "usb_bulk.h"
#include "xil_types.h"    // UINTPTR, u8/u32
#include "xil_cache.h"    // Xil_DCacheFlushRange
#include <string.h>       // memset
#include "nbuffer.h"


#include "dma_stuff.h"

#ifndef ALIGNMENT_CACHELINE
#define ALIGNMENT_CACHELINE __attribute__((aligned(32)))
#endif

/* === GPIO context === */
static XGpioPs *g_gpio = NULL;
static uint32_t g_gpio_pin = 0xFFFFFFFF;   // invalid pin -> GPIO не активен

void usb_bulk_set_gpio(XGpioPs *gpio, uint32_t pin) {
	g_gpio = gpio;
	g_gpio_pin = pin;
}

/* Local pointer to USB, used in main via usb_bulk_set_instance() */
static XUsbPs *g_inst = NULL;

static size_t g_tx_len = 0;

//n-buffer create
nbuffer_t nb ALIGNMENT_CACHELINE;

void usb_bulk_set_instance(XUsbPs *inst) {
	g_inst = inst;
}

void usb_bulk_init(void) {
	nbuf_init(&nb);
	dma_init();
	nbuf_fill_init();

}

static int try_kick_tx(void) {

	if (!nbuf_can_usb(&nb))        // no more ready blocks
	        return 1;

	size_t n = nb.blk_bytes;

	uint8_t *ptr = nbuf_usb_acquire(&nb);   // READY -> USB_BUSY
	    if (!ptr) return 1;

	Xil_DCacheFlushRange((UINTPTR) ptr, n); //
	int st = XUsbPs_EpBufferSend(g_inst, /*ep=*/1, ptr, n);
	if (st == XST_SUCCESS) {
		g_tx_len = n; /// how much we loaded for sending
		XGpioPs_WritePin(g_gpio, g_gpio_pin, 0);

	}
	return st;
}

void usb_stream_start(void) {

	try_kick_tx();
}

/* Handle when transfered EP1 IN */
void Ep1_In_Handler(void *CallBackRef, u8 EpNum, u8 EventType, void *Data) {
	(void) EpNum;
	(void) CallBackRef;

	XGpioPs_WritePin(g_gpio, g_gpio_pin, 1);

	if (EventType == XUSBPS_EP_EVENT_DATA_TX) {
		nbuf_usb_done(&nb);        // USB_BUSY -> FREE, cons_i++
		try_kick_tx();
	}
}

/* =============  Ring buffer and DMA  ============ */

void nbuf_fill_init(void) {
    while (nbuf_can_dma(&nb)) {
        uint8_t *dst = nbuf_dma_acquire(&nb);

        if (!dst) break;

        // Blocking simple DMA s2mm for whole size block
        dma_s2mm_start(dst, nb.blk_bytes);

        nbuf_dma_done(&nb);
    }
}





