#include "cmsis_os2.h"
#include "hf_common.h"
#include "main.h"
#include "protocol.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "list.h"
#include "task.h"

#define head_meg "\xA5\x5A\xAA\x55"
#define end_msg "\x0D\x0A\x0D\x0A"
#define req_ok "\x6B\x6F"
#define req_fail "\x6C\x69\x61\x66"

extern uint8_t ip_address[4];
extern uint8_t netmask_address[4];
extern uint8_t getway_address[4];
extern uint8_t mac_address[6];

void es_send_req(b_frame_class_t *pframe, uint8_t req_cmd, int8_t *frame_data, uint8_t len)
{
	uint8_t buf[MAX_FRAME_LEN] = {0};
	uint8_t b_len, xor = 0;
	for (int i = 0; i < len; i++) {
		xor ^= frame_data[i];
	}
	memcpy(buf, pframe->frame_info.head, pframe->frame_info.head_len);
	b_len = pframe->frame_info.head_len;

	memcpy(buf + b_len, &req_cmd, 1);
	b_len += 1;

	memcpy(buf + b_len, &len, 1);
	b_len += 1;
	memcpy(buf + b_len, frame_data, len);
	b_len += len;

	memcpy(buf + b_len, &xor, 1);
	b_len += 1;

	memcpy(buf + b_len, pframe->frame_info.end, pframe->frame_info.end_len);
	b_len += pframe->frame_info.end_len;

	HAL_UART_Transmit(pframe->uart, buf, b_len, HAL_MAX_DELAY); // transmit the full sentence again
}

int32_t es_set_gpio(struct gpio_cmd *gpio)
{
	GPIO_TypeDef *gpio_group;
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	if (gpio->group == 0)
		gpio_group = GPIOA;
	else if (gpio->group == 1)
		gpio_group = GPIOB;
	else if (gpio->group == 2)
		gpio_group = GPIOC;
	else if (gpio->group == 3)
		gpio_group = GPIOD;
	else if (gpio->group == 4)
		gpio_group = GPIOE;
	else if (gpio->group == 5)
		gpio_group = GPIOF;
	else if (gpio->group == 6)
		gpio_group = GPIOG;
	else if (gpio->group == 7)
		gpio_group = GPIOH;
	else if (gpio->group == 8)
		gpio_group = GPIOI;
	else
		return HAL_ERROR;
	// printf("%s %d\n", __func__, __LINE__);

	HAL_GPIO_WritePin(gpio_group, gpio->pin_num, gpio->value);

	GPIO_InitStruct.Pin = gpio->pin_num;
	GPIO_InitStruct.Mode = gpio->driection;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(gpio_group, &GPIO_InitStruct);
	return HAL_OK;
}

int32_t es_set_eth(struct ip_t *ip, struct netmask_t *netmask, struct getway_t *gw, struct eth_mac_t *mac)
{
	if (ip != NULL) {
		ip_address[0] = ip->ip_addr0;
		ip_address[1] = ip->ip_addr1;
		ip_address[2] = ip->ip_addr2;
		ip_address[3] = ip->ip_addr3;
		// printf("IP_ADDR0:%d IP_ADDR1: %d IP_ADDR2: %d IP_ADDR3 %d\n", \
			// ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
	}
	if (netmask != NULL) {
		netmask_address[0] = netmask->netmask_addr0;
		netmask_address[1] = netmask->netmask_addr1;
		netmask_address[2] = netmask->netmask_addr2;
		netmask_address[3] = netmask->netmask_addr3;
		// printf("NETMASK_ADDR0:%d NETMASK_ADDR1: %d NETMASK_ADDR2: %d NETMASK_ADDR3 %d\n",
		// netmask_address[0], netmask_address[1], netmask_address[2], netmask_address[3]);
	}
	if (gw != NULL) {
		getway_address[0] = gw->getway_addr0;
		getway_address[1] = gw->getway_addr1;
		getway_address[2] = gw->getway_addr2;
		getway_address[3] = gw->getway_addr3;
		// printf("GETWAY_ADDR0:%d GETWAY_ADDR1: %dGETWAY_ADDR2: %d GETWAY_ADDR3 %d\n",
			// getway_address[0], getway_address[1], getway_address[2], getway_address[3]);
	}
	if (mac != NULL) {
		mac_address[0] = mac->eth_mac_addr0;
		mac_address[1] = mac->eth_mac_addr1;
		mac_address[2] = mac->eth_mac_addr2;
		mac_address[3] = mac->eth_mac_addr3;
		mac_address[4] = mac->eth_mac_addr4;
		mac_address[5] = mac->eth_mac_addr5;
		// printf("GETWAY_ADDR0:%d GETWAY_ADDR1: %dGETWAY_ADDR2: %d GETWAY_ADDR3 %d\n",
			// getway_address[0], getway_address[1], getway_address[2], getway_address[3]);
	}
	extern void dynamic_change_eth(void);
	dynamic_change_eth();
	return HAL_OK;
}

