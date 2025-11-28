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
  * Copyright 2024 Beijing ESWIN Computing Technology Co., Ltd.
  *   Authors:
  *   XuXiang<xuxiang@eswincomputing.com>
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
// Modified for PlatformIO: using FreeRTOS library
#include <cmsis_os2.h>
#include <stdio.h>
#include "stm32f4xx_hal_iwdg.h"
#include "lwip.h"
#include "console.h"
#include "telnet_server.h"
#include "telnet_som_console.h"
#include "telnet_mcu_console.h"
#include "telnet_mcu_server.h"
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
  .stack_size = 1024 * 2,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t key_task_handle;
const osThreadAttr_t gpio_task_attributes = {
  .name = "KeyTask",
  .stack_size = 256 * 3,
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
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
extern void hf_power_task (void* parameter);
extern void hf_gpio_task (void* parameter);
int retry_count = 10;
void hf_main_task(void *argument)
{
  int ret = 0;
  printf("HiFive 106SC, BMC Version:%d.%d.%d!\n",
    (uint8_t)(BMC_SOFTWARE_VERSION_MAJOR), (uint8_t)(BMC_SOFTWARE_VERSION_MINOR), (uint8_t)(BMC_SOFTWARE_VERSION_PATCH));

  /* get board info from eeprom where the MAC is stored */
  do
  {
    osDelay(220);
    ret = es_init_info_in_eeprom();
  } while (ret && (retry_count--));

  if(ret || retry_count <=0) {
    printf("severe error: get info from eeprom failed!!!");
    while(1);
  }
  #if ES_EEPROM_INFO_TEST
  es_eeprom_info_test();
  #endif

  /* init code for LWIP */
  eth_get_address();
  MX_LWIP_Init();

  power_task_handle = osThreadNew(hf_power_task, NULL, &power_task_attributes);
  http_task_handle = osThreadNew(hf_http_task, NULL, &http_task_attributes);
  key_task_handle = osThreadNew(hf_gpio_task, NULL, &gpio_task_attributes);
  uart4_protocol_task_handle = osThreadNew(uart4_protocol_task, NULL, &protocol_task_attributes);
  daemon_keelive_task_handle = osThreadNew(deamon_keeplive_task, NULL, &daemon_keeplive_task_attributes);
  #if ES_PRODUCTION_LINE_TEST
  printf("***Production Line Test Mode!***\n");
  protocol_task_handle = osThreadNew(protocol_task, NULL, &protocol_task_attributes);
  #else
  if (pdTRUE != xbspConsoleInit(CONSOLE_STACK_SIZE, CONSOLE_TASK_PRIORITY, &consoleHandle)) {
    printf("Err:Failed to create CLI!\n");
  }
  telnet_create(23, telenet_receiver_callback, NULL);
  #endif
  telnet_mcu_create(25);
  set_mcu_led_status(LED_MCU_RUNING);

  for(;;)
  {
    osDelay(800);
  }
}


void get_rtc_info(void)
{
  RTC_DateTypeDef GetData;
  RTC_TimeTypeDef GetTime;
  HAL_RTC_GetTime(&hrtc, &GetTime, RTC_FORMAT_BIN);
  /* Get the RTC current Date */
  HAL_RTC_GetDate(&hrtc, &GetData, RTC_FORMAT_BIN);

  /* Display date Format : yy/mm/dd */
  printf("%s yy/mm/dd  %02d/%02d/%02d\r\n", __func__, 2000 + GetData.Year, GetData.Month, GetData.Date);
  /* Display time Format : hh:mm:ss */
  printf("%s hh:mm:ss %02d:%02d:%02d\r\n", __func__, GetTime.Hours, GetTime.Minutes, GetTime.Seconds);


}

