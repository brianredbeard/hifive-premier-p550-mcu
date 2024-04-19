/* Includes ------------------------------------------------------------------*/
#include <stdio.h>

#include "cmsis_os.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
#include "hf_common.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define SHORT_CLICK_THRESHOLD 50
#define LONG_PRESS_THRESHOLD 3000
#define PRESS_Time 100
#define BUTTON_ERROR_Time 10000

#define FLAGS_KEY 0x00000001U
#define FLAGS_SOM_RST_OUT 0x00000010U
#define FLAGS_MCU_RESET_SOM 0x00000100U
#define FLAGS_ALL 0x00000FFFU

#define KEY_PUSHDOWN GPIO_PIN_RESET
#define KEY_RELEASE GPIO_PIN_SET
/* Private macro -------------------------------------------------------------*/
typedef enum {
  KEY_IDLE_STATE = 0,
  KEY_PRESS_DETECTED_STATE,
  KEY_RELEASE_DETECTED_STATE,
  KEY_SHORT_PRESS_STATE,
  KEY_LONG_PRESS_STATE,
  KEY_DOUBLE_PRESS_STATE,
  KEY_PRESS_STATE_END
} button_state_t;

/* Private variables ---------------------------------------------------------*/
extern power_switch_t som_power_state;

osEventFlagsId_t gpio_eventflags_id = NULL;

/* Private function prototypes -----------------------------------------------*/