int32_t es_set_rtc_date(struct rtc_date_t *sdate)
{
	RTC_DateTypeDef sDate = {0};
	sDate.Year = sdate->Year - 2000;
	sDate.Month = sdate->Month;
	sDate.Date = sdate->Date;
	sDate.WeekDay = sdate->WeekDay;

	// printf("yy/mm/dd  %02d/%02d/%02d\r\n", sDate.Year, sDate.Month, sDate.Date);
	if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
		return HAL_ERROR;
	return HAL_OK;
}

int32_t es_set_rtc_time(struct rtc_time_t *stime)
{
	RTC_TimeTypeDef sTime = {0};
	sTime.Hours = stime->Hours;
	sTime.Minutes = stime->Minutes;
	sTime.Seconds = stime->Seconds;
	sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTime.StoreOperation = RTC_STOREOPERATION_SET;
	if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
		return HAL_ERROR;
	return HAL_OK;
}

int32_t es_get_rtc_date(struct rtc_date_t *sdate)
{
	RTC_DateTypeDef GetData;
	if (HAL_RTC_GetDate(&hrtc, &GetData, RTC_FORMAT_BCD) != HAL_OK)
		return HAL_ERROR;
	// printf("yy/mm/dd  %02d/%02d/%02d\r\n", 2000 + GetData.Year, GetData.Month, GetData.Date);
	sdate->Year = 2000 + GetData.Year;
	sdate->Month = GetData.Month;
	sdate->Date = GetData.Date;
	sdate->WeekDay = GetData.WeekDay;
	return HAL_OK;
}

int32_t es_get_rtc_time(struct rtc_time_t *stime)
{
	RTC_TimeTypeDef GetTime;
	if (HAL_RTC_GetTime(&hrtc, &GetTime, RTC_FORMAT_BCD) != HAL_OK)
		return HAL_ERROR;
	// printf(" hh:mm:ss %02d:%02d:%02d\r\n", GetTime.Hours, GetTime.Minutes, GetTime.Seconds);
	stime->Hours = GetTime.Hours;
	stime->Minutes = GetTime.Minutes;
	stime->Seconds = GetTime.Seconds;
	return HAL_OK;
}

extern uint32_t pwm_period;
uint32_t fan0_duty = 0;
uint32_t fan1_duty = 0;

int32_t es_set_fan_duty(struct fan_control_t *fan)
{
	TIM_OC_InitTypeDef sConfigOC = {0};
	if(fan->duty > 100)
		return HAL_ERROR;

	if (fan->fan_num == 0) {
		if (HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_1) != HAL_OK)
			return HAL_ERROR;

		fan0_duty = fan->duty;
		sConfigOC.OCMode = TIM_OCMODE_PWM1;
		sConfigOC.Pulse = fan->duty*pwm_period/100;
		sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
		sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
		if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
			return HAL_ERROR;

		if (HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1) != HAL_OK)
			return HAL_ERROR;
	} else if (fan->fan_num == 1) {
		if (HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_2) != HAL_OK)
			return HAL_ERROR;

		fan1_duty = fan->duty;
		sConfigOC.OCMode = TIM_OCMODE_PWM1;
		sConfigOC.Pulse = fan->duty*pwm_period/100;
		sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
		sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
		if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
			return HAL_ERROR;

		if (HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2) != HAL_OK)
			return HAL_ERROR;
	} else {
		return HAL_ERROR;
	}
	return HAL_OK;
}

int32_t es_get_fan_duty(struct fan_control_t *fan)
{
	if (fan->fan_num == 0) {
		fan->duty = fan0_duty;
	} else if (fan->fan_num == 1) {
		fan->duty = fan1_duty;
	} else {
		return HAL_ERROR;
	}
	return HAL_OK;
}


void es_process_cmd(b_frame_class_t *pframe)
{
	uint8_t cmd = pframe->frame.cmd;
	// printf("%s %d cmd %x\n", __func__, __LINE__, cmd);
	switch (cmd) {
	case CMD_EEPROM_WP:
		es_eeprom_wp(pframe->frame.data.value);
		es_send_req(pframe, CMD_RES, req_ok, sizeof(req_ok) - 1);
		break;
	case CMD_GPIO:
		if (es_set_gpio(&pframe->frame.data.gpio) != HAL_OK)
			es_send_req(pframe, CMD_RES, req_fail, sizeof(req_fail) - 1);
		else
			es_send_req(pframe, CMD_RES, req_ok, sizeof(req_ok) - 1);
		break;
	case CMD_SET_IP:
		es_set_eth(&pframe->frame.data.ip, NULL, NULL, NULL);
		es_send_req(pframe, CMD_RES, req_ok, sizeof(req_ok) - 1);
		break;
	case CMD_SET_NETMASK:
		es_set_eth(NULL, &pframe->frame.data.netmask, NULL, NULL);
		es_send_req(pframe, CMD_RES, req_ok, sizeof(req_ok) - 1);
		break;
	case CMD_SET_GATWAY:
		es_set_eth(NULL, NULL, &pframe->frame.data.getway, NULL);
		es_send_req(pframe, CMD_RES, req_ok, sizeof(req_ok) - 1);
		break;
	case CMD_SET_MAC:
		es_set_eth(NULL, NULL, NULL, &pframe->frame.data.mac);
		es_send_req(pframe, CMD_RES, req_ok, sizeof(req_ok) - 1);
		break;
	case CMD_SET_DATE:
		if (es_set_rtc_date(&pframe->frame.data.rtc_date) != HAL_OK)
			es_send_req(pframe, CMD_RES, req_fail, sizeof(req_fail) - 1);
		else
			es_send_req(pframe, CMD_RES, req_ok, sizeof(req_ok) - 1);
		break;
	case CMD_SET_TIME:
		if (es_set_rtc_time(&pframe->frame.data.rtc_date) != HAL_OK)
			es_send_req(pframe, CMD_RES, req_fail, sizeof(req_fail) - 1);
		else
			es_send_req(pframe, CMD_RES, req_ok, sizeof(req_ok) - 1);
		break;
	case CMD_SET_FAN_DUTY:
		if (es_set_fan_duty(&pframe->frame.data.fan) != HAL_OK)
			es_send_req(pframe, CMD_RES, req_fail, sizeof(req_fail) - 1);
		else
			es_send_req(pframe, CMD_RES, req_ok, sizeof(req_ok) - 1);
		break;
	case CMD_GET_DATE:
		if (es_get_rtc_date(&pframe->frame.data.rtc_date) != HAL_OK)
			es_send_req(pframe, CMD_RES, req_fail, sizeof(req_fail) - 1);
		else
			es_send_req(pframe, CMD_GET_DATE, &pframe->frame.data.rtc_date, sizeof(struct rtc_date_t));
		// printf("yy/mm/dd  %02d/%02d/%02d %02d\r\n", 2000 + pframe->frame.data.rtc_date.Year, pframe->frame.data.rtc_date.Month, \
				pframe->frame.data.rtc_date.Date, pframe->frame.data.rtc_date.WeekDay);
		break;
	case CMD_GET_TIME:
		if (es_get_rtc_time(&pframe->frame.data.rtc_time) != HAL_OK)
			es_send_req(pframe, CMD_RES, req_fail, sizeof(req_fail) - 1);
		else
			es_send_req(pframe, CMD_GET_TIME, &pframe->frame.data.rtc_time, sizeof(struct rtc_time_t));
		// printf(" hh:mm:ss %02d:%02d:%02d\r\n", pframe->frame.data.rtc_time.Hours,\
						pframe->frame.data.rtc_time.Minutes, pframe->frame.data.rtc_time.Seconds);
		break;
	case CMD_GET_FAN_DUTY:
		if (es_get_fan_duty(&pframe->frame.data.fan) != HAL_OK)
			es_send_req(pframe, CMD_RES, req_fail, sizeof(req_fail) - 1);
		else
			es_send_req(pframe, CMD_GET_FAN_DUTY, &pframe->frame.data.fan, sizeof(struct fan_control_t));
		break;
	default:;
		break;
	}
}

uint32_t RxBuf_SIZE = 64;
uint8_t RxBuf[96];
b_frame_class_t frame_uart3;
void deamon_test(void);
void protocol_task(void *argument)
{
	// printf("%s %d\n", __func__, __LINE__);
	b_frame_t frame_info;
	frame_info.head = head_meg;
	frame_info.head_len = sizeof(head_meg) - 1;
	frame_info.end = end_msg;
	frame_info.end_len = sizeof(end_msg) - 1;
	frame_info.pname = "uart3";
	frame_uart3._in_frame_buffer_size = 256;
	frame_uart3.uart = &huart3;
	es_frame_init(&frame_uart3, &frame_info);
	/* enable uart3 dma rx */
	HAL_UARTEx_ReceiveToIdle_DMA(frame_uart3.uart, RxBuf, RxBuf_SIZE);

	uint8_t protocol_status = 0;
	for (;;) {
		deamon_test();
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
				// printf("%s CHECK_CMD fail\n", __func__);
				es_send_req(&frame_uart3, CMD_RES, req_fail, sizeof(req_fail) - 1);
				protocol_status = CHECK_HEAD;
			}
			break;
		case CHECK_TAIL:
			// printf("%s CHECK_TAIL\n", __func__);
			if (!es_check_tail(&frame_uart3)) {
				protocol_status = PROCESS_CMD;
			} else {
				es_send_req(&frame_uart3, CMD_RES, req_fail, sizeof(req_fail) - 1);
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
		osDelay(10);
	}
}

#define FRAME_HEADER    0xA55AAA55
#define FRAME_TAIL      0xBDBABDBA

uint8_t UART4_RxBuf[sizeof(Message)];
b_frame_class_t frame_uart4;
extern DMA_HandleTypeDef hdma_uart4_rx;
// Define a global list
List_t WebCmdList;

// Define command types
typedef enum {
	MSG_REQUEST = 0x01,
	MSG_REPLY = 0x02,
} MsgType;

void dump_message(Message data)
{
	printf("Header: 0x%lX, Cmd Type: 0x%x, Data Len: %d, Checksum: 0x%X, Tail: 0x%lx\n",
		data.header, data.cmd_type, data.data_len, data.checksum, data.tail);
}
// Function to check message checksum
int check_checksum(Message *msg)
{
	unsigned char checksum = 0;
	checksum ^= msg->msg_type;
	checksum ^= msg->cmd_type;
	checksum ^= msg->data_len;
	for (int i = 0; i < msg->data_len; ++i) {
		checksum ^= msg->data[i];
	}
	return checksum == msg->checksum;
}

void generate_checksum(Message *msg)
{
	unsigned char checksum = 0;
	checksum ^= msg->msg_type;
	checksum ^= msg->cmd_type;
	checksum ^= msg->data_len;
	for (int i = 0; i < msg->data_len; ++i) {
		checksum ^= msg->data[i];
	}
	msg->checksum = checksum;
}

BaseType_t transmit_deamon_request(Message *msg)
{
	UART_HandleTypeDef *huart = &huart4;

	generate_checksum(msg);
	// Transmit using DMA
	HAL_StatusTypeDef status = HAL_UART_Transmit(huart, (uint8_t *)msg,
			sizeof(Message), HAL_MAX_DELAY);
	if (status == HAL_OK) {
		return status; // Successful transmission
	} else {
		printf("[%s %d]:Failed to transmit msg, status %d!\n",__func__,__LINE__, status);
		return status; // Transmission failed
	}
}
int web_cmd_handle(CommandType cmd, void *data, int data_len)
{
	HAL_StatusTypeDef status;
	uint32_t ulNotificationValue;
	int ret = -HAL_ERROR;

	WebCmd webcmd = {
		.cmd_result = -1,
		.data = {0},
		.xTaskToNotify = xTaskGetCurrentTaskHandle(),
	};

	Message msg = {
		.header = FRAME_HEADER,
		.msg_type = MSG_REQUEST,
		.cmd_type = cmd,
		.data_len = data_len,
		.tail = FRAME_TAIL,
	};

	/*Add webcmd to waiting list*/
		// Initialize list item
	vListInitialiseItem(&(webcmd.xListItem));
		// Set the list item owner
	listSET_LIST_ITEM_OWNER(&(webcmd.xListItem), &webcmd);
		// Enter critical section to ensure thread safety when inserting item
	taskENTER_CRITICAL();
	vListInsertEnd(&WebCmdList, &(webcmd.xListItem));
	taskEXIT_CRITICAL();

	msg.xTaskToNotify = (uint32_t)webcmd.xTaskToNotify;
	dump_message(msg);
	status = transmit_deamon_request(&msg);
	if (HAL_OK != status) {
		ret = status;
		goto err_msg;
	}
	/*wait 100ms to get the result*/
	if (xTaskNotifyWait(0, 0, &ulNotificationValue, pdMS_TO_TICKS(100)) == pdTRUE) {
		ret = webcmd.cmd_result;
		memcpy(data, webcmd.data, data_len);
	} else {
		ret = -HAL_TIMEOUT;
		goto err_msg;
	}
	return ret;

err_msg:
	taskENTER_CRITICAL();
	uxListRemove(&(webcmd.xListItem));
	taskEXIT_CRITICAL();
	return ret;
}

