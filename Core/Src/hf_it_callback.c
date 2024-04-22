#include <stdio.h>
#include <string.h>

#include "cmsis_os.h"
#include "hf_common.h"
#include "main.h"
#include "stm32f4xx_hal_iwdg.h"

/**
 * @brief  Alarm A callback.
 * @param  hrtc pointer to a RTC_HandleTypeDef structure that contains
 *                the configuration information for RTC.
 * @retval None
 */
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(hrtc);

	printf("\r\n HAL_RTC_AlarmAEventCallback \r\n");
	/* NOTE: This function should not be modified, when the callback is needed,
			 the HAL_RTC_AlarmAEventCallback could be implemented in the user file
	 */
}

/**
 * @brief  Alarm B callback.
 * @param  hrtc pointer to a RTC_HandleTypeDef structure that contains
 *                the configuration information for RTC.
 * @retval None
 */
void HAL_RTCEx_AlarmBEventCallback(RTC_HandleTypeDef *hrtc)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(hrtc);

	/* NOTE: This function should not be modified, when the callback is needed,
			 the HAL_RTCEx_AlarmBEventCallback could be implemented in the user file
	 */
	printf("\r\n HAL_RTCEx_AlarmBEventCallback \r\n");
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM1 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM1) {
		HAL_IncTick();
	}
}

extern DMA_HandleTypeDef hdma_usart3_rx;

#include "protocol_lib/protocol.h"
#include "protocol_lib/ringbuffer.h"
#include "stm32f4xx_hal_dma.h"
extern uint32_t RxBuf_SIZE;
extern uint8_t RxBuf[96];
extern b_frame_class_t frame_uart3;
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	if (huart->Instance == USART3) {
		printf("%s Size %d sizeof(RxBuf) %d\n", __func__, Size, sizeof(RxBuf));
		es_frame_put(&frame_uart3, RxBuf, Size);
		memset(RxBuf, 0, sizeof(RxBuf) / sizeof(uint8_t));

		HAL_UARTEx_ReceiveToIdle_DMA(&huart3, RxBuf, RxBuf_SIZE);
		__HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);
	}
}


/**
  * @brief  Input Capture callback in non-blocking mode
  * @param  htim TIM IC handle
  * @retval None
  */
uint32_t PWM2_T_Count =0;
uint32_t PWM2_D_Count =0;

__weak void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(htim);
	if(htim->Instance == TIM5)
	{
		if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
		{
			PWM2_T_Count = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
		}
		else if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)   // 通道2
		{
			PWM2_D_Count = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
		}
	}
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	   ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	printf("Wrong parameters value: file %s on line %ld\r\n", file, line);
}
#endif /* USE_FULL_ASSERT */
