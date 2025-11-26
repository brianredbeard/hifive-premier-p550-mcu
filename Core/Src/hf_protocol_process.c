#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "hf_common.h"
#include "list.h"
#include "main.h"
#include "protocol.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "hf_spi_slv.h"

#define head_meg "\xA5\x5A\xAA\x55"
#define end_msg "\x0D\x0A\x0D\x0A"
#define req_ok "\x6B\x6F"
#define req_fail "\x6C\x69\x61\x66"

extern uint8_t ip_address[4];
extern uint8_t netmask_address[4];
extern uint8_t getway_address[4];
extern uint8_t mac_address[6];

void es_send_req(b_frame_class_t *pframe,uint8_t req_cmd, char *frame_data,uint8_t len)
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
	gpio->group >>= 8;
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
		// printf("IP_ADDR0:%d IP_ADDR1: %d IP_ADDR2: %d IP_ADDR3 %d\n",
		// 	ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
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
	extern void dynamic_change_eth(void);
	dynamic_change_eth();
	return HAL_OK;
}

extern uint32_t pwm_period;
uint32_t fan0_duty = 0;
uint32_t fan1_duty = 0;

int32_t es_set_fan_duty(struct fan_control_t *fan)
{
	TIM_OC_InitTypeDef sConfigOC = {0};
	if (fan->duty > 100)
		return HAL_ERROR;

	if (fan->fan_num == 0) {
		if (HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_1) != HAL_OK)
			return HAL_ERROR;

		sConfigOC.OCMode = TIM_OCMODE_PWM1;
		sConfigOC.Pulse = fan->duty * pwm_period / 100;
		sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
		sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
		if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
			return HAL_ERROR;
		if (HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1) != HAL_OK)
			return HAL_ERROR;
		fan0_duty = fan->duty;
	} else if (fan->fan_num == 1) {
		if (HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_2) != HAL_OK)
			return HAL_ERROR;

		sConfigOC.OCMode = TIM_OCMODE_PWM1;
		sConfigOC.Pulse = fan->duty * pwm_period / 100;
		sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
		sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
		if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
			return HAL_ERROR;
		if (HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2) != HAL_OK)
			return HAL_ERROR;
		fan1_duty = fan->duty;
	} else {
		return HAL_ERROR;
	}
	// printf("fan0_duty %d\n",fan0_duty);
	// printf("fan1_duty %d\n",fan1_duty);
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

void big2little(uint8_t *cmd, uint64_t addr, uint8_t len)
{
	if(len == 4)
	{
		cmd[3] = (addr & 0xff);
		cmd[2] = ((addr >> 8) & 0xff);
		cmd[1] = ((addr >> 16) & 0xff);
		cmd[0] = ((addr >> 24) & 0xff);
	}
	else if(len == 8){
		cmd[7] = (addr & 0xff);
		cmd[6] = ((addr >> 8) & 0xff);
		cmd[5] = ((addr >> 16) & 0xff);
		cmd[4] = ((addr >> 24) & 0xff);
		cmd[3] = ((addr >> 32) & 0xff);
		cmd[2] = ((addr >> 40) & 0xff);
		cmd[1] = ((addr >> 48) & 0xff);
		cmd[0] = ((addr >> 56) & 0xff);
	}
}

void little2big(uint8_t *cmd, uint64_t addr, uint8_t len)
{
	if(len == 4)
	{
		cmd[0] = (addr & 0xff);
		cmd[1] = ((addr >> 8) & 0xff);
		cmd[2] = ((addr >> 16) & 0xff);
		cmd[3] = ((addr >> 24) & 0xff);
	}
	else if(len == 8){
		cmd[0] = (addr & 0xff);
		cmd[1] = ((addr >> 8) & 0xff);
		cmd[2] = ((addr >> 16) & 0xff);
		cmd[3] = ((addr >> 24) & 0xff);
		cmd[4] = ((addr >> 32) & 0xff);
		cmd[5] = ((addr >> 40) & 0xff);
		cmd[6] = ((addr >> 48) & 0xff);
		cmd[7] = ((addr >> 56) & 0xff);
	}
}

