#ifndef _RING_BUF_H_
#define _RING_BUF_H_

#include <stdint.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *pHead;
    uint8_t *pTail;
    uint8_t *pWrite;
    uint8_t *pRead;
    uint8_t is_full; // New member to indicate if the ring buffer is full
}ring_buf_t;

uint8_t ring_buf_init(ring_buf_t *r, uint32_t size);
void ring_buf_free(ring_buf_t *r);
void ring_buf_clr(ring_buf_t *r);
int32_t get_ring_buf_len(ring_buf_t *r);
int32_t get_ring_buf_free_space(ring_buf_t *r);
int32_t write_ring_buf(ring_buf_t *r, uint8_t *pBuff, uint32_t len);
int32_t read_ring_buf(ring_buf_t *r, uint8_t *pBuff, uint32_t len);
uint32_t ring_buf_check_get(ring_buf_t *r, uint8_t *buf, uint32_t len);
uint32_t ring_buf_clr_len(ring_buf_t *r, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif
