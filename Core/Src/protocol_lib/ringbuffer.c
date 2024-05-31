#include "ringbuffer.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define B_ERROR 1
#define B_SUCCESS 0
uint8_t ring_buf_init(ring_buf_t *r, uint32_t size)
{
	if (NULL != r->pHead)
		free(r->pHead);

	r->pHead = (uint8_t *)malloc(size * sizeof(uint8_t));
	if (r->pHead == NULL)
	{
		printf("pHead is null\n");
		return B_ERROR;
	}
	memset(r->pHead, 0, sizeof(size));

    r->pWrite = r->pHead;
    r->pRead = r->pHead;
    r->pTail = r->pHead + size;
    r->is_full = 0; // Initialize is_full to false

	return B_SUCCESS;
}

void ring_buf_free(ring_buf_t *r)
{
	if (NULL != r->pHead)
		free(r->pHead);
}

void ring_buf_clr(ring_buf_t *r)
{
	if (NULL == r->pHead) {
		printf("%s : RingBuff is not Init!\n", __func__);
	}
	r->pWrite = r->pHead;
	r->pRead = r->pHead;
    r->is_full = 0; // Clear the is_full flag
}

int32_t get_ring_buf_len(ring_buf_t *r)
{
	uint32_t len;

	if (NULL == r->pHead) {
		printf("%s : RingBuff is not Init!\n", __func__);
		return -1;
	}
    if (r->is_full) {
        return r->pTail - r->pHead; // If buffer is full, length is equal to size
    }
	if (r->pRead > r->pWrite) {
		len = r->pTail - r->pRead;
		len += r->pWrite - r->pHead;
	} else {
		len = r->pWrite - r->pRead;
	}
	return len;
}

int32_t get_ring_buf_free_space(ring_buf_t *r)
{
	uint32_t len;
	if (NULL == r->pHead) {
		printf("%s : RingBuff is not Init!\n", __func__);
		return -1;
	}
    if (r->is_full) {
        return 0; // If buffer is full, free space is 0
    }
	if (r->pRead > r->pWrite) {
		len = r->pRead - r->pWrite;
	} else {
		len = r->pTail - r->pWrite;
		len += r->pRead - r->pHead;
	}
	return len;
}

int32_t write_ring_buf(ring_buf_t *r, uint8_t *pBuff, uint32_t len)
{
	if (NULL == r->pHead) {
		printf("%s : RingBuff is not Init!\n", __func__);
		return -1;
	}
	if (len > r->pTail - r->pHead) {
		printf("%s :New add buff is too long\n", __func__);
		return -1;
	}
	if (len > get_ring_buf_free_space(r)) {
		printf("%s :New add buff is larger than the free area!\n", __func__);
		return -1;
	}
	if (r->pWrite + len > r->pTail) {
		int32_t pre_len = r->pTail - r->pWrite;
		int32_t last_len = len - pre_len;
		memcpy(r->pWrite, pBuff, pre_len);
		memcpy(r->pHead, pBuff + pre_len, last_len);
		r->pWrite = r->pHead + last_len;
	} else {
		memcpy(r->pWrite, pBuff, len);
		r->pWrite += len;
	}

	if (r->pWrite == r->pRead)
		r->is_full = 1; // Update is_full flag

	return len;
}

int32_t read_ring_buf(ring_buf_t *r, uint8_t *pBuff, uint32_t len)
{
	if (0 == len)
		return len;

	if (NULL == r->pHead) {
		printf("%s : RingBuff is not Init!\n", __func__);
		return -1;
	}

	if (len > r->pTail - r->pHead) {
		printf("%s :Read buff size is too long\n", __func__);
		return -1;
	}

	if (len > get_ring_buf_len(r)) {
		printf("%s :Read buff size is larger than the valid area!\n", __func__);
		return -1;
	}

	if (r->is_full)
		r->is_full = 0; // Clear is_full flag when reading

	if (r->pRead + len > r->pTail) {
		uint32_t pre_len = r->pTail - r->pRead;
		uint32_t last_len = len - pre_len;
		memcpy(pBuff, r->pRead, pre_len);
		memcpy(pBuff + pre_len, r->pHead, last_len);
		r->pRead = r->pHead + last_len;
	} else {
		memcpy(pBuff, r->pRead, len);
		r->pRead += len;
	}
	return len;
}

uint32_t ring_buf_check_get(ring_buf_t *r, uint8_t *pBuff, uint32_t len)
{
	if (0 == len)
		return B_SUCCESS;

	if (NULL == r->pHead) {
		printf("%s : RingBuff is not Init!\n", __func__);
		return -1;
	}

	if (len > r->pTail - r->pHead) {
		printf("%s :Read buff size is too long\n", __func__);
		return -1;
	}

	if (len > get_ring_buf_len(r)) {
		printf("%s :Read buff size is larger than the valid area!\n", __func__);
		return -1;
	}

	if (r->pRead + len > r->pTail) {
		uint32_t pre_len = r->pTail - r->pRead;
		uint32_t last_len = len - pre_len;
		memcpy(pBuff, r->pRead, pre_len);
		memcpy(pBuff + pre_len, r->pHead, last_len);
	} else {
		memcpy(pBuff, r->pRead, len);
	}
	return len;
}

int ring_buf_clr_len(ring_buf_t *r, uint32_t len)
{
	if (0 == len)
		return B_SUCCESS;

	if (NULL == r->pHead) {
		printf("%s : RingBuff is not Init!\n", __func__);
		return -1;
	}

	if (len > get_ring_buf_len(r)) {
		printf("%s : len is larger than the valid area and clean the buf!\n", __func__);
		r->pRead = r->pWrite = r->pHead;
	} else  if (r->pRead + len > r->pTail) {
		uint32_t last_len = len - (r->pTail - r->pRead);
		r->pRead = r->pHead + last_len;
	} else {
		r->pRead += len;
	}

	if (r->pRead == r->pWrite)
		r->is_full = 0; // Update is_full flag after clearing

	return 0;
}