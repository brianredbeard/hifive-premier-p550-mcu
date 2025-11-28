#include "protocol.h"
#include <stdio.h>

uint8_t es_frame_init(b_frame_class_t *pframe, b_frame_t *pframeinit)
{
	uint8_t err = 0;
	if ((!pframeinit) || (!pframe)) {
		return B_ERROR;
	}

	pframe->frame_info.pname = pframeinit->pname;
	pframe->frame_info.head = pframeinit->head;
	pframe->frame_info.head_len = pframeinit->head_len;
	pframe->frame_info.end = pframeinit->end;
	pframe->frame_info.end_len = pframeinit->end_len;

	err = ring_buf_init(&pframe->_frame_ring, pframe->_in_frame_buffer_size);
	if (!err)
		return B_SUCCESS;
	return B_ERROR;
}

uint8_t es_frame_put(b_frame_class_t *pframe, uint8_t *dat, uint32_t len)
{
	if (!dat || len == 0) {
		return B_ERROR;
	}
	uint32_t putlen = write_ring_buf(&pframe->_frame_ring, dat, len);

	if (putlen != len) {
		ring_buf_clr(&pframe->_frame_ring);
		// printf("%s-err:The ringbuffer is full putlen %x\r\n", pframe->frame_info.pname, putlen);
		return B_ERROR;
	}
	return B_SUCCESS;
}

void es_frame_fifo_clear(b_frame_class_t *pframe)
{
	if (!pframe) {
		// printf("%s-err:pframe is null\r\n", pframe->frame_info.pname);
		return;
	}
	ring_buf_clr(&pframe->_frame_ring);
}

uint8_t es_check_head(b_frame_class_t *pframe)
{
	uint8_t i = 0;
	uint8_t tmp;
	int32_t len = get_ring_buf_len(&pframe->_frame_ring);
	if (len < 0 || (len < pframe->frame_info.head_len + pframe->frame_info.end_len + 1))
		return B_ERROR;

	if (pframe->frame_info.head_len == 0)
		return B_SUCCESS;

	do {
		read_ring_buf(&pframe->_frame_ring, &tmp, 1);
		if (tmp == pframe->frame_info.head[i]) {
			if (++i == pframe->frame_info.head_len) {
				return B_SUCCESS;
			}
		} else {
			break;
		}
	} while (1);
	// printf("%s %d B_ERROR\n", __func__, __LINE__);

	return B_ERROR;
}

uint8_t es_get_cmd(b_frame_class_t *pframe)
{
	uint8_t cmd, ret = B_ERROR;
	if (read_ring_buf(&pframe->_frame_ring, &cmd, 1) == 1) {
		// printf("cmd %x\n", cmd);
		if (CMD_EEPROM_WP <= cmd && CMD_RES >= cmd) {
			pframe->frame.cmd = cmd;
			ret = B_SUCCESS;
		}
	}
	// printf("%s %d, ret %d\n", __func__, __LINE__, ret);
	return ret;
}

uint8_t es_check_frame(b_frame_class_t *pframe)
{
	uint8_t len, xor;
	uint8_t buf[MAX_FRAME_LEN];
	uint8_t current_xor = 0;

	// printf("%s %d\n", __func__, __LINE__);
	if (read_ring_buf(&pframe->_frame_ring, &len, 1) != 1)
		return B_ERROR;

	// printf("%s %d\n", __func__, __LINE__);
	memset(buf, 0, MAX_FRAME_LEN * sizeof(uint8_t));
	// printf("%s %d\n", __func__, __LINE__);
	if (read_ring_buf(&pframe->_frame_ring, buf, len) != len)
		return B_ERROR;
	pframe->frame.len = len;
	for (int i = 0; i < len; i++) {
		current_xor ^= buf[i];
		printf("buf[%d] %x\n", i, buf[i]);
	}

	if (read_ring_buf(&pframe->_frame_ring, &xor, 1) != 1)
		return B_ERROR;
	if (current_xor == xor) {
		memset(&pframe->frame.data.value, 0, sizeof(pframe->frame.data.value));
		memcpy(&pframe->frame.data.value, buf, len);
		return B_SUCCESS;
	}
	return B_ERROR;
}

uint8_t es_get_cmd_and_data(b_frame_class_t *pframe)
{
	if (es_get_cmd(pframe) != B_SUCCESS)
		return B_ERROR;
	if (es_check_frame(pframe) != B_SUCCESS)
		return B_ERROR;
	return B_SUCCESS;
}

uint8_t es_check_tail(b_frame_class_t *pframe)
{
	uint8_t i = 0;
	uint8_t tmp;
	uint16_t len = get_ring_buf_len(&pframe->_frame_ring);

	if (len < pframe->frame_info.end_len) {
		// printf("%s %d B_ERROR\n", __func__, __LINE__);
		return B_ERROR;
	}

	if (pframe->frame_info.end_len == 0) {
		// printf("%s %d B_SUCCESS\n", __func__, __LINE__);
		return B_SUCCESS;
	}

	do {
		ring_buf_check_get(&pframe->_frame_ring, &tmp, 1);
		// printf("tmp(%d) %x end %x\n", i, tmp, pframe->frame_info.end[i]);
		if (tmp == pframe->frame_info.end[i]) {
			ring_buf_clr_len(&pframe->_frame_ring, 1);
			if (++i == pframe->frame_info.end_len) {
				return B_SUCCESS;
			}
		} else {
			break;
		}
	} while (1);

	return B_ERROR;
}
