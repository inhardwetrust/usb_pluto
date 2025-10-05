// nbuffer.h
#pragma once
#include <stdint.h>
#include <stddef.h>

#include "xaxidma.h"

#define NBUF_COUNT 3u          // сколько блоков в пуле (тройная буферизация по умолчанию)
#define NBUF_BLK_BYTES 32768   // размер одного блока (кратно ширине AXI-Stream, TLAST на конце)

#ifndef CACHELINE_ALIGN
#define CACHELINE_ALIGN __attribute__((aligned(32)))
#endif

// Состояние каждого блока
typedef enum {
    NBUF_FREE = 0,     // свободен для приёма DMA
    NBUF_DMA_BUSY,     // DMA BD переписаны, может писать
    NBUF_READY,        // заполнен — готов к отправке по USB
    NBUF_USB_BUSY      // сейчас отправляется по USB
} nbuf_state_t;

// Главная структура «мульти-буфера»
typedef struct {
    // Конфигурация
    uint32_t nb;                 // Количество блоков (== NBUF_COUNT)
    uint32_t blk_bytes;          // размер одного блока (== NBUF_BLK_BYTES)

    // Индексы производитель/потребитель
    volatile int prod_i;         // куда следующий приём DMA положит данные
    volatile int cons_i;         // откуда следующий USB возьмёт данные

    // Таблица состояний каждого подбуфера
    volatile nbuf_state_t st[NBUF_COUNT];
    XAxiDma_BdRing *rx_ring;
    volatile int bd_idx ;
    volatile int usb_tx_idx;

    // Сам статический пул данных (в BSS/DDR)
    uint8_t data[NBUF_COUNT * NBUF_BLK_BYTES] CACHELINE_ALIGN;
} nbuffer_t;

// Утилита: получить указатель на i-й блок
static inline uint8_t* nbuf_ptr(nbuffer_t* nb, int i) {
    return &nb->data[(uint32_t)i * nb->blk_bytes];
}

//prototypes
void nbuf_init(nbuffer_t* nb);

int      nbuf_can_dma(nbuffer_t *nb);            // st[prod_i] == FREE ?
uint8_t* nbuf_dma_acquire(nbuffer_t *nb);        // mark DMA_BUSY, return ptr
void     nbuf_dma_done(nbuffer_t *nb);           // DMA_BUSY -> READY, prod_i++

int      nbuf_can_usb(nbuffer_t *nb);            // st[cons_i] == READY ?
uint8_t* nbuf_usb_acquire(nbuffer_t *nb);        // READY -> USB_BUSY, return ptr
void     nbuf_usb_done(nbuffer_t *nb);           // USB_BUSY -> FREE,  cons_i++