void handle_deamon_mesage(Message *msg)
{
	if (MSG_REPLY == msg->msg_type) {
		if (!listLIST_IS_EMPTY(&WebCmdList)) {
			// Enter critical section to ensure thread safety when traversing and deleting
			taskENTER_CRITICAL();
			for (ListItem_t *pxItem = listGET_HEAD_ENTRY(&WebCmdList);
				pxItem != listGET_END_MARKER(&WebCmdList);) {
					WebCmd * pxWebCmd = (WebCmd *)listGET_LIST_ITEM_OWNER(pxItem);
					// Get the next item before deleting the current one
					ListItem_t *pxNextItem = listGET_NEXT(pxItem);
					if ((uint32_t)pxWebCmd->xTaskToNotify == msg->xTaskToNotify) {
						pxWebCmd->cmd_result = msg->cmd_result;
						memcpy(pxWebCmd->data, msg->data, msg->data_len);
						// Remove the current item from the list
						uxListRemove(pxItem);
						xTaskNotifyGive(pxWebCmd->xTaskToNotify);
					}
					// Move to the next item
					pxItem = pxNextItem;
			}
			taskEXIT_CRITICAL();
		}
	}else {
		printf("Unsupport msg type: 0x%x\n", msg->cmd_type);
		dump_message(*msg);
	}
}

volatile int g_test_flag = 0;
void deamon_test(void)
{
	int ret;
	char data[FRAME_DATA_MAX];

	if (g_test_flag == 0)
		return;

	ret = web_cmd_handle(CMD_READ_BOARD_INFO, data, FRAME_DATA_MAX);

	//pass
	printf("call read borad info, result 0x%x, data %s\n", ret, data);
	g_test_flag = 0;
	return;//0 success,TODO call
}

void uart4_protocol_task(void *argument)
{
	b_frame_t frame_info;
	ring_buf_t *ring;
	int32_t len;
	Message msg;

	frame_info.head = head_meg;
	frame_info.head_len = sizeof(head_meg) - 1;
	frame_info.end = end_msg;
	frame_info.end_len = sizeof(end_msg) - 1;
	frame_info.pname = "uart4";
	frame_uart4.uart = &huart4;
	frame_uart4._in_frame_buffer_size = sizeof(UART4_RxBuf);

	//Init msg ring buffer for deamon
	es_frame_init(&frame_uart4, &frame_info);
	ring = &frame_uart4._frame_ring;

	//Init web server msg list
	vListInitialise(&WebCmdList);

	//trigger uart rx
	HAL_UARTEx_ReceiveToIdle_DMA(&huart4, UART4_RxBuf, sizeof(UART4_RxBuf));
	for (;;) {
		len = get_ring_buf_len(ring);
		if (!len) {
			osDelay(10);
			continue;
		}
		if (len < sizeof(msg)) {

		}
		len = read_ring_buf(ring, (uint8_t *)&msg, sizeof(msg));
		if (len != sizeof(msg)) {

		}

		if (msg.header == FRAME_HEADER && msg.tail == FRAME_TAIL) {
			// Check checksum
			if (check_checksum(&msg)) {
				// handle command
				handle_deamon_mesage(&msg);
			} else {
				printf("Checksum error!\n");
				dump_message(msg);
			}
		} else {
			printf("Invalid message format!\n");
			dump_message(msg);
		}
		osDelay(10);
	}
}