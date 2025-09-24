//DMA
#define DMA_DEV_ID      XPAR_AXIDMA_0_DEVICE_ID   // из xparameters.h

int dma_init(void);
void nbuf_fill_init(void);
int dma_s2mm_start(void *dst_buf, size_t nbytes);

int dma_sg_init_and_start(void);
void dma_sg_poll_once(void);
