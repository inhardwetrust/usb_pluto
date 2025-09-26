//usb_bulk.с
#include "usb_bulk.h"
#include "xil_types.h"    // UINTPTR, u8/u32
#include "xil_cache.h"    // Xil_DCacheFlushRange
#include <string.h>       // memset
#include "nbuffer.h"


#include "dma_stuff.h"
#include "xaxidma.h"

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
	//dma_init();

	dma_sg_setup(&nb);

	//dma_sg_init_and_start(&nb);



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

	//try_kick_tx();
	dma_sg_hard_restart(&nb);
}

/* Handle when transfered EP1 IN */
void Ep1_In_Handler(void *CallBackRef, u8 EpNum, u8 EventType, void *Data) {
	(void) EpNum;
	(void) CallBackRef;

	XGpioPs_WritePin(g_gpio, g_gpio_pin, 1);

	if (EventType == XUSBPS_EP_EVENT_DATA_TX) {

		// продолжаем возращать буфер в очередь

		XAxiDma_Bd *bd;
		if (XAxiDma_BdRingAlloc(nb.rx_ring, 1, &bd) != XST_SUCCESS) return;

		// готовим буфер для S2MM (DMA будет ПИСАТЬ сюда)
		uint8_t *p = nbuf_ptr(&nb, nb.bd_idx);
		Xil_DCacheInvalidateRange((UINTPTR)p, nb.blk_bytes);

		// настроить BD заново под этот же буфер
		int st = 0;
		st |= XAxiDma_BdSetBufAddr(bd, (UINTPTR)p);
		st |= XAxiDma_BdSetLength (bd, nb.blk_bytes, nb.rx_ring->MaxTransferLen);
		XAxiDma_BdSetId(bd, (UINTPTR)nb.bd_idx);
		if (st != XST_SUCCESS) { XAxiDma_BdRingUnAlloc(nb.rx_ring, 1, bd); return; }

		// (не обязательно, но ок) flush самой записи BD
		Xil_DCacheFlushRange((UINTPTR)bd, BD_ALIGN);

		// отдать BD железу
		if (XAxiDma_BdRingToHw(nb.rx_ring, 1, bd) == XST_SUCCESS) {
			nbuf_usb_done(&nb);
		} else {
		    XAxiDma_BdRingUnAlloc(nb.rx_ring, 1, bd);
		}




		//nbuf_usb_done(&nb);        // USB_BUSY -> FREE, cons_i++
		//try_kick_tx();
	}
}

/* =============  N-buffer and DMA  ============ */

void nbuf_fill(void) {
    while (nbuf_can_dma(&nb)) {
        uint8_t *dst = nbuf_dma_acquire(&nb);

        if (!dst) break;

        // Blocking simple DMA s2mm for whole size block
        dma_s2mm_start(dst, nb.blk_bytes);

        nbuf_dma_done(&nb);
    }
}


void dma_irq_handler_fp1(void *Ref) {
	//XGpioPs_WritePin(g_gpio, g_gpio_pin, 0);

	XAxiDma_BdRing *rx = nb.rx_ring;

	u32 s = XAxiDma_BdRingGetIrq(rx); // reading status register
	    if (!s) return;
	    XAxiDma_BdRingAckIrq(rx, s);  // reset all flags

	    if (s & XAXIDMA_IRQ_ERROR_MASK) {
	        return;
	    }

	    // fetch ONLY 1 ready BD
	    XAxiDma_Bd *bd_done = NULL;
	    int n = XAxiDma_BdRingFromHw(rx, 1, &bd_done);
	    if (n != 1) return;

	    nb.bd_idx= (int)XAxiDma_BdGetId(bd_done);
	    //int idx = (int)XAxiDma_BdGetId(bd_done);
	    UINTPTR addr = XAxiDma_BdGetBufAddr(bd_done);
	    Xil_DCacheInvalidateRange(addr, nb.blk_bytes);

	    nb.st[nb.bd_idx]      = NBUF_READY;
	    XAxiDma_BdRingFree(nb.rx_ring, 1, bd_done); // First part of task - return buffer to HQ que


	    try_kick_tx();
	    nb.st[nb.bd_idx]      = NBUF_USB_BUSY;






}


