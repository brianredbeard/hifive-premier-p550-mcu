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

/* Private includes ----------------------------------------------------------*/
#include "hf_common.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
osThreadId_t main_task_handle;
const osThreadAttr_t main_task_attributes = {
  .name = "MainTask",
  .stack_size = 1024*2,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t protocol_task_handle;
const osThreadAttr_t protocol_task_attributes = {
  .name = "protocolTask",
  .stack_size = 1024*8,
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

osThreadId_t moniter_task_handle;
const osThreadAttr_t MoniterTask_attributes = {
  .name = "MoniterTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
void hf_main_task(void *argument);
void MoniterTask(void *argument);
void protocol_task(void *argument);

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
  printf("\r\n%s %d %s %s\r\n",__func__, __LINE__, __DATE__, __TIME__);

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

  power_task_handle = osThreadNew(hf_power_task, NULL, &power_task_attributes);
  key_task_handle = osThreadNew(hf_gpio_task, NULL, &gpio_task_attributes);
  http_task_handle = osThreadNew(hf_http_task, NULL, &http_task_attributes);
  moniter_task_handle = osThreadNew(MoniterTask, NULL, &MoniterTask_attributes);
  // protocol_task_handle = osThreadNew(protocol_task, NULL, &protocol_task_attributes);
  printf("HiFive 106SC \n");
  osDelay(900);
  // extern void MX_IWDG_Init(void);

	// MX_IWDG_Init();

  mcu_status_led_on(pdTRUE);
  for(;;)
  {

    // HAL_IWDG_Refresh(&hiwdg);
    osDelay(800);
  }
}


#if 0
void get_rtc_info(void)
{
  RTC_DateTypeDef GetData;
  RTC_TimeTypeDef GetTime;
  RTC_AlarmTypeDef alarm1;
  RTC_AlarmTypeDef alarm2;
  HAL_RTC_GetTime(&hrtc, &GetTime, RTC_FORMAT_BIN);
  /* Get the RTC current Date */
  HAL_RTC_GetDate(&hrtc, &GetData, RTC_FORMAT_BIN);

  /* Display date Format : yy/mm/dd */
  printf("%02d/%02d/%02d\r\n",2000 + GetData.Year, GetData.Month, GetData.Date);
  /* Display time Format : hh:mm:ss */
  printf("%02d:%02d:%02d\r\n",GetTime.Hours, GetTime.Minutes, GetTime.Seconds);

  HAL_RTC_GetAlarm(&hrtc, &alarm1, RTC_ALARM_A, RTC_FORMAT_BIN);
  HAL_RTC_GetAlarm(&hrtc, &alarm2, RTC_ALARM_B, RTC_FORMAT_BIN);
  printf("alarm1 info :\n");
  printf("AlarmMask %x, AlarmSubSecondMask %x, \n AlarmDateWeekDaySel %x, AlarmDateWeekDay %x, Alarm %x:\n",
        alarm1.AlarmMask, alarm1.AlarmSubSecondMask, alarm1.AlarmDateWeekDaySel, alarm1.AlarmDateWeekDay, alarm1.Alarm );
  printf("%02d/%02d/%02d\r\n",alarm1.AlarmTime.Hours, alarm1.AlarmTime.Minutes, alarm1.AlarmTime.Seconds);

  printf("alarm2 info :\n");
  printf("AlarmMask %x, AlarmSubSecondMask %x, \n AlarmDateWeekDaySel %x, AlarmDateWeekDay %x, Alarm %x:\n",
        alarm2.AlarmMask, alarm2.AlarmSubSecondMask, alarm2.AlarmDateWeekDaySel, alarm2.AlarmDateWeekDay, alarm2.Alarm );
  printf("%02d/%02d/%02d\r\n",alarm2.AlarmTime.Hours, alarm2.AlarmTime.Minutes, alarm2.AlarmTime.Seconds);
  printf("\r\n");
}
#endif
void MoniterTask(void *argument)
{
  TaskStatus_t *StatusArray;
  UBaseType_t task_num;
  uint32_t TotalRunTime;
  TaskHandle_t TaskHandle;
  TaskStatus_t TaskStatus;

  osDelay(9000);
  for(;;)
  {
    task_num = uxTaskGetNumberOfTasks();      //获取系统任务数量
    printf("\nuxTaskGetNumberOfTasks %ld\r\n", task_num);

    StatusArray = pvPortMalloc(task_num*sizeof(TaskStatus_t));//申请内存
    if(StatusArray!=NULL)                   //内存申请成功
    {
        uxTaskGetSystemState((TaskStatus_t*   )StatusArray,   //任务信息存储数组
                                      (UBaseType_t     )task_num,  //任务信息存储数组大小
                                      (uint32_t*       )&TotalRunTime);//保存系统总的运行时间
        printf("\nTaskName\t\tPriority\t\tTaskNumber\t\t\r\n");
        for(uint32_t x=0;x < task_num;x++)
        {
              TaskHandle = xTaskGetHandle(StatusArray[x].pcTaskName);         //根据任务名获取任务句柄

              vTaskGetInfo((TaskHandle_t  )TaskHandle,        //任务句柄
                          (TaskStatus_t* )&TaskStatus,       //任务信息结构体
                          (BaseType_t    )pdTRUE,            //允许统计任务堆栈历史最小剩余大小
                          (eTaskState    )eInvalid);         //函数自己获取任务运行壮态

              printf("task name:                %s\r\n",TaskStatus.pcTaskName);
              printf("task number:              %d\r\n",(int)TaskStatus.xTaskNumber);
              printf("task state:              %d\r\n",TaskStatus.eCurrentState);
              printf("task run time:             %d\r\n",(int)TaskStatus.ulRunTimeCounter);
              printf("task stack base:        %#x\r\n",(int)TaskStatus.pxStackBase);
              printf("stack high water mark: %d\r\n",TaskStatus.usStackHighWaterMark);
        }
    }
    vPortFree(StatusArray);
    osDelay(5000);

  }
}