int32_t es_spi_wl(struct spi_slv_w32_t *spi)
{
	uint32_t addr = 0;
	uint32_t value = 0;
	big2little((uint8_t *)&addr, spi->addr, 4);
	big2little((uint8_t *)&value, spi->value, 4);
	// printf("%s %d addr %lx val %lx\n",__func__,__LINE__, addr, value);
	return es_spi_write((uint8_t *)&value, addr, 4);
}

int32_t es_spi_rl32(struct spi_slv_w32_t *spi)
{
	uint32_t addr=0, value=0;
	int32_t ret = 0;
	big2little((uint8_t *)&addr, spi->addr, 4);
	ret = eswin_rx((uint8_t *)&value, addr, 4);
	little2big((uint8_t *)&spi->value, value,4);
	return ret;
}


typedef enum req_type { REQ_OK = 0x0, REQ_FAIL = 0x1, REQ_OTHER } req_type_t;
void es_process_cmd(b_frame_class_t *pframe)
{
	uint8_t cmd = pframe->frame.cmd;
	uint8_t req_type = REQ_FAIL;
	switch (cmd) {
	case CMD_EEPROM_WP:
		if (pframe->frame.len == sizeof(uint8_t)) {
			es_eeprom_wp(pframe->frame.data.value);
			req_type = REQ_OK;
		}
		break;
	case CMD_GPIO:
		if (pframe->frame.len == (sizeof(struct gpio_cmd))) {
			if (es_set_gpio(&pframe->frame.data.gpio) == HAL_OK)
				req_type = REQ_OK;
		}
		break;
	case CMD_SET_IP:
		if (pframe->frame.len == sizeof(struct ip_t)) {
			es_set_eth(&pframe->frame.data.ip, NULL, NULL, NULL);
			req_type = REQ_OK;
		}
		break;
	case CMD_SET_NETMASK:
		if (pframe->frame.len == sizeof(struct netmask_t)) {
			es_set_eth(NULL, &pframe->frame.data.netmask, NULL, NULL);
			req_type = REQ_OK;
		}
		break;
	case CMD_SET_GATWAY:
		if (pframe->frame.len == sizeof(struct getway_t)) {
			es_set_eth(NULL, NULL, &pframe->frame.data.getway, NULL);
			req_type = REQ_OK;
		}
		break;
	case CMD_SET_DATE:
		if (pframe->frame.len == (sizeof(struct rtc_date_t) - 1)) {
			uint16_t year =  (pframe->frame.data.rtc_date.Year >> 8) | (pframe->frame.data.rtc_date.Year && 0xff) << 8;
			pframe->frame.data.rtc_date.Year = year;
			if (es_set_rtc_date(&pframe->frame.data.rtc_date) == HAL_OK)
				req_type = REQ_OK;
		}
		break;
	case CMD_SET_TIME:
		if (pframe->frame.len == sizeof(struct rtc_time_t)) {
			if (es_set_rtc_time(&pframe->frame.data.rtc_time) == HAL_OK)
				req_type = REQ_OK;
		}
		break;
	case CMD_SET_FAN_DUTY:
		if (pframe->frame.len == sizeof(struct fan_control_t)) {
			if (es_set_fan_duty(&pframe->frame.data.fan) == HAL_OK)
				req_type = REQ_OK;
		}
		break;
	case CMD_SPI_SLV_WL:
		if (pframe->frame.len == sizeof(struct spi_slv_w32_t)) {
			if (es_spi_wl(&pframe->frame.data.spislv32) == HAL_OK)
				req_type = REQ_OK;
		}
		break;
	case CMD_GET_DATE:
		if (pframe->frame.len == 0) {
			if (es_get_rtc_date(&pframe->frame.data.rtc_date) == HAL_OK)
			{
				es_send_req(pframe, CMD_GET_DATE, (char *)&pframe->frame.data.rtc_date, sizeof(struct rtc_date_t));
				req_type = REQ_OTHER;
			}
		}
		// printf("yy/mm/dd  %02d/%02d/%02d %02d\r\n", 2000 + pframe->frame.data.rtc_date.Year, pframe->frame.data.rtc_date.Month,
				// pframe->frame.data.rtc_date.Date, pframe->frame.data.rtc_date.WeekDay);
		break;
	case CMD_GET_TIME:
		if (pframe->frame.len == 0) {
			if (es_get_rtc_time(&pframe->frame.data.rtc_time) == HAL_OK) {
				es_send_req(pframe, CMD_GET_TIME, (char *)&pframe->frame.data.rtc_time, sizeof(struct rtc_time_t));
				req_type = REQ_OTHER;
			}
		}
		// printf(" hh:mm:ss %02d:%02d:%02d\r\n", pframe->frame.data.rtc_time.Hours,
						// pframe->frame.data.rtc_time.Minutes, pframe->frame.data.rtc_time.Seconds);
		break;
	case CMD_GET_FAN_DUTY:
		printf("len %xuint8_t\n",pframe->frame.len);
		if (pframe->frame.len == sizeof(uint8_t)) {
			if (es_get_fan_duty(&pframe->frame.data.fan) == HAL_OK){
				es_send_req(pframe, CMD_GET_FAN_DUTY, (char *)&pframe->frame.data.fan, sizeof(struct fan_control_t));
				req_type = REQ_OTHER;
			}
		}
		break;
	case CMD_SPI_SLV_RL:
		if (pframe->frame.len == sizeof(uint32_t)) {
			if (es_spi_rl32(&pframe->frame.data.spislv32) == HAL_OK) {
				es_send_req(pframe, CMD_SPI_SLV_RL, (char *)&pframe->frame.data.spislv32.value, sizeof(uint32_t));
				req_type = REQ_OTHER;
			}
		}
		break;
	default:;
		break;
	}
	if (req_type == REQ_OK)
		es_send_req(pframe, CMD_RES, req_ok, sizeof(req_ok) - 1);
	else if (req_type == REQ_FAIL)
		es_send_req(pframe, CMD_RES, req_fail, sizeof(req_fail) - 1);
}

