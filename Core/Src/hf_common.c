/* Includes ------------------------------------------------------------------*/
#include "hf_common.h"

#include "cmsis_os.h"
#include "main.h"

/* typedef -----------------------------------------------------------*/
/* define ------------------------------------------------------------*/
/* macro -------------------------------------------------------------*/
/* variables ---------------------------------------------------------*/
/* function prototypes -----------------------------------------------*/

int _write(int fd, char *ch, int len)
{
	uint8_t val = '\r';
	uint16_t length = 0;
	for (length = 0; length < len; length++) {
		if (ch[length] == '\n') {
			HAL_UART_Transmit(&huart3, (uint8_t *)&val, 1, 0xFFFF);
		}
		HAL_UART_Transmit(&huart3, (uint8_t *)&ch[length], 1, 0xFFFF);
	}
	return length;
}


void es_eeprom_wp(uint8_t flag)
{
	if(flag) {
		// printf("eeprom wp enable \n");
		HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_SET);
	}
	else {
		// printf("eeprom wp disgenable \n");
		HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_RESET);
	}
}