/************************************************************************************
* Copyright 2024, Beijing ESWIN Computing Technology Co., Ltd.. All rights reserved.
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
* http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*************************************************************************************/

/************************************************************************************
* SPDX-License-Identifier: MIT, Apache
* 
* Author: linmin@eswincomputing.com
* 
* File Description: hf_common.h
*  Declaration of the cbinfo structure and common api, such as power and
   rtc operations.
************************************************************************************/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HF_COMMON_H
#define __HF_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>

#include "FreeRTOS.h"
#include "list.h"
#include "queue.h"
#include "stm32f4xx_hal.h"
#include "task.h"

/* types ------------------------------------------------------------*/
typedef enum {
	IDLE_STATE,
	ATX_PS_ON_STATE,
	DC_PWR_ON_STATE,
	DC_PWR_GOOD_STATE,
	SOM_STATUS_CHECK_STATE,
	POWERON,
	STOP_POWER
} power_state_t;

typedef enum {
	KEY_IDLE_STATE = 0,
	KEY_PRESS_DETECTED_STATE,
	KEY_RELEASE_DETECTED_STATE,
	KEY_SHORT_PRESS_STATE,
	KEY_LONG_PRESS_STATE,
	KEY_DOUBLE_PRESS_STATE,
	KEY_PRESS_STATE_END
} button_state_t;

typedef enum {
	SOM_POWER_OFF,
	SOM_POWER_ON
} power_switch_t;

typedef enum {
	LED_MCU_RUNING = 0x1u,
	LED_SOM_BOOTING,
	LED_SOM_KERNEL_RUNING,
	LED_USER_INFO_RESET
} led_status_t;

typedef enum {
	SOM_DAEMON_OFF,
	SOM_DAEMON_ON,
} deamon_stats_t;

#define FRAME_DATA_MAX 250

// Define command types
typedef enum {
	CMD_POWER_OFF = 0x01,
	CMD_REBOOT,
	CMD_READ_BOARD_INFO,
	CMD_CONTROL_LED,
	CMD_PVT_INFO,
	CMD_BOARD_STATUS,
	CMD_POWER_INFO,
	CMD_RESTART, // cold reboot with power off/on
				 // You can continue adding other command types
} CommandType;

// Message structure
typedef struct {
	uint32_t header;			  // Frame header
	uint32_t xTaskToNotify;		  // id
	uint8_t msg_type;			  // Message type
	uint8_t cmd_type;			  // Command type
	uint8_t cmd_result;			  // command result
	uint8_t data_len;			  // Data length
	uint8_t data[FRAME_DATA_MAX]; // Data
	uint8_t checksum;			  // Checksum
	uint32_t tail;				  // Frame tail
} __attribute__((packed)) Message;

// WebCmd structure
typedef struct {
	ListItem_t xListItem; // FreeRTOS list item, must be the first member of the struct
	TaskHandle_t xTaskToNotify;
	uint8_t cmd_result;			  // command result
	uint8_t data[FRAME_DATA_MAX]; // command result Data
} WebCmd;

extern QueueHandle_t xUart4MsgQueue;
extern Message UART4_RxMsg;

typedef struct {
	uint32_t consumption;
	uint32_t current;
	uint32_t voltage;
} __attribute__((packed)) power_info;

/* The datas(structures) below need to be stored or updated in eeprom */
// The board info that was initialized(burned) in the eeprom at the factory
typedef struct {
	uint32_t magicNumber;
	uint8_t formatVersionNumber;
	uint16_t productIdentifier;
	uint8_t pcbRevision;
	uint8_t bomRevision;
	uint8_t bomVariant;
	char boardSerialNumber[18]; // 18 bytes of serial number, excluding string terminator
	uint8_t manufacturingTestStatus;
	uint8_t ethernetMAC1[6];	// The MAC of the SOM
	uint8_t ethernetMAC2[6];	// The MAC of the SOM
	uint8_t ethernetMAC3[6];	// The MAC of the MCU
	uint32_t crc32Checksum;
	// uint8_t padding[4];
} __attribute__((packed)) CarrierBoardInfo;

