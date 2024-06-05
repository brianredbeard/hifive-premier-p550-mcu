/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
#include "stm32f4xx_hal_iwdg.h"
#include "console.h"

/* Private includes ----------------------------------------------------------*/
#include "hf_common.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
osThreadId_t main_task_handle;
const osThreadAttr_t main_task_attributes = {
  .name = "MainTask",
  .stack_size = 1024,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t protocol_task_handle;
osThreadId_t uart4_protocol_task_handle;
osThreadId_t daemon_keelive_task_handle;

const osThreadAttr_t daemon_keeplive_task_attributes = {
  .name = "DaemonTask",
  .stack_size = 1024 * 2,
  .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t protocol_task_attributes = {
  .name = "protocolTask",
  .stack_size = 1024*2,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t power_task_handle;
const osThreadAttr_t power_task_attributes = {
  .name = "PowerTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t key_task_handle;
const osThreadAttr_t gpio_task_attributes = {
  .name = "KeyTask",
  .stack_size = 1024 * 2,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t http_task_handle;
const osThreadAttr_t http_task_attributes = {
  .name = "HttpTask",
  .stack_size = 1024,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
void hf_main_task(void *argument);
void protocol_task(void *argument);
void uart4_protocol_task(void *argument);
void deamon_keeplive_task(void *argument);

/* Private user code ---------------------------------------------------------*/

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* configure the system clock and Initialize all configured peripherals. */
  board_init();

  /* Init scheduler */
  osKernelInitialize();

  /* add mutexes, ... */

  /* add semaphores, ... */

  /* start timers, add new ones, ... */

  /* add queues, ... */

  /* Create the thread(s) */
  /* creation of defaultTask */
  main_task_handle = osThreadNew(hf_main_task, NULL, &main_task_attributes);

  /* add threads, ... */

  /* add events, ... */

  /* Start scheduler */
  osKernelStart();
  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  while (1)
  {

  }
}
static void mcu_status_led_on(uint8_t turnon)
{
  if (HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4) != HAL_OK) {
    printf("HAL_TIM_PWM_Start(&htim4,TIM_CHANNEL_4) err\n");
    Error_Handler();
  }
}
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
extern void hf_power_task (void* parameter);
extern void hf_gpio_task (void* parameter);

void hf_main_task(void *argument)
{
  printf("HiFive 106SC!\n");
  extern void power_test_dome(void);
  mcu_status_led_on(pdTRUE);
  power_test_dome();

  /* get board info from eeprom where the MAC is stored */
  if(es_init_info_in_eeprom()) {
    printf("severe error: get info from eeprom failed!!!");
    while(1);
  }
  #if ES_EEPROM_INFO_TEST
  es_eeprom_info_test();
  #endif

  power_task_handle = osThreadNew(hf_power_task, NULL, &power_task_attributes);
  http_task_handle = osThreadNew(hf_http_task, NULL, &http_task_attributes);
  key_task_handle = osThreadNew(hf_gpio_task, NULL, &gpio_task_attributes);
  uart4_protocol_task_handle = osThreadNew(uart4_protocol_task, NULL, &protocol_task_attributes);
  daemon_keelive_task_handle = osThreadNew(deamon_keeplive_task, NULL, &daemon_keeplive_task_attributes);
  #if ES_PRODUCTION_LINE_TEST
  protocol_task_handle = osThreadNew(protocol_task, NULL, &protocol_task_attributes);
  #else
  if (pdTRUE != xbspConsoleInit(CONSOLE_STACK_SIZE, CONSOLE_TASK_PRIORITY, &consoleHandle)) {
    printf("Err:Failed to create CLI!\n");
  }
  #endif
  // extern void MX_IWDG_Init(void);
  // MX_IWDG_Init();


  for(;;)
  {
    // HAL_IWDG_Refresh(&hiwdg);
    osDelay(800);
  }
}