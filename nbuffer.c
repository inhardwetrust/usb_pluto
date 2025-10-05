// nbuffer.c
#include "nbuffer.h"

void nbuf_init(nbuffer_t* nb) {
    nb->nb        = NBUF_COUNT;
    nb->blk_bytes = NBUF_BLK_BYTES;
    nb->prod_i    = 0;
    nb->cons_i    = 0;


    for (uint32_t i = 0; i < nb->nb; ++i) {
        nb->st[i]  = NBUF_FREE;
        // memset(nbuf_ptr(nb, i), 0, nb->blk_bytes); // Обнулить сами данные, необязательно
    }

    nb->rx_ring = NULL;
    nb->bd_idx= 0;
    nb->usb_tx_idx = -1;
}


int nbuf_can_dma(nbuffer_t* nb) {
    return (nb->st[nb->prod_i] == NBUF_FREE);
}

/* Выдать ptr под DMA и сразу обозначить, что блок занят DMA */
uint8_t* nbuf_dma_acquire(nbuffer_t* nb) {
    if (nb->st[nb->prod_i] != NBUF_FREE) return NULL;
    nb->st[nb->prod_i] = NBUF_DMA_BUSY;
    return nbuf_ptr(nb, nb->prod_i);
}


/* DMA закончился: текущий prod становится READY и prod_i сдвигается */
void nbuf_dma_done(nbuffer_t* nb) {
    int i = nb->prod_i;
    if (nb->st[i] == NBUF_FREE) {
        nb->st[i] = NBUF_READY;
        nb->prod_i = (i + 1) % (int)nb->nb;
    }
}

//// ------------- USB side -------------- ////
int nbuf_can_usb(nbuffer_t *nb) {
    return nb->st[nb->cons_i] == NBUF_READY;
}

uint8_t* nbuf_usb_acquire(nbuffer_t *nb) {
    if (nb->st[nb->cons_i] != NBUF_READY) return NULL;
    nb->st[nb->cons_i] = NBUF_USB_BUSY;
    return nbuf_ptr(nb, nb->cons_i);
}

void nbuf_usb_unacquire(nbuffer_t *nb) {
    nb->st[nb->cons_i] = NBUF_READY;
    return;
}

void nbuf_usb_done(nbuffer_t *nb) {
    int i = nb->cons_i;
    if (nb->st[i] == NBUF_USB_BUSY) {
        nb->st[i] = NBUF_FREE;
        nb->cons_i = (i + 1) % (int)nb->nb;
    }
}
