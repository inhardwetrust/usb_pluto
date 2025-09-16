//ringbuf.h
#pragma once
#include <stddef.h>
#include <stdint.h>



#define RBUF_SIZE 256

typedef struct ringbuf {
	size_t  wp;               // write pointer
	size_t  rp;
    uint8_t data[RBUF_SIZE];  // сам массив хранится внутри структуры
                 // read pointer
} ringbuf_t;

void init_ringbuf(ringbuf_t *rb);
 size_t ringbuf_wcount_contig(const ringbuf_t *rb);
 size_t ringbuf_rcount_contig(const ringbuf_t *rb);

size_t ringbuf_wspace_total(const ringbuf_t *rb);

 // Сдвиги указателей
 void ringbuf_advance_write(ringbuf_t *rb, size_t len);
 void ringbuf_advance_read (ringbuf_t *rb, size_t len);

