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