uint32_t RxBuf_SIZE = 64;
uint8_t RxBuf[96];
b_frame_class_t frame_uart3;
void protocol_task(void *argument)
{
	// printf("%s %d\n", __func__, __LINE__);
	uint8_t protocol_status = 0;
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

Message UART4_RxMsg;
extern DMA_HandleTypeDef hdma_uart4_rx;
List_t WebCmdList;
QueueHandle_t xUart4MsgQueue;

// Define command types
typedef enum {
	MSG_REQUEST = 0x01,
	MSG_REPLY,
	MSG_NOTIFLY,
} MsgType;

void dump_message(Message data)
{
	printf("Header: 0x%lX, Msg_type %d, Cmd Type: 0x%x, Data Len: %d, Checksum: 0x%X, Tail: 0x%lx\n",
		data.header, data.msg_type, data.cmd_type, data.data_len, data.checksum, data.tail);
}
// Define a mutex handle
SemaphoreHandle_t xMutex = NULL;
TimerHandle_t xSomPowerOffTimer;
TimerHandle_t xSomRebootTimer;
TimerHandle_t xSomRestartTimer;

// Function to initialize the mutex
void init_transmit_mutex(void) {
    // Create the mutex
    xMutex = xSemaphoreCreateMutex();
    // Check if the mutex was created successfully
    if (xMutex == NULL) {
        // Mutex creation failed
        // Handle the error here
    }
}

// Function to acquire the mutex
void acquire_transmit_mutex(void) {
    // Attempt to take the mutex
    if (xSemaphoreTake(xMutex, portMAX_DELAY) != pdPASS) {
        // Failed to acquire the mutex
        // Handle the error here
    }
}

// Function to release the mutex
void release_transmit_mutex(void) {
    // Release the mutex
    xSemaphoreGive(xMutex);
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

static BaseType_t xTransmitRequestToSOM(Message *msg)
{
	UART_HandleTypeDef *huart = &huart4;

	// Acquire the mutex before transmitting
	acquire_transmit_mutex();

	generate_checksum(msg);
	// Transmit using DMA
	HAL_StatusTypeDef status = HAL_UART_Transmit(huart, (uint8_t *)msg,
			sizeof(Message), HAL_MAX_DELAY);

	// Release the mutex after transmitting
	release_transmit_mutex();

	if (status == HAL_OK) {
		return status; // Successful transmission
	} else {
		if (SOM_DAEMON_ON == get_som_daemon_state()) {
			printf("[%s %d]:Failed to transmit msg, status %d!\n",__func__,__LINE__, status);
		}
		return status; // Transmission failed
	}
}

int web_cmd_handle(CommandType cmd, void *data, int data_len, uint32_t timeout)
{
	HAL_StatusTypeDef status;
	uint32_t ulNotificationValue;
	int ret = HAL_ERROR;

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
	if (SOM_POWER_ON != get_som_power_state()) {
		ret = HAL_ERROR;
		return ret;
	}
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
	//dump_message(msg);
	status = xTransmitRequestToSOM(&msg);
	if (HAL_OK != status) {
		ret = status;
		goto err_msg;
	}
	/*wait to get the result*/
	if (xTaskNotifyWait(0, 0, &ulNotificationValue, pdMS_TO_TICKS(timeout)) == pdTRUE) {
		ret = webcmd.cmd_result;
		if (HAL_OK != ret) {
			printf("[%s %d]:Som process cmd %d failed, ret %d\n",__func__,__LINE__, cmd, ret);
		}
		memcpy(data, webcmd.data, data_len);
	} else {
		ret = HAL_TIMEOUT;
		goto err_msg;
	}
	return ret;

err_msg:
	taskENTER_CRITICAL();
	uxListRemove(&(webcmd.xListItem));
	taskEXIT_CRITICAL();
	return ret;
}

static void buf_dump(uint8_t *data, uint32_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		printf("0x%x ", data[i]);
		if (i != 0 && i % 20 == 0) {
			printf("\n");
		}
	}
	printf("\n");
}

volatile deamon_stats_t som_daemon_state = SOM_DAEMON_OFF;
deamon_stats_t get_som_daemon_state(void)
{
	deamon_stats_t state;

	// Enter critical section to ensure thread safety when traversing and deleting
	taskENTER_CRITICAL();
	state = som_daemon_state;
	taskEXIT_CRITICAL();

	return state;
}

void change_som_daemon_state(deamon_stats_t newState)
{

	// Enter critical section to ensure thread safety when traversing and deleting
	taskENTER_CRITICAL();
	som_daemon_state = newState;
	taskEXIT_CRITICAL();
	return ;
}

void deamon_keeplive_task(void *argument)
{
	int ret = HAL_OK;
	deamon_stats_t old_status;
	static uint8_t count = 0;
	struct rtc_date_t date = {0};
	struct rtc_time_t time = {0};

	for (;;) {
		old_status = get_som_daemon_state();
		if (SOM_POWER_ON != get_som_power_state()) {
			ret = HAL_ERROR;
		} else {
			ret = web_cmd_handle(CMD_BOARD_STATUS, NULL, 0, 1000);
		}
		if (HAL_OK != ret) {
			if (count <= 5 && SOM_DAEMON_ON == get_som_daemon_state())
				printf("SOM keeplive request failed(ret %d)!\n", ret);

			count++;
			if (count >= 5) {
				change_som_daemon_state(SOM_DAEMON_OFF);
			}
		} else {
			change_som_daemon_state(SOM_DAEMON_ON);;
			count = 0;
		}
		if (old_status != get_som_daemon_state()) {
			es_get_rtc_date(&date);
			es_get_rtc_time(&time);
			printf("SOM Daemon status change to %s at %d-%02d-%02d %02d:%02d:%02d!\n",
				get_som_daemon_state() == SOM_DAEMON_ON ? "on" : "off",
				date.Year, date.Month, date.Date, time.Hours, time.Minutes, time.Seconds);
		}
		/*check every 1 second*/
		osDelay(pdMS_TO_TICKS(1000));
	}
}

void handle_notify_mesage(Message *msg)
{
	if (CMD_POWER_OFF == msg->cmd_type) {
		// Here the SOM should at shutdown state in opensbi, we can turn off its power safely
		vStopSomPowerOffTimer();
		printf("Poweroff SOM normaly!\n");
		change_som_power_state(SOM_POWER_OFF);
	} else if (CMD_RESTART == msg->cmd_type) {
		StopSomRestartTimer();
		printf("Restart SOM normaly!\n");
		vRestartSOM();
	}
}

void handle_som_mesage(Message *msg)
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
	} else if (MSG_NOTIFLY == msg->msg_type) {
		handle_notify_mesage(msg);
	} else {
		printf("Unsupport msg type: 0x%x\n", msg->msg_type);
		buf_dump((uint8_t *)msg, sizeof(*msg));
		dump_message(*msg);
	}
}

