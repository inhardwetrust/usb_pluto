
#include "xaxidma.h"
#include "dma_stuff.h"

static XAxiDma AxiDma;

int dma_init(void) {
    XAxiDma_Config *cfg = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (!cfg) return -1;
    if (XAxiDma_CfgInitialize(&AxiDma, cfg) != XST_SUCCESS) return -2;

    // Simple mode, без SG
    if (XAxiDma_HasSg(&AxiDma)) return -3;
    // включим прерывания при желании (понадобится ISR и подключенный s2mm_introut)
    // XAxiDma_IntrEnable(&AxiDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);

    return 0;
}

int dma_s2mm_start(void *dst_buf, size_t nbytes) {
    // invalidate cache на область, куда будет писать DMA
    Xil_DCacheInvalidateRange((UINTPTR)dst_buf, nbytes);

    // запустить приём в DDR
    int st = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)dst_buf, nbytes, XAXIDMA_DEVICE_TO_DMA);
    if (st != XST_SUCCESS) return -4;

    // ждать завершения (polling). С IRQ это не нужно.
    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA)) { /* spin */ }

    // область уже валидна (мы её invalid'или перед стартом)
    return 0;
}
