/************************************************************************************
* Copyright 2024, Beijing ESWIN Computing Technology Co., Ltd.. All rights reserved.
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
* http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*************************************************************************************/

/************************************************************************************
* SPDX-License-Identifier: MIT, Apache
* 
* Author: xuxiang@eswincomputing.com
* 
* File Description: hf_i2c.h
*  Header file for the hf_i2c.c
************************************************************************************/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HF_I2C_H
#define __HF_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* types ------------------------------------------------------------*/
/* constants --------------------------------------------------------*/
/* macro ------------------------------------------------------------*/
/* define------------------------------------------------------------*/
/* functions prototypes ---------------------------------------------*/
int hf_i2c_reg_write(I2C_HandleTypeDef *hi2c, uint8_t slave_addr, uint8_t reg_addr, uint8_t *data_ptr);
int hf_i2c_reg_read(I2C_HandleTypeDef *hi2c, uint8_t slave_addr, uint8_t reg_addr, uint8_t *data_ptr);
int hf_i2c_mem_read(I2C_HandleTypeDef *hi2c, uint8_t slave_addr, 
						uint8_t reg_addr, uint8_t *data_ptr, uint32_t len);
int hf_i2c_mem_write(I2C_HandleTypeDef *hi2c, uint8_t slave_addr,
						uint8_t reg_addr, uint8_t *data_ptr, uint32_t len);
int hf_i2c_reg_read_block(I2C_HandleTypeDef *hi2c, uint8_t slave_addr,
					uint8_t reg_addr, uint8_t *data_ptr, uint8_t len);
int hf_i2c_reg_write_block(I2C_HandleTypeDef *hi2c, uint8_t slave_addr,
					 uint8_t reg_addr, uint8_t *data_ptr, uint8_t len);
#ifdef __cplusplus
}
#endif

#endif /* __HF_I2C_H */