typedef struct {
	char AdminName[32];
	char AdminPassword[32];
	uint8_t ip_address[4];
	uint8_t netmask_address[4];
	uint8_t gateway_address[4];
	uint32_t crc32Checksum;
} MCUServerInfo;

typedef struct {
	uint8_t som_pwr_lost_resume_attr;	  // enable(0xE) or disable(0xD) the last power state of the SOM
	uint8_t som_pwr_last_state;			  // record the latest power state of the SOM: POWER ON(0xE), POWER OFF(0xD)
	uint8_t som_dip_switch_soft_ctl_attr; // determin whether the bootsel of the SOM is controlled by software(0xE) via
										  // GPIO or by hardware switch(0xD)
	uint8_t som_dip_switch_soft_state; // record the DIP Switch software state of the SOM, it is used for SOM bootsel,
									   // bit0---bit3 stand for the DIP0---DIP3
} SomPwrMgtDIPInfo;

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

struct fan_control_t {
	uint8_t fan_num;
	uint8_t duty;
};

struct spi_slv_w32_t {
	uint32_t addr;
	uint32_t value;
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

typedef struct {
	int dip01; // 0 on,1 off
	int dip02;
	int dip03;
	int dip04;
	int swctrl; // 0 hw,1,sw
} DIPSwitchInfo;
/* constants --------------------------------------------------------*/
extern UART_HandleTypeDef huart3;

/* macro ------------------------------------------------------------*/
#define __ALIGN_KERNEL(x, a) __ALIGN_KERNEL_MASK(x, (typeof(x))(a)-1)
#define __ALIGN_KERNEL_MASK(x, mask) (((x) + (mask)) & ~(mask))
#define ALIGN(x, a) __ALIGN_KERNEL((x), (a))

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define SOM_MAC0_IDX	0
#define SOM_MAC1_IDX	1
#define MCU_MAC_IDX	2
/* define ------------------------------------------------------------*/
#define BMC_SOFTWARE_VERSION_MAJOR                   2
#define BMC_SOFTWARE_VERSION_MINOR                   4

#define MAGIC_NUMBER	0x45505EF1

#define AT24C_ADDR (0x50<<1)

/*
EEPROM Mapping
-----------------------
Type		Size(Byte)	Offset
-----------------------
Gap		0		256
User Data	96		160
Gap		16		144
B cbinfo	64		80
Gap		16		64
A cbinfo	64		0
*/
#define CBINFO_MAX_SIZE		64
#define GAP_SIZE		16
#define USER_MAX_SIZE		96
/* A, B cbinfo */
#define CARRIER_BOARD_INFO_EEPROM_MAIN_OFFSET	0
#define CARRIER_BOARD_INFO_EEPROM_BACKUP_OFFSET	(CARRIER_BOARD_INFO_EEPROM_MAIN_OFFSET + CBINFO_MAX_SIZE + GAP_SIZE)
/* User data */
#define MCU_SERVER_INFO_EEPROM_OFFSET		(CARRIER_BOARD_INFO_EEPROM_BACKUP_OFFSET + CBINFO_MAX_SIZE + GAP_SIZE)
#define SOM_PWRMGT_DIP_INFO_EEPROM_OFFSET	(MCU_SERVER_INFO_EEPROM_OFFSET + sizeof(MCUServerInfo))


#define DEFAULT_ADMIN_NAME	"admin"
#define DEFAULT_ADMIN_PASSWORD	"123456"

#define SOM_PWR_LOST_RESUME_ENABLE 0xE // the eeprom internal value. For outside users, the corresponding value is TURE
#define SOM_PWR_LOST_RESUME_DISABLE 0xD

#define SOM_PWR_LAST_STATE_ON 0xE // the eeprom internal value. For outside users, the corresponding value is TURE
#define SOM_PWR_LAST_STATE_OFF 0xD

#define SOM_DIP_SWITCH_SOFT_CTL_ENABLE \
	0xE // the eeprom internal value. For outside users, the corresponding value is TURE
#define SOM_DIP_SWITCH_SOFT_CTL_DISABLE 0xD

#define SOM_DIP_SWITCH_STATE_EMMC 0xE1 // the eeprom internal value. For outside users, the corresponding value is 0x1

#define ES_EEPROM_INFO_TEST 0
/* CLI console settings */
#define CONSOLE_INSTANCE USART3
#define CONSOLE_TASK_PRIORITY 1
#define CONSOLE_STACK_SIZE 1024 // 3000
#define consoleHandle huart3

#define ES_PRODUCTION_LINE_TEST 0

/* functions prototypes ---------------------------------------------*/
void hexstr2mac(uint8_t *mac, const char *hexstr);
uint64_t atoh(const char *in, uint32_t len);

void hf_http_task(void *argument);
void es_eeprom_wp(uint8_t flag);
void som_reset_control(uint8_t reset);
int web_cmd_handle(CommandType cmd, void *data, int data_len, uint32_t timeout);

int es_init_info_in_eeprom(void);

int es_get_carrier_borad_info(CarrierBoardInfo *pCarrier_board_info);
int es_set_carrier_borad_info(CarrierBoardInfo *pCarrier_board_info);

int es_get_mcu_mac(uint8_t *p_mac_address, uint8_t index);
int es_set_mcu_mac(uint8_t *p_mac_address, uint8_t index);

int es_get_mcu_ipaddr(uint8_t *p_ip_address);
int es_set_mcu_ipaddr(uint8_t *p_ip_address);

int es_get_mcu_netmask(uint8_t *p_netmask_address);
int es_set_mcu_netmask(uint8_t *p_netmask_address);

int es_get_mcu_gateway(uint8_t *p_gateway_address);
int es_set_mcu_gateway(uint8_t *p_gateway_address);

int es_get_username_password(char *p_admin_name, char *p_admin_password);
int es_set_username_password(const char *p_admin_name, const char *p_admin_password);

int is_som_pwr_lost_resume(void);
int es_set_som_pwr_lost_resume_attr(int isResumePwrLost);

int es_get_som_pwr_last_state(int *p_som_pwr_last_state);
int es_set_som_pwr_last_state(int som_pwr_last_state);

int es_get_som_dip_switch_soft_ctl_attr(int *p_som_dip_switch_soft_ctl_attr);
int es_set_som_dip_switch_soft_ctl_attr(int som_dip_switch_soft_ctl_attr);

int es_get_som_dip_switch_soft_state(uint8_t *p_som_dip_switch_soft_state);
int es_set_som_dip_switch_soft_state(uint8_t som_dip_switch_soft_state);

int es_get_som_dip_switch_soft_state_all(int *p_som_dip_switch_soft_ctl_attr, uint8_t *p_som_dip_switch_soft_state);
int es_set_som_dip_switch_soft_state_all(int som_dip_switch_soft_ctl_attr, uint8_t som_dip_switch_soft_state);

int es_eeprom_info_test(void);
power_switch_t get_som_power_state(void);
void change_som_power_state(power_switch_t newState);
int change_power_status(int status);
void vRestartSOM(void);
deamon_stats_t get_som_daemon_state(void);
void change_som_daemon_state(deamon_stats_t newState);

void TriggerSomPowerOffTimer(void);
void vStopSomPowerOffTimer(void);
void TriggerSomRebootTimer(void);
void StopSomRebootTimer(void);
void TriggerSomRestartTimer(void);

void set_bootsel(uint8_t is_soft_crtl, uint8_t sel);
int get_bootsel(int *pCtl_attr, uint8_t *pSel);
int init_bootsel(void);

int get_dip_switch(DIPSwitchInfo *dipSwitchInfo);
int set_dip_switch(DIPSwitchInfo dipSwitchInfo);

uint32_t es_autoboot(void);

int32_t es_set_rtc_date(struct rtc_date_t *sdate);
int32_t es_set_rtc_time(struct rtc_time_t *stime);
int32_t es_get_rtc_date(struct rtc_date_t *sdate);
int32_t es_get_rtc_time(struct rtc_time_t *stime);
power_info get_power_info(void);
int xSOMRestartHandle(void);
int xSOMRebootHandle(void);

void set_mcu_led_status(led_status_t type);
int es_restore_userdata_to_factory(void);

/* Dynamically change eth */
int32_t es_set_eth(struct ip_t *ip, struct netmask_t *netmask, struct getway_t *gw, struct eth_mac_t *mac);
void MX_I2C3_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __HF_COMMON_H */
