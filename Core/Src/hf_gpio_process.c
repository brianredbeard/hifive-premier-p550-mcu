/* Includes ------------------------------------------------------------------*/
#include <stdio.h>

#include "cmsis_os.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
#include "hf_common.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define SHORT_CLICK_THRESHOLD 50
#define LONG_PRESS_THRESHOLD 4000
#define PRESS_Time 50
#define BUTTON_ERROR_Time 10000

#define FLAGS_KEY			0x00000001U
#define FLAGS_SOM_RST_OUT	0x00000010U
#define FLAGS_MCU_RESET_SOM	0x00000100U
#define FLAGS_KEY_USER_RST	0x00001000U
#define FLAGS_ALL			0x0000FFFFU

#define KEY_PUSHDOWN GPIO_PIN_RESET
#define KEY_RELEASE GPIO_PIN_SET
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
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
	} else if (KEY_USER_RST_Pin == GPIO_Pin) {
		osEventFlagsSet(gpio_eventflags_id, FLAGS_KEY_USER_RST);
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
	int ret = 0;

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
			} else if ((get_som_power_state() == SOM_POWER_OFF) && (currentTime - pressStartTime >= PRESS_Time)) {
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
			if (get_som_power_state() != SOM_POWER_ON) {
				vStopSomPowerOffTimer();
				change_som_power_state(SOM_POWER_ON);
			}
			break;
		case KEY_LONG_PRESS_STATE:
			printf("KEY_LONG_PRESS_STATE time %ld\n", currentTime - pressStartTime);
			button_state = KEY_PRESS_STATE_END;
			if (get_som_power_state() == SOM_POWER_ON) {
				ret = web_cmd_handle(CMD_POWER_OFF, NULL, 0, 2000);
				if (HAL_OK != ret) {
					change_som_power_state(SOM_POWER_OFF);
				}
				// Trigger the Som timer to enusre SOM could poweroff in 5 senconds
				TriggerSomPowerOffTimer();
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
	StopSomRebootTimer();
}

static void som_rst_feedback_process(void)
{
	printf("%s %d som reset\n", __func__, __LINE__);
	if (get_som_power_state() == SOM_DAEMON_ON) {
		som_reset_control(pdTRUE);
		osDelay(2);
		som_reset_control(pdFALSE);
	}
}

static uint8_t get_user_key_status(void)
{
	return HAL_GPIO_ReadPin(KEY_USER_RST_GPIO_Port, KEY_USER_RST_Pin);
}
#define USER_RST_THRESHOLD 10000
static void key_user_rst_process(void)
{
	uint8_t key_status = KEY_PUSHDOWN;
	button_state_t button_state = KEY_IDLE_STATE;
	TickType_t pressStartTime = 0;
	TickType_t currentTime = 0;
	int ret = 0;
	int led_type = 0;

	while (key_status == KEY_PUSHDOWN) {
		currentTime = xTaskGetTickCount();
		switch (button_state) {
		case KEY_IDLE_STATE:
			button_state = KEY_PRESS_DETECTED_STATE;
			pressStartTime = currentTime;
			break;
		case KEY_PRESS_DETECTED_STATE:
			if((currentTime - pressStartTime) > USER_RST_THRESHOLD) {
				button_state = KEY_RELEASE_DETECTED_STATE;
			} else if (get_user_key_status() == KEY_RELEASE) {
				button_state = KEY_PRESS_STATE_END;
			}
			break;
		case KEY_RELEASE_DETECTED_STATE:
			if((currentTime - pressStartTime) >= USER_RST_THRESHOLD)
			{
				printf("restore userdata to factory\n");
				// TODO : user reset function
				led_type = get_mcu_led_status();
				set_mcu_led_status(LED_USER_INFO_RESET);
				es_restore_userdata_to_factory();
			}
			button_state = KEY_PRESS_STATE_END;
			break;
		case KEY_PRESS_STATE_END:
			if (get_user_key_status() == KEY_RELEASE) {
				// printf("sw IDLE_STATE\n");
				set_mcu_led_status(led_type);
				key_status = KEY_RELEASE;
				button_state = IDLE_STATE;
			}
			break;
		}
		osDelay(1);
	}
}

void hf_gpio_task(void *parameter)
{
	int flags = 0;
	printf("hf_gpio_task started!!!\r\n");
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
			if (FLAGS_KEY_USER_RST & flags)
				key_user_rst_process();
		}
	}
}