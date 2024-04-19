#include "protocol.h"

#include <stdint.h>
#include <stdio.h>

#include "hf_common.h"
#include "main.h"
#include "ringbuffer.h"

#define B_ERROR 1
#define B_SUCCESS 0

extern DMA_HandleTypeDef hdma_usart3_rx;
#include "stm32f4xx.h"
uint32_t RxBuf_SIZE = 64;
uint8_t RxBuf[96];
b_frame_class_t frame_uart3;

#define MAX_FRAME_LEN 32

typedef enum protocol_status_type {
	CHECK_HEAD = 0,
	CHECK_CMD = 1,
	CHECK_FRAME_LEN = 2,
	CHECK_DATA = 3,
	CHECK_XOR = 4,
	CHECK_TAIL = 5,
	PROCESS_CMD = 6
};
typedef enum protocol_cmd_type { CMD_EEPROM_WP = 0x90, CMD_GPIO = 0x91, CMD_SET_IP = 0x92, CMD_RES = 0xb0 };

#define head_meg "\xA5\x5A\xAA\x55"
#define end_msg "\x0D\x0A\x0D\x0A"

uint8_t es_frame_init(b_frame_class_t *pframe, b_frame_t *pframeinit)
{
	uint8_t err = 0;
	if ((!pframeinit) || (!pframe)) {
		// printf("\n%s-err:frame parameter error, parameter is null\r\n", pframe->frame_info.pname);
		return B_ERROR;
	}

	pframe->frame_info.pname = pframeinit->pname;
	pframe->frame_info.head = pframeinit->head;
	pframe->frame_info.head_len = pframeinit->head_len;
	pframe->frame_info.end = pframeinit->end;
	pframe->frame_info.end_len = pframeinit->end_len;
	// printf("rin_addr_t %p size %d\n", &pframe->_frame_ring, pframe->_in_frame_buffer_size);
	// printf("pname %s head_len %d end_len %d\n", pframe->frame_info.pname, pframe->frame_info.head_len, \
		   pframe->frame_info.end_len);

	err = ring_buf_init(&pframe->_frame_ring, pframe->_in_frame_buffer_size);
	// printf("\nrhead %p tail %p write %p, read %p\n", pframe->_frame_ring.pHead, pframe->_frame_ring.pTail, \
		   pframe->_frame_ring.pWrite, pframe->_frame_ring.pRead);
	if (!err)
		return B_SUCCESS;
	// printf("\n%s-err:ring_buf_init err ring_buf init err\r\n", pframe->frame_info.pname);
	return B_ERROR;
}

uint8_t es_frame_put(b_frame_class_t *pframe, uint8_t *dat, uint32_t len)
{
	if (!dat || len == 0) {
		return B_ERROR;
	}
	uint8_t putlen = write_ring_buf(&pframe->_frame_ring, dat, len);

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
			// printf("tmp(%d) %x \n", i, tmp);
			if (++i == pframe->frame_info.head_len) {
				// printf("%s %d B_SUCCESS\n", __func__, __LINE__);
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
		if (CMD_EEPROM_WP <= cmd && CMD_SET_IP >= cmd) {
			pframe->frame.cmd = cmd;
			ret = B_SUCCESS;
		}
	}
	// printf("%s %d, ret %d\n", __func__, __LINE__, ret);
	return ret;
}

uint8_t es_check_frame(b_frame_class_t *pframe)
{
	uint8_t len, xor, ret = B_ERROR;
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
	for (int i = 0; i < len; i++) {
		current_xor ^= buf[i];
		// printf("buf[%d] %x\n", i, buf[i]);
	}

	// printf("%s %d\n", __func__, __LINE__);
	if (read_ring_buf(&pframe->_frame_ring, &xor, 1) != 1)
		return B_ERROR;
	// printf("%s %d\n", __func__, __LINE__);
	if (current_xor == xor) {
		// printf("%s %d\n", __func__, __LINE__);
		memset(&pframe->frame.data.value, 0, sizeof(pframe->frame.data.value));
		memcpy(&pframe->frame.data.value, buf, len);
		return B_SUCCESS;
	}
	// printf("%s %d\n", __func__, __LINE__);
	return B_ERROR;
}

