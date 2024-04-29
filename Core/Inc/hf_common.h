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

#define FRAME_DATA_MAX 250

// Define command types
typedef enum {
	CMD_POWER_OFF = 0x01,
	CMD_RESET = 0x02,
	CMD_READ_BOARD_INFO = 0x03,
	CMD_CONTROL_LED = 0x04,
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

extern uint8_t UART4_RxBuf[sizeof(Message)];

/* constants --------------------------------------------------------*/
/* macro ------------------------------------------------------------*/
/* define ------------------------------------------------------------*/

/* functions prototypes ---------------------------------------------*/

void hf_http_task(void *argument);
void es_eeprom_wp(uint8_t flag);


#ifdef __cplusplus
}
#endif

#endif /* __HF_COMMON_H */
