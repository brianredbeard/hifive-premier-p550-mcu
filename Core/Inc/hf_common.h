/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HF_COMMON_H
#define __HF_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "stm32f4xx_hal.h"
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
