//DMA

#include "nbuffer.h"

#define DMA_DEV_ID      XPAR_AXIDMA_0_DEVICE_ID   // из xparameters.h
#define BD_ALIGN     64             // for AXI DMA BD always 64 bytes


int dma_init(void);
void nbuf_fill_init(void);
int dma_s2mm_start(void *dst_buf, size_t nbytes);

int dma_sg_init_and_start(nbuffer_t *nb);
void dma_sg_poll_once(void);