void vSomPowerOffTimerCallback(TimerHandle_t Timer)
{
	if (SOM_POWER_OFF != get_som_power_state()) {
		change_som_power_state(SOM_POWER_OFF);
		printf("Poweroff SOM timeout, shutdown it now!\n");
	}
}

void TriggerSomPowerOffTimer(void)
{
	if (xTimerStart(xSomPowerOffTimer, 0) != pdPASS) {
		printf("Failed to trigger poweroff Som timer!\n");
	}
}

void vStopSomPowerOffTimer(void)
{
	if (xTimerStop(xSomPowerOffTimer, 0) != pdPASS) {
		printf("Failed to trigger poweroff Som timer!\n");
	}
}

void vSomRebootTimerCallback(TimerHandle_t Timer)
{
	som_reset_control(pdTRUE);
	osDelay(10);
	som_reset_control(pdFALSE);
	printf("Reboot SOM timeout, force reboot SOM!\n");
}

void TriggerSomRebootTimer(void)
{
	if (xTimerStart(xSomRebootTimer, 0) != pdPASS) {
		printf("Failed to trigger Som reboot timer!\n");
	}
}
void StopSomRebootTimer(void)
{
	if (xTimerStop(xSomRebootTimer, 0) != pdPASS) {
		printf("Failed to trigger Som reboot timer!\n");
	}
}

