#ifndef __ES_PROTOCOL_CORE_H__
#define __ES_PROTOCOL_CORE_H__

#include "ringbuffer.h"
#include "stdint.h"
#include "string.h"
#include "stm32f4xx_hal.h"

#define B_ERROR 1
#define B_SUCCESS 0
#define MAX_FRAME_LEN 32

typedef struct {
	const uint8_t *pname;
	uint16_t head_len;
	uint16_t end_len;
	const char *head;
	const char *end;
} b_frame_t;

struct gpio_cmd {
	uint16_t group;
	uint16_t pin_num;
	uint8_t driection;
	uint8_t value;
};

struct ip_t {
	uint8_t ip_addr0;
	uint8_t ip_addr1;
	uint8_t ip_addr2;
	uint8_t ip_addr3;
};

struct netmask_t {
	uint8_t netmask_addr0;
	uint8_t netmask_addr1;
	uint8_t netmask_addr2;
	uint8_t netmask_addr3;
};

struct getway_t {
	uint8_t getway_addr0;
	uint8_t getway_addr1;
	uint8_t getway_addr2;
	uint8_t getway_addr3;
};

struct eth_mac_t {
	uint8_t eth_mac_addr0;
	uint8_t eth_mac_addr1;
	uint8_t eth_mac_addr2;
	uint8_t eth_mac_addr3;
	uint8_t eth_mac_addr4;
	uint8_t eth_mac_addr5;
};

struct rtc_date_t {
  uint16_t Year;
  uint8_t Month;
  uint8_t Date;
  uint8_t WeekDay;
};

struct rtc_time_t {
	uint8_t Hours;
	uint8_t Minutes;
	uint8_t Seconds;
};

struct fan_control_t {
	uint8_t fan_num;
	uint8_t duty;
};

struct spi_slv_w32_t {
	uint32_t addr;
	uint32_t value;
};

struct frame_data {
	uint8_t is_valid;
	uint8_t cmd;
	uint16_t len;
	union {
		uint32_t value;
		struct gpio_cmd gpio;
		struct ip_t ip;
		struct netmask_t netmask;
		struct getway_t getway;
		struct eth_mac_t mac;
		struct rtc_date_t rtc_date;
		struct rtc_time_t rtc_time;
		struct fan_control_t fan;
		struct spi_slv_w32_t spislv32;
	} data;
};

typedef struct {
	b_frame_t frame_info;
	ring_buf_t _frame_ring;
	uint32_t _in_frame_buffer_size;
	struct frame_data frame;
	UART_HandleTypeDef *uart;
} b_frame_class_t;

typedef enum protocol_status_type {
	CHECK_HEAD = 0,
	CHECK_CMD = 1,
	CHECK_FRAME_LEN = 2,
	CHECK_DATA = 3,
	CHECK_XOR = 4,
	CHECK_TAIL = 5,
	PROCESS_CMD = 6
}protocol_status_type_e;

typedef enum protocol_cmd_type {
	CMD_EEPROM_WP = 0x90,
	CMD_GPIO = 0x91,
	CMD_SET_IP = 0x92,
	CMD_SET_NETMASK = 0x93,
	CMD_SET_GATWAY = 0x94,
	CMD_SET_MAC = 0x95,
	CMD_SET_DATE = 0x96,
	CMD_SET_TIME = 0x97,
	CMD_SET_FAN_DUTY = 0x98,
	CMD_SPI_SLV_WL = 0x99,
	CMD_GET_DATE = 0xA6,
	CMD_GET_TIME = 0xA7,
	CMD_GET_FAN_DUTY = 0xa8,
	CMD_SPI_SLV_RL = 0xa9,
	CMD_RES = 0xb0
}protocol_cmd_type_t;

uint8_t es_frame_init(b_frame_class_t *pframe, b_frame_t *pframeinit);
uint8_t es_check_head(b_frame_class_t *pframe);
uint8_t es_get_cmd_and_data(b_frame_class_t *pframe);
uint8_t es_check_tail(b_frame_class_t *pframe);
uint8_t es_frame_put(b_frame_class_t *pframe, uint8_t *dat, uint32_t len);

#endif
