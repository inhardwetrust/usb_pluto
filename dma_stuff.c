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



// Глобалки для просмотра в отладчике
volatile u32 dbg_s2mm_cr   = 0;
volatile u32 dbg_s2mm_sr   = 0;
volatile u32 dbg_s2mm_cdesc= 0;
volatile u32 dbg_s2mm_tdesc= 0;
volatile u32 dbg_s2mm_idle = 0;
volatile u32 dbg_s2mm_halt = 0;
volatile u32 dbg_s2mm_err  = 0;

void dma_s2mm_capture_status(void)
{
    u32 base = AxiDma.RegBase;
    u32 off  = XAXIDMA_RX_OFFSET;

    dbg_s2mm_cr    = XAxiDma_ReadReg(base, off + XAXIDMA_CR_OFFSET);
    dbg_s2mm_sr    = XAxiDma_ReadReg(base, off + XAXIDMA_SR_OFFSET);
    dbg_s2mm_cdesc = XAxiDma_ReadReg(base, off + XAXIDMA_CDESC_OFFSET);
    dbg_s2mm_tdesc = XAxiDma_ReadReg(base, off + XAXIDMA_TDESC_OFFSET);

    dbg_s2mm_halt  = (dbg_s2mm_sr & 0x1u) ? 1u : 0u;       // Halted
    dbg_s2mm_idle  = (dbg_s2mm_sr & 0x2u) ? 1u : 0u;       // Idle
    dbg_s2mm_err   = (dbg_s2mm_sr & XAXIDMA_ERR_ALL_MASK); // Ошибки, если !=0
}

// DMASR bits (по даташиту AXI DMA)
#define DMASR_HALTED      0x00000001
#define DMASR_IDLE        0x00000002
#define DMASR_SGINCLD     0x00000008
#define DMASR_DMAINTERR   0x00000010
#define DMASR_DMASLVERR   0x00000020
#define DMASR_DMADECERR   0x00000040
#define DMASR_SGINTERR    0x00000080
#define DMASR_SGSLVERR    0x00000100
#define DMASR_SGDECERR    0x00000200
#define DMASR_IOC_IRQ     0x00001000
#define DMASR_DLY_IRQ     0x00002000
#define DMASR_ERR_IRQ     0x00004000

volatile u32 dbg_sr_raw=0, dbg_halted=0, dbg_idle=0, dbg_sg=0;
volatile u32 dbg_dma_int=0, dbg_dma_slv=0, dbg_dma_dec=0;
volatile u32 dbg_sg_int=0,  dbg_sg_slv=0,  dbg_sg_dec=0;
volatile u32 dbg_irq_ioc=0, dbg_irq_dly=0, dbg_irq_err=0;

void dma_s2mm_capture_sr(void)
{
    u32 sr = XAxiDma_ReadReg(AxiDma.RegBase,
                             XAXIDMA_RX_OFFSET + XAXIDMA_SR_OFFSET);

    dbg_sr_raw = sr;
    dbg_halted = (sr & DMASR_HALTED)   ? 1:0;
    dbg_idle   = (sr & DMASR_IDLE)     ? 1:0;
    dbg_sg     = (sr & DMASR_SGINCLD)  ? 1:0;

    dbg_dma_int= (sr & DMASR_DMAINTERR) ? 1:0;  // internal err (DMA datapath)
    dbg_dma_slv= (sr & DMASR_DMASLVERR) ? 1:0;  // SLVERR на M_AXI_S2MM
    dbg_dma_dec= (sr & DMASR_DMADECERR) ? 1:0;  // DECERR на M_AXI_S2MM

    dbg_sg_int = (sr & DMASR_SGINTERR)  ? 1:0;  // internal err (SG path)
    dbg_sg_slv = (sr & DMASR_SGSLVERR)  ? 1:0;  // SLVERR на M_AXI_SG
    dbg_sg_dec = (sr & DMASR_SGDECERR)  ? 1:0;  // DECERR на M_AXI_SG

    dbg_irq_ioc= (sr & DMASR_IOC_IRQ)   ? 1:0;
    dbg_irq_dly= (sr & DMASR_DLY_IRQ)   ? 1:0;
    dbg_irq_err= (sr & DMASR_ERR_IRQ)   ? 1:0;
}

static void s2mm_soft_reset(void) {
  u32 base = AxiDma.RegBase, off = XAXIDMA_RX_OFFSET;
  XAxiDma_WriteReg(base, off + XAXIDMA_CR_OFFSET, XAXIDMA_CR_RESET_MASK);
  while (XAxiDma_ReadReg(base, off + XAXIDMA_CR_OFFSET) & XAXIDMA_CR_RESET_MASK) {
	  int c=5;
  }
}



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
    dma_s2mm_capture_status();
    dma_s2mm_capture_sr();

    if (!XAxiDma_HasSg(&AxiDma)) return XST_FAILURE;

    //s2mm_soft_reset();

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
    dma_s2mm_capture_sr();


    dma_s2mm_capture_status();

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
    dma_s2mm_capture_status();

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
    dma_s2mm_capture_sr();
    dma_s2mm_capture_status();

    return st;
}


// опционально: дамп статуса канала (полезно при отладке)
static inline void dma_dump_s2mm_status(void) {
    u32 sr = XAxiDma_ReadReg(AxiDma.RegBase, XAXIDMA_RX_OFFSET + XAXIDMA_SR_OFFSET);
    xil_printf("S2MM DMASR=0x%08lx (Halted=%d Idle=%d Err=0x%lx)\r\n",
        (unsigned long)sr,
        (int)(sr & 1), (int)((sr >> 1) & 1),
        (unsigned long)(sr & XAXIDMA_ERR_ALL_MASK));
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


