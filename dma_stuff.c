//dma_stuff.c
#include "xaxidma.h"
#include "dma_stuff.h"


static XAxiDma AxiDma;

/* ----------- SG mode ------------------ */

#define INTC_DEV_ID  XPAR_SCUGIC_0_DEVICE_ID
//#define DMA_IRQ_ID   XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID  // см. xparameters.h

#define BLK_BYTES    NBUF_BLK_BYTES			// one frame size
#define BD_COUNT     NBUF_COUNT              // two BD: for buf0 и buf1

// --- Memory for BD-ring (в DDR), by 64, aligned 64
static u8 BdSpace[BD_COUNT * BD_ALIGN] __attribute__((aligned(BD_ALIGN)));

static u8 buf0[BLK_BYTES] __attribute__((aligned(64)));
static u8 buf1[BLK_BYTES] __attribute__((aligned(64)));
static volatile u32 g_rx_done = 0;

// Глобально (для последующего опроса)
 XAxiDma_BdRing *g_RxRing = NULL;





int dma_sg_init_and_start(nbuffer_t *nb)
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

    //for each subbuffer
    for (int i = 0; i < (int)nb->nb; i++) {
            uint8_t *p = nbuf_ptr(nb, i);              // address i-part
            Xil_DCacheInvalidateRange((UINTPTR)p, BLK_BYTES);
            st  = XAxiDma_BdSetBufAddr(Bd, (UINTPTR)p);
            st |= XAxiDma_BdSetLength(Bd, BLK_BYTES, g_RxRing->MaxTransferLen);
            XAxiDma_BdSetId(Bd, (UINTPTR)i);  // Id for each BD..
            if (st != XST_SUCCESS) return st;

            Bd = XAxiDma_BdRingNext(g_RxRing, Bd);
        }


    // Отдаём BD железу
    Xil_DCacheFlushRange((UINTPTR)BdSpace, BD_COUNT * BD_ALIGN);
    //st = XAxiDma_BdRingToHw(g_RxRing, BD_COUNT, BdPtr); All buffs for HW
    const int PRIME_HW_BDS = 2;
    st = XAxiDma_BdRingToHw(g_RxRing, PRIME_HW_BDS, BdPtr);//  Priming only ... bufs

    if (st != XST_SUCCESS) return st;

    // Interrupt start
    XAxiDma_BdRingSetCoalesce(g_RxRing, 1, 0); /// 1 is when 1 buffer filled. 0 - is 0 delay timer
    XAxiDma_BdRingAckIrq(g_RxRing, XAXIDMA_IRQ_ALL_MASK);
    XAxiDma_BdRingIntEnable(g_RxRing, XAXIDMA_IRQ_IOC_MASK | XAXIDMA_IRQ_ERROR_MASK);

    // Стартуем приём
    st = XAxiDma_BdRingStart(g_RxRing);

    nb->rx_ring = g_RxRing; // Save g_RxRing for usb_bulk.c

    return st;
}



int dma_sg_setup(nbuffer_t *nb)
{
    int st;


    XAxiDma_Config *cfg = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (!cfg) return XST_FAILURE;

    st = XAxiDma_CfgInitialize(&AxiDma, cfg);
    if (st != XST_SUCCESS) return st;


    if (!XAxiDma_HasSg(&AxiDma)) return XST_FAILURE;

    g_RxRing = XAxiDma_GetRxRing(&AxiDma);
        nb->rx_ring = g_RxRing;

    return st;
}


int dma_sg_hard_restart(nbuffer_t *nb)
{
    int st;

    // 0) Глушим IRQ ринга
    XAxiDma_BdRingIntDisable(g_RxRing,
        XAXIDMA_IRQ_IOC_MASK | XAXIDMA_IRQ_ERROR_MASK);

    // 1) Аппаратный reset DMA
    XAxiDma_Reset(&AxiDma);
    while (!XAxiDma_ResetIsDone(&AxiDma)) { }

    // 2) Создаём кольцо поверх BdSpace
    st = XAxiDma_BdRingCreate(g_RxRing,
            (UINTPTR)BdSpace,
            (UINTPTR)BdSpace,
            BD_ALIGN,
            BD_COUNT);
    if (st != XST_SUCCESS) return st;

    // 3) Шаблон BD
    XAxiDma_Bd tpl;
    XAxiDma_BdClear(&tpl);
    st = XAxiDma_BdRingClone(g_RxRing, &tpl);
    if (st != XST_SUCCESS) return st;

    // 4) Аллоцируем ВСЕ BD и заправляем
    XAxiDma_Bd *bd_ptr;
    st = XAxiDma_BdRingAlloc(g_RxRing, BD_COUNT, &bd_ptr);
    if (st != XST_SUCCESS) return st;

    XAxiDma_Bd *bd = bd_ptr;
    for (int i = 0; i < (int)nb->nb; i++) {
        uint8_t *p = nbuf_ptr(nb, i);
        Xil_DCacheInvalidateRange((UINTPTR)p, BLK_BYTES);

        st  = XAxiDma_BdSetBufAddr(bd, (UINTPTR)p);
        st |= XAxiDma_BdSetLength(bd, BLK_BYTES,
                                  g_RxRing->MaxTransferLen);
        XAxiDma_BdSetId(bd, (UINTPTR)i);
        if (st != XST_SUCCESS) return st;

        bd = XAxiDma_BdRingNext(g_RxRing, bd);
    }

    Xil_DCacheFlushRange((UINTPTR)BdSpace, BD_COUNT * BD_ALIGN);

    // 5) Отдаём ВСЕ BD в HW
    st = XAxiDma_BdRingToHw(g_RxRing, BD_COUNT, bd_ptr);
    if (st != XST_SUCCESS) return st;

    // 6) IRQ + старт
    XAxiDma_BdRingSetCoalesce(g_RxRing, 1, 0);
    XAxiDma_BdRingAckIrq(g_RxRing, XAXIDMA_IRQ_ALL_MASK);
    XAxiDma_BdRingIntEnable(g_RxRing,
        XAXIDMA_IRQ_IOC_MASK | XAXIDMA_IRQ_ERROR_MASK);

    st = XAxiDma_BdRingStart(g_RxRing);
    if (st != XST_SUCCESS) return st;

    nb->rx_ring = g_RxRing;
    return XST_SUCCESS;
}