void vSomRestartTimerCallback(TimerHandle_t Timer)
{
	printf("Restart SOM timeout, force restart SOM!\n");
	vRestartSOM();
}

void TriggerSomRestartTimer(void)
{
	if (xTimerStart(xSomRestartTimer, 0) != pdPASS) {
		printf("Failed to trigger Som restart timer!\n");
	}
}

void StopSomRestartTimer(void)
{
	if (xTimerStop(xSomRestartTimer, 0) != pdPASS) {
		printf("Failed to trigger Som restart timer!\n");
	}
}

#define QUEUE_LENGTH 8

void uart4_protocol_task(void *argument)
{
	Message msg;

	init_transmit_mutex();

	xUart4MsgQueue = xQueueCreate(QUEUE_LENGTH, sizeof(Message));
	if (xUart4MsgQueue == NULL) {
		printf("[%s %d]:Failed to create SOM msg queue!\n",__func__,__LINE__);
		return;
	}

	//Init web server cmd list
	vListInitialise(&WebCmdList);

	/* Create a timer with a timeout set to 6 seconds */
	xSomPowerOffTimer = xTimerCreate( "SomPowerOffTimer", pdMS_TO_TICKS(6000),
			pdFALSE, (void *)0, vSomPowerOffTimerCallback);

	if (xSomPowerOffTimer == NULL) {
		printf("[%s %d]:Failed to create SOM poweroff timer!\n",__func__,__LINE__);
		return;
	}

	/* Create a timer with a timeout set to 5 seconds */
	xSomRebootTimer = xTimerCreate( "SomRebootTimer", pdMS_TO_TICKS(5000),
			pdFALSE, (void *)0, vSomRebootTimerCallback);

	if (xSomRebootTimer == NULL) {
		printf("[%s %d]:Failed to create SOM reboot timer!\n",__func__,__LINE__);
		return;
	}

	/* Create a timer with a timeout set to 5 seconds */
	xSomRestartTimer = xTimerCreate( "SomRestartTimer", pdMS_TO_TICKS(5000),
			pdFALSE, (void *)0, vSomRestartTimerCallback);

	if (xSomRestartTimer == NULL) {
		printf("[%s %d]:Failed to create SOM restart timer!\n",__func__,__LINE__);
		return;
	}
	for (;;) {
		if (xQueueReceive(xUart4MsgQueue, &(msg), portMAX_DELAY)) {
			if (msg.header == FRAME_HEADER && msg.tail == FRAME_TAIL) {
				// Check checksum
				if (check_checksum(&msg)) {
					// handle command
					handle_som_mesage(&msg);
				} else {
					printf("[%s %d]:SOM msg checksum error!\n",__func__,__LINE__);
					buf_dump((uint8_t *)&msg, sizeof(msg));
					dump_message(msg);
				}
			} else {
				printf("[%s %d]:Invalid SOM message format!\n",__func__,__LINE__);
				buf_dump((uint8_t *)&msg, sizeof(msg));
				dump_message(msg);
			}
		}
	}
}
