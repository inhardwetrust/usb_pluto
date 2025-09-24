//nbuffer.c
#include "xaxidma.h"
#include "dma_stuff.h"
#include "nbuffer.h"

static XAxiDma AxiDma;

/* ----------- SG mode ------------------ */

#define INTC_DEV_ID  XPAR_SCUGIC_0_DEVICE_ID
//#define DMA_IRQ_ID   XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID  // см. xparameters.h

#define BLK_BYTES    (64)			// one frame size
#define BD_COUNT     2              // two BD: for buf0 и buf1
#define BD_ALIGN     64             // for AXI DMA BD always 64 bytes

// --- Memory for BD-ring (в DDR), by 64, aligned 64
static u8 BdSpace[BD_COUNT * BD_ALIGN] __attribute__((aligned(BD_ALIGN)));

static u8 buf0[BLK_BYTES] __attribute__((aligned(64)));
static u8 buf1[BLK_BYTES] __attribute__((aligned(64)));
static volatile u32 g_rx_done = 0;

// Глобально (для последующего опроса)
static XAxiDma_BdRing *g_RxRing = NULL;





int dma_sg_init_and_start(void)
{
    int st;


    XAxiDma_Config *cfg = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (!cfg) return XST_FAILURE;

    st = XAxiDma_CfgInitialize(&AxiDma, cfg);
    if (st != XST_SUCCESS) return st;

    XAxiDma_Reset(&AxiDma);
    while (!XAxiDma_ResetIsDone(&AxiDma)) {
    	 int g=5;
    }


    if (!XAxiDma_HasSg(&AxiDma)) return XST_FAILURE;


    // RX (S2MM) ring
    g_RxRing = XAxiDma_GetRxRing(&AxiDma);

    // Создаём ring поверх нашей памяти BdSpace
    st = XAxiDma_BdRingCreate(
            g_RxRing,
            (UINTPTR)BdSpace,          // физ/вирт начало (baremetal: одинаковы)
            (UINTPTR)BdSpace,
            BD_ALIGN,                  // 64 байта на BD
            BD_COUNT);
    if (st != XST_SUCCESS) return st;

    // Шаблон BD
    XAxiDma_Bd BdTemplate;
    XAxiDma_BdClear(&BdTemplate);
    st = XAxiDma_BdRingClone(g_RxRing, &BdTemplate);
    if (st != XST_SUCCESS) return st;

    // Первичная заправка: два BD на buf0 и buf1
    XAxiDma_Bd *BdPtr;
    st = XAxiDma_BdRingAlloc(g_RxRing, BD_COUNT, &BdPtr);
    if (st != XST_SUCCESS) return st;

    XAxiDma_Bd *Bd = BdPtr;

    Xil_DCacheInvalidateRange((UINTPTR)buf0, BLK_BYTES);
    st  = XAxiDma_BdSetBufAddr(Bd, (UINTPTR)buf0);
    st |= XAxiDma_BdSetLength(Bd, BLK_BYTES, g_RxRing->MaxTransferLen);
    //st |= XAxiDma_BdSetCtrl(Bd, XAXIDMA_BD_CTRL_IOC_MASK); // IOC можно оставить
    if (st != XST_SUCCESS) return st;

    Bd = XAxiDma_BdRingNext(g_RxRing, Bd);

    Xil_DCacheInvalidateRange((UINTPTR)buf1, BLK_BYTES);
    st  = XAxiDma_BdSetBufAddr(Bd, (UINTPTR)buf1);
    st |= XAxiDma_BdSetLength(Bd, BLK_BYTES, g_RxRing->MaxTransferLen);
    //st |= XAxiDma_BdSetCtrl(Bd, XAXIDMA_BD_CTRL_IOC_MASK);
    if (st != XST_SUCCESS) return st;

    // Отдаём BD железу
    Xil_DCacheFlushRange((UINTPTR)BdSpace, BD_COUNT * BD_ALIGN);
    st = XAxiDma_BdRingToHw(g_RxRing, BD_COUNT, BdPtr);
    if (st != XST_SUCCESS) return st;

    // ВНИМАНИЕ: прерывания НЕ включаем (polling)
    // XAxiDma_BdRingIntEnable(...);  // <-- удалено

    // Стартуем приём
    st = XAxiDma_BdRingStart(g_RxRing);

    return st;
}




// один «тик» поллинга (забрать готовые BD, инвалидация, ре-зарядка)
void dma_sg_poll_once(void)
{
    if (!g_RxRing) return;

    XAxiDma_Bd *bd_done;
    int n = XAxiDma_BdRingFromHw(g_RxRing, BD_COUNT, &bd_done);
    if (n <= 0) return;

    XAxiDma_Bd *bd = bd_done;
    for (int i = 0; i < n; i++) {

        // адрес буфера, куда DMA только что написал
        UINTPTR addr = XAxiDma_BdGetBufAddr(bd);

        // (если доступно в вашей версии) фактическая длина
        // u32 act = XAxiDma_BdGetActualLength(bd, g_RxRing->MaxTransferLen);

        // ВАЖНО: перед чтением CPU — обновить кэш, чтобы увидеть «свежее»
        Xil_DCacheInvalidateRange(addr, BLK_BYTES);

        // --- пример: показать первые 8 байт ---
        u8 *p = (u8 *)addr;


        // РЕ-ЗАРЯДКА этого же BD (чтобы поток не останавливался)
        // (invalidate перед подачей под S2MM уже сделан только что; можно ещё раз)
        Xil_DCacheInvalidateRange(addr, BLK_BYTES);
        XAxiDma_BdSetBufAddr(bd, addr);
        XAxiDma_BdSetLength(bd, BLK_BYTES, g_RxRing->MaxTransferLen);
        // Ctrl/IOC не обязателен в поллинге

        bd = XAxiDma_BdRingNext(g_RxRing, bd);
    }


    // вернуть обработанные BD обратно в железо
    XAxiDma_BdRingToHw(g_RxRing, n, bd_done);
}




/// Old dma
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
    int st = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)dst_buf, 64, XAXIDMA_DEVICE_TO_DMA);
    if (st != XST_SUCCESS) return -4;

    // ждать завершения (polling). С IRQ это не нужно.
    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA)) {
    int c=5;
     }

    // область уже валидна (мы её invalid'или перед стартом)
    return 0;
}