/**
 * @brief  EXTI line detection callbacks.
 * @param  GPIO_Pin Specifies the pins connected EXTI line
 * @retval None
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(GPIO_Pin);
	if (PWR_SW_P_Pin == GPIO_Pin) {
		// printf("PWR_SW_P_Pin it \n");
		osEventFlagsSet(gpio_eventflags_id, FLAGS_KEY);
	} else if (SOM_RST_OUT_N_Pin == GPIO_Pin) {
		// printf("SOM_RST_OUT_N_Pin it  som software reset\n");
		osEventFlagsSet(gpio_eventflags_id, FLAGS_SOM_RST_OUT);
	} else if (MCU_RESET_SOM_N_Pin == GPIO_Pin) {
		// printf("MCU_RESET_SOM_N_Pin it   som key reset\n");
		osEventFlagsSet(gpio_eventflags_id, FLAGS_MCU_RESET_SOM);
	}
}

static uint8_t get_key_status(void)
{
	return HAL_GPIO_ReadPin(PWR_SW_P_GPIO_Port, PWR_SW_P_Pin);
}

static void key_process(void)
{
	uint8_t key_status = KEY_PUSHDOWN;
	button_state_t button_state = KEY_IDLE_STATE;
	TickType_t pressStartTime = 0;
	TickType_t ReleaseTime = 0;
	TickType_t currentTime = 0;

	while (key_status == KEY_PUSHDOWN) {
		currentTime = xTaskGetTickCount();
		switch (button_state) {
		case KEY_IDLE_STATE:
			button_state = KEY_PRESS_DETECTED_STATE;
			pressStartTime = currentTime;
			// printf("pressStartTime %d\n",pressStartTime);
			break;
		case KEY_PRESS_DETECTED_STATE:
			if (get_key_status() == KEY_RELEASE) {
				// printf("KEY_PRESS_DETECTED_STATE\n");
				ReleaseTime = currentTime;
				button_state = KEY_RELEASE_DETECTED_STATE;
			} else if (currentTime - pressStartTime > LONG_PRESS_THRESHOLD) {
				button_state = KEY_LONG_PRESS_STATE;
			} else if ((som_power_state == SOM_POWER_OFF) && (currentTime - pressStartTime >= PRESS_Time)) {
				button_state = KEY_SHORT_PRESS_STATE;
			}
			break;
		case KEY_RELEASE_DETECTED_STATE:
			if ((get_key_status() == KEY_PUSHDOWN) && (currentTime - ReleaseTime <= PRESS_Time)) {
				// printf("double ReleaseTime - pressStartTime %ld\n", ReleaseTime - pressStartTime);
				// printf("double currentTime - pressStartTime %ld\n", currentTime - ReleaseTime);
				button_state = KEY_DOUBLE_PRESS_STATE;
			} else if (ReleaseTime - pressStartTime <= PRESS_Time) {
				// printf("err ReleaseTime - pressStartTime %ld\n", ReleaseTime - pressStartTime);
				// printf("err currentTime - pressStartTime %ld\n", currentTime - ReleaseTime);
				button_state = KEY_DOUBLE_PRESS_STATE;
			} else if ((currentTime - pressStartTime >= LONG_PRESS_THRESHOLD)) {
				// printf("long ReleaseTime - pressStartTime %ld\n", ReleaseTime - pressStartTime);
				// printf("long currentTime - pressStartTime %ld\n", currentTime - ReleaseTime);
				button_state = KEY_PRESS_STATE_END;
			} else if ((currentTime - pressStartTime >= SHORT_CLICK_THRESHOLD)) {
				// printf("short ReleaseTime - pressStartTime %ld\n",ReleaseTime - pressStartTime);
				// printf("short currentTime - pressStartTime %ld\n",currentTime - ReleaseTime);
				button_state = KEY_SHORT_PRESS_STATE;
			} else {
				printf("other ReleaseTime - pressStartTime %ld\n", ReleaseTime - pressStartTime);
				printf("other currentTime - ReleaseTime %ld\n", currentTime - ReleaseTime);
				button_state = KEY_DOUBLE_PRESS_STATE;
			}
			break;
		case KEY_SHORT_PRESS_STATE:
			printf("KEY_SHORT_PRESS_STATE time %ld\n", currentTime - pressStartTime);
			button_state = KEY_PRESS_STATE_END;
			if (som_power_state != SOM_POWER_ON) {
				som_power_state = SOM_POWER_ON;
			}
			break;
		case KEY_LONG_PRESS_STATE:
			printf("KEY_LONG_PRESS_STATE time %ld\n", currentTime - pressStartTime);
			button_state = KEY_PRESS_STATE_END;
			if (som_power_state == SOM_POWER_ON) {
				som_power_state = SOM_POWER_OFF;
			}
			break;
		case KEY_DOUBLE_PRESS_STATE:
			// printf("KEY_DOUBLE_PRESS_STATE time %ld\n",currentTime - pressStartTime);
			// printf("other currentTime - ReleaseTime %ld\n", currentTime - ReleaseTime);
			button_state = KEY_PRESS_STATE_END;
			break;
		case KEY_PRESS_STATE_END:
			if (get_key_status() == KEY_RELEASE) {
				// printf("sw IDLE_STATE\n");
				ReleaseTime = currentTime;
				key_status = KEY_RELEASE;
				button_state = IDLE_STATE;
			}
			break;
		}
		osDelay(1);
	}
}

static void mcu_reset_som_process(void)
{
	printf("%s %d mcu reset som\n", __func__, __LINE__);
}

static void som_rst_feedback_process(void)
{
	printf("%s %d som reset\n", __func__, __LINE__);
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = MCU_RESET_SOM_N_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(MCU_RESET_SOM_N_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_WritePin(MCU_RESET_SOM_N_GPIO_Port, MCU_RESET_SOM_N_Pin, GPIO_PIN_RESET);
	osDelay(2);
	GPIO_InitStruct.Pin = MCU_RESET_SOM_N_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(MCU_RESET_SOM_N_GPIO_Port, &GPIO_InitStruct);
}

void hf_gpio_task(void *parameter)
{
	int flags = 0;
	gpio_eventflags_id = osEventFlagsNew(NULL);
	while (1) {
		flags = osEventFlagsWait(gpio_eventflags_id, FLAGS_ALL, osFlagsWaitAny, osWaitForever);
		if (flags > 0) {
			if (flags & FLAGS_KEY)
				key_process();
			if (FLAGS_SOM_RST_OUT & flags)
				som_rst_feedback_process();
			if (FLAGS_MCU_RESET_SOM & flags)
				mcu_reset_som_process();
		}
	}
}
