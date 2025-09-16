//ringbuf.c
#include "ringbuf.h"
#include <stdlib.h>
#include <string.h>

void init_ringbuf(ringbuf_t *rb) {
    rb->wp   = 0;
    rb->rp   = 0;
    memset(rb->data, 0, RBUF_SIZE); // обнуляем, необязательно
}


// Сколько подряд можно ЗАПИСАТЬ (до конца массива, без wrap)
// В реализации без счётчика всегда держим 1 пустой байт для различения full/empty.
 size_t ringbuf_wcount_contig(const ringbuf_t *rb) {
    if (rb->wp >= rb->rp) {
        // место от wp до конца, но оставляем 1 байт, если rp==0
        size_t tail_space = RBUF_SIZE - rb->wp;
        if (rb->rp == 0) {
            if (tail_space == 0) return 0;
            return tail_space - 1; // оставляем одну ячейку
        }
        return tail_space;
    } else {
        // место от wp до rp, минус 1
        return (rb->rp - rb->wp) - 1;
    }
}

//Сколько можно читать линейно
 size_t ringbuf_rcount_contig(const ringbuf_t *rb) {
    if (rb->wp >= rb->rp) return rb->wp - rb->rp;
    else                  return RBUF_SIZE - rb->rp;
}

 size_t ringbuf_wspace_total(const ringbuf_t *rb) {
      if (rb->wp >= rb->rp) return RBUF_SIZE - (rb->wp - rb->rp) - 1;
      else                  return (rb->rp - rb->wp) - 1;
  }

// Линейная запись len байт (len <= ringbuf_wcount_contig(rb))
 void ringbuf_advance_write(ringbuf_t *rb, size_t len) {
    rb->wp = (rb->wp + len) % RBUF_SIZE;
}

// Линейное чтение len байт (len <= ringbuf_rcount_contig(rb))
 void ringbuf_advance_read(ringbuf_t *rb, size_t len) {
    rb->rp = (rb->rp + len) % RBUF_SIZE;
}

 // ----- Эмулятор DMA -----
 static size_t rb_dma_write(uint8_t *dst, size_t len, uint8_t val) {
     memset(dst, val, len);
     return len;
 }


