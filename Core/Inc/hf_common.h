/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HF_COMMON_H
#define __HF_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "list.h"
#include "task.h"
#include "queue.h"

/* types ------------------------------------------------------------*/
typedef enum {
  IDLE_STATE,
  ATX_PS_ON_STATE,
  ATX_PWR_GOOD_STATE,
  DC_PWR_GOOD_STATE,
  SOM_STATUS_CHECK_STATE,
  RESET_SOM,
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
  SOM_DAEMON_OFF,
  SOM_DAEMON_ON,
} deamon_stats_t;

#define FRAME_DATA_MAX 250

// Define command types
typedef enum {
	CMD_POWER_OFF = 0x01,
	CMD_RESET,
	CMD_READ_BOARD_INFO,
	CMD_CONTROL_LED,
	CMD_PVT_INFO,
	CMD_BOARD_STATUS,
	// You can continue adding other command types
} CommandType;

// Message structure
typedef struct {
	uint32_t header;	// Frame header
	uint32_t xTaskToNotify; // id
	uint8_t msg_type; 	// Message type
	uint8_t cmd_type; 	// Command type
	uint8_t cmd_result;  // command result
	uint8_t data_len; 	// Data length
	uint8_t data[FRAME_DATA_MAX]; // Data
	uint8_t checksum; 	// Checksum
	uint32_t tail;		// Frame tail
} __attribute__((packed)) Message;

// WebCmd structure
typedef struct {
	ListItem_t xListItem; 	// FreeRTOS list item, must be the first member of the struct
	TaskHandle_t xTaskToNotify;
	uint8_t cmd_result;  // command result
	uint8_t data[FRAME_DATA_MAX]; // command result Data
} WebCmd;

extern QueueHandle_t xUart4MsgQueue;
extern Message UART4_RxMsg;

/* The datas(structures) below need to be stored or updated in eeprom */
// The board info that was initialized(burned) in the eeprom at the factory
typedef struct {
	uint32_t magicNumber;
	uint8_t formatVersionNumber;
	uint16_t productIdentifier;
	uint8_t pcbRevision;
	uint8_t bomRevision;
	uint8_t bomVariant;
	uint8_t boardSerialNumber[18];
	uint8_t manufacturingTestStatus;
	uint8_t ethernetMAC1[6];	// The MAC of the SOM
	uint8_t ethernetMAC2[6];	// The MAC of the SOM
	uint8_t ethernetMAC3[6];	// The MAC of the MCU
	uint32_t crc32Checksum;
	// uint8_t padding[4];
} CarrierBoardInfo;

typedef struct {
	char AdminName[32];
	char AdminPassword[32];
	uint8_t ip_address[4];
	uint8_t netmask_address[4];
	uint8_t gateway_address[4];
	// uint8_t padding[4];
} MCUServerInfo;

typedef struct {
	uint8_t som_pwr_lost_resume_attr;	// enable(0xE) or disable(0xD) the last power state of the SOM
	uint8_t som_pwr_last_state;		// record the latest power state of the SOM: POWER ON(0xE), POWER OFF(0xD)
	uint8_t som_dip_switch_soft_ctl_attr;	// determin whether the bootsel of the SOM is controlled by software(0xE) via GPIO or by hardware switch(0xD)
	uint8_t som_dip_switch_soft_state;	// record the DIP Switch software state of the SOM, it is used for SOM bootsel, bit0---bit3 stand for the DIP0---DIP3
} SomPwrMgtDIPInfo;

/* constants --------------------------------------------------------*/
/* macro ------------------------------------------------------------*/
#define __ALIGN_KERNEL(x, a)		__ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#define ALIGN(x, a)		__ALIGN_KERNEL((x), (a))

#define MAX(x , y)  (((x) > (y)) ? (x) : (y))
#define MIN(x , y)  (((x) < (y)) ? (x) : (y))
/* define ------------------------------------------------------------*/
#define MAGIC_NUMBER	0xdeadbeaf

#define AT24C_ADDR (0x50<<1)

#define CARRIER_BOARD_INFO_EEPROM_OFFSET	0
#define MCU_SERVER_INFO_EEPROM_OFFSET		(CARRIER_BOARD_INFO_EEPROM_OFFSET + sizeof(CarrierBoardInfo))
#define SOM_PWRMGT_DIP_INFO_EEPROM_OFFSET	(MCU_SERVER_INFO_EEPROM_OFFSET + sizeof(MCUServerInfo))

#define DEFAULT_ADMIN_NAME	"admin"
#define DEFAULT_ADMIN_PASSWORD	"123456"

#define SOM_PWR_LOST_RESUME_ENABLE	0xE // the eeprom internal value. For outside users, the corresponding value is TURE
#define SOM_PWR_LOST_RESUME_DISABLE	0xD

#define SOM_PWR_LAST_STATE_ON	0xE // the eeprom internal value. For outside users, the corresponding value is TURE
#define SOM_PWR_LAST_STATE_OFF	0xD

#define SOM_DIP_SWITCH_SOFT_CTL_ENABLE	0xE // the eeprom internal value. For outside users, the corresponding value is TURE
#define SOM_DIP_SWITCH_SOFT_CTL_DISABLE	0xD

#define SOM_DIP_SWITCH_STATE_EMMC	0xE1 // the eeprom internal value. For outside users, the corresponding value is 0x1


#define ES_EEPROM_INFO_TEST 0

/* functions prototypes ---------------------------------------------*/
void hexstr2mac(uint8_t *mac, char *hexstr);

void hf_http_task(void *argument);
void es_eeprom_wp(uint8_t flag);
void som_reset_control(uint8_t reset);
int web_cmd_handle(CommandType cmd, void *data, int data_len, uint32_t timeout);

int es_init_info_in_eeprom(void);

int es_get_carrier_borad_info(CarrierBoardInfo *pCarrier_board_info);

int es_get_mcu_mac(uint8_t *p_mac_address);
int es_set_mcu_mac(uint8_t *p_mac_address);

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

int es_eeprom_info_test(void);
power_switch_t get_som_power_state(void);
void change_som_power_state(power_switch_t newState);
deamon_stats_t get_som_daemon_state(void);
void change_som_daemon_state(deamon_stats_t newState);

void TriggerSomTimer(void);

#ifdef __cplusplus
}
#endif

#endif /* __HF_COMMON_H */
