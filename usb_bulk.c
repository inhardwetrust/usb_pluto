#include "usb_bulk.h"
#include "xil_types.h"    // UINTPTR, u8/u32
#include "xil_cache.h"    // Xil_DCacheFlushRange
#include <string.h>       // memset
#include "ringbuf.h"

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
ringbuf_t rb ALIGNMENT_CACHELINE;

void usb_bulk_set_instance(XUsbPs *inst) {
	g_inst = inst;
}

void usb_bulk_init(void) {
	init_ringbuf(&rb);
	dma_init();
}

static int try_kick_tx(void) {

	size_t avail = ringbuf_rcount_contig(&rb);
	if (avail == 0)
		return 1;

	size_t n = (avail > USB_CHUNK_MAX) ? USB_CHUNK_MAX : avail;

	uint8_t *ptr = &rb.data[rb.rp];

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

		ringbuf_advance_read(&rb, g_tx_len);
		try_kick_tx();
	}
}

/* =============  Ring buffer and DMA  ============ */

// fake DMA: fill part dst with len and val
static size_t rb_dma_write(uint8_t *dst, size_t len, uint8_t val) {
	memset(dst, val, len);
	return len;
}

size_t rb_write(uint8_t val) {
	size_t cont = ringbuf_wcount_contig(&rb);  // How much can write linear
	if (cont == 0)
		return 0;

	size_t n = (cont > DMA_CHUNK_BYTES) ? DMA_CHUNK_BYTES : cont;

	uint8_t *dst = &rb.data[rb.wp];
	//size_t wrote = rb_dma_write(dst, n, val);  // fake DMA writer
	size_t wrote = dma_s2mm_start(dst, n);


	ringbuf_advance_write(&rb, n);

	return wrote;
}
