/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "hf_common.h"
/* Private includes ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/
extern CRC_HandleTypeDef hcrc;
extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c3;
extern RNG_HandleTypeDef hrng;
extern RTC_HandleTypeDef hrtc;
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern DMA_HandleTypeDef hdma_spi1_rx;
extern DMA_HandleTypeDef hdma_spi1_tx;
extern DMA_HandleTypeDef hdma_spi2_rx;
extern DMA_HandleTypeDef hdma_spi2_tx;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim5;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart6;
extern WWDG_HandleTypeDef hwwdg;
extern IWDG_HandleTypeDef hiwdg;

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);
void Error_Handler(void);
void i2c_init(I2C_TypeDef *Instance);
void i2c_deinit(I2C_TypeDef *Instance);
void uart_init(USART_TypeDef *Instance);
void uart_deinit(USART_TypeDef *Instance);
int board_init(void);

/* Private defines -----------------------------------------------------------*/
#define ATX_PS_ON_Pin GPIO_PIN_3
#define ATX_PS_ON_GPIO_Port GPIOE
#define ATX_PWR_OK_Pin GPIO_PIN_4
#define ATX_PWR_OK_GPIO_Port GPIOE
#define DCDC_PWR_OK_Pin GPIO_PIN_5
#define DCDC_PWR_OK_GPIO_Port GPIOE
#define MCU_PHY_RESETN_Pin GPIO_PIN_0
#define MCU_PHY_RESETN_GPIO_Port GPIOC
#define SPI1_NSS_Pin GPIO_PIN_4
#define SPI1_NSS_GPIO_Port GPIOA
#define EEPROM_WP_Pin GPIO_PIN_14
#define EEPROM_WP_GPIO_Port GPIOB
#define UART_MUX_SEL_Pin GPIO_PIN_15
#define UART_MUX_SEL_GPIO_Port GPIOB
#define PWR_LED_Pin GPIO_PIN_10
#define PWR_LED_GPIO_Port GPIOD
#define SLP_LED_Pin GPIO_PIN_11
#define SLP_LED_GPIO_Port GPIOD
#define JTAG_MUX_SEL_Pin GPIO_PIN_10
#define JTAG_MUX_SEL_GPIO_Port GPIOA
#define JTAG_MUX_EN_Pin GPIO_PIN_11
#define JTAG_MUX_EN_GPIO_Port GPIOA
#define PWR_SW_P_Pin GPIO_PIN_12
#define PWR_SW_P_GPIO_Port GPIOA
#define PWR_SW_P_EXTI_IRQn EXTI15_10_IRQn
#define BOOT_SEL0_Pin GPIO_PIN_0
#define BOOT_SEL0_GPIO_Port GPIOD
#define BOOT_SEL1_Pin GPIO_PIN_1
#define BOOT_SEL1_GPIO_Port GPIOD
#define BOOT_SEL2_Pin GPIO_PIN_2
#define BOOT_SEL2_GPIO_Port GPIOD
#define BOOT_SEL3_Pin GPIO_PIN_3
#define BOOT_SEL3_GPIO_Port GPIOD
#define SOM_PMIC_ON_REQ_Pin GPIO_PIN_4
#define SOM_PMIC_ON_REQ_GPIO_Port GPIOD
#define MCU_RESET_SOM_N_Pin GPIO_PIN_5
#define MCU_RESET_SOM_N_GPIO_Port GPIOD
#define SOM_RST_OUT_N_Pin GPIO_PIN_6
#define SOM_RST_OUT_N_GPIO_Port GPIOD
#define SPI2_NSS_Pin GPIO_PIN_9
#define SPI2_NSS_GPIO_Port GPIOB
#define CHASS_FAN_TACH1_Pin GPIO_PIN_0
#define CHASS_FAN_TACH1_GPIO_Port GPIOE
#define CHASS_FAN_TACH2_Pin GPIO_PIN_1
#define CHASS_FAN_TACH2_GPIO_Port GPIOE

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