uint8_t es_get_cmd_and_data(b_frame_class_t *pframe)
{
	// printf("%s %d\n", __func__, __LINE__);
	if (es_get_cmd(pframe) != B_SUCCESS)
		return B_ERROR;
	// printf("%s %d\n", __func__, __LINE__);
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
#include "cmsis_os2.h"
extern osThreadId_t http_task_handle;
extern osThreadAttr_t http_task_attributes;
extern uint8_t IP_ADDRESS[4];
#include "main.h"
#include "stm32f4xx_hal.h"
#define req_ok "\x6B\x6F"
#define req_fail "\x6C\x69\x61\x66"

void es_send_req(b_frame_class_t *pframe, uint8_t true)
{
	uint8_t buf[MAX_FRAME_LEN] = {0};
	uint8_t len, b_len, xor = 0;
	uint8_t req_cmd = CMD_RES;
	uint8_t *cmd_dat;
	if (true) {
		len = sizeof(req_ok) - 1;
		cmd_dat = req_ok;
	} else {
		cmd_dat = req_fail;
		len = sizeof(req_fail) - 1;
	}
	for (int i = 0; i < len; i++) {
		xor ^= cmd_dat[i];
	}
	memcpy(buf, pframe->frame_info.head, pframe->frame_info.head_len);
	b_len = pframe->frame_info.head_len;

	memcpy(buf + b_len, &req_cmd, 1);
	b_len += 1;

	memcpy(buf + b_len, &len, 1);
	b_len += 1;
	memcpy(buf + b_len, cmd_dat, len);
	b_len += len;

	memcpy(buf + b_len, &xor, 1);
	b_len += 1;

	memcpy(buf + b_len, pframe->frame_info.end, pframe->frame_info.end_len);
	b_len += pframe->frame_info.end_len;
	HAL_UART_Transmit(&huart3, buf, b_len, HAL_MAX_DELAY); // transmit the full sentence again
}

uint8_t es_process_cmd(b_frame_class_t *pframe)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_TypeDef *gpio_group;
	uint8_t cmd = pframe->frame.cmd;
	// printf("%s %d cmd %x\n", __func__, __LINE__, cmd);
	switch (cmd) {
	case CMD_EEPROM_WP:
		es_eeprom_wp(pframe->frame.data.value);
		es_send_req(pframe, 1);
		break;
	case CMD_GPIO:
		if (pframe->frame.data.gpio.group == 0)
			gpio_group = GPIOA;
		else if (pframe->frame.data.gpio.group == 1)
			gpio_group = GPIOB;
		else if (pframe->frame.data.gpio.group == 2)
			gpio_group = GPIOC;
		else if (pframe->frame.data.gpio.group == 3)
			gpio_group = GPIOD;
		else if (pframe->frame.data.gpio.group == 4)
			gpio_group = GPIOE;
		else if (pframe->frame.data.gpio.group == 5)
			gpio_group = GPIOF;
		else if (pframe->frame.data.gpio.group == 6)
			gpio_group = GPIOG;
		else if (pframe->frame.data.gpio.group == 7)
			gpio_group = GPIOH;
		else if (pframe->frame.data.gpio.group == 8)
			gpio_group = GPIOI;
		else
			es_send_req(pframe, 0);
		// printf("%s %d\n", __func__, __LINE__);

		HAL_GPIO_WritePin(gpio_group, pframe->frame.data.gpio.pin_num, pframe->frame.data.gpio.value);

		GPIO_InitStruct.Pin = pframe->frame.data.gpio.pin_num;
		GPIO_InitStruct.Mode = pframe->frame.data.gpio.driection;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(gpio_group, &GPIO_InitStruct);
		es_send_req(pframe, 1);
		break;
	case CMD_SET_IP:
		if (http_task_handle)
			osThreadTerminate(http_task_handle);
		IP_ADDRESS[0] = pframe->frame.data.ip.ip_addr0;
		IP_ADDRESS[1] = pframe->frame.data.ip.ip_addr1;
		IP_ADDRESS[2] = pframe->frame.data.ip.ip_addr2;
		IP_ADDRESS[3] = pframe->frame.data.ip.ip_addr3;
		// printf("IP_ADDR0:%d IP_ADDR1: %d IP_ADDR2: %d IP_ADDR3 %d\n", IP_ADDRESS[0], IP_ADDRESS[1], IP_ADDRESS[2], \
			   IP_ADDRESS[3]);
		http_task_handle = osThreadNew(hf_http_task, NULL, &http_task_attributes);
		es_send_req(pframe, 1);
		break;
	default:;
		break;
	}
}

void protocol_task(void *argument)
{
	// printf("%s %d\n", __func__, __LINE__);
	b_frame_t frame_info;
	frame_info.head = head_meg;
	// frame_info.head_len = 4;
	frame_info.head_len = sizeof(head_meg) - 1;
	frame_info.end = end_msg;
	frame_info.end_len = sizeof(end_msg) - 1;
	// frame_info.end_len = 4;
	frame_info.pname = "uart3";
	frame_uart3._in_frame_buffer_size = 256;
	es_frame_init(&frame_uart3, &frame_info);
	// memset(&pframe->frame, 0, siezeof(struct frame_data));
	// printf("%s %d\n", __func__, __LINE__);
	/* enable uart3 dma rx */
	HAL_UARTEx_ReceiveToIdle_DMA(&huart3, RxBuf, RxBuf_SIZE);
	uint8_t protocol_status = 0;
	for (;;) {
		switch (protocol_status) {
		case CHECK_HEAD:
			if (!es_check_head(&frame_uart3))
				protocol_status = CHECK_CMD;
			break;
		case CHECK_CMD:
			// printf("%s CHECK_CMD\n", __func__);
			if (!es_get_cmd_and_data(&frame_uart3)) {
				protocol_status = CHECK_TAIL;
			} else {
				es_send_req(&frame_uart3, 0);
				protocol_status = CHECK_HEAD;
			}
			break;
		case CHECK_TAIL:
			// printf("%s CHECK_TAIL\n", __func__);
			if (!es_check_tail(&frame_uart3)) {
				protocol_status = PROCESS_CMD;
			} else {
				es_send_req(&frame_uart3, 0);
				protocol_status = CHECK_HEAD;
			}
			break;
		case PROCESS_CMD:
			es_process_cmd(&frame_uart3);
			protocol_status = CHECK_HEAD;
			break;
		default:
			protocol_status = CHECK_HEAD;
		}
		osDelay(900);
	}
}