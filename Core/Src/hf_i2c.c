// SPDX-License-Identifier: GPL-2.0-only
/*
 * I2C hal api, especially for eeprom(memory type), INA226, PAC1934 and PCA9450
 *
 * Copyright 2024 Beijing ESWIN Computing Technology Co., Ltd.
 *   Authors:
 *    XuXiang<xuxiang@eswincomputing.com>
 *    LinMin<linmin@eswincomputing.com>
 *
 */
#include "stm32f4xx_hal.h"
#include "main.h"
#include <stdio.h>
#include "cmsis_os.h"

void hf_i2c_reinit(I2C_HandleTypeDef *hi2c)
{
	if (hi2c->Instance == I2C1)
	{
		printf("I2C1 reinit\n");
		i2c_deinit(I2C1);
		i2c_init(I2C1);
	}
	else if (hi2c->Instance == I2C3)
	{
		printf("I2C3 reinit\n");
		i2c_deinit(I2C3);
		i2c_init(I2C3);
	}

}
int hf_i2c_reg_write(I2C_HandleTypeDef *hi2c, uint8_t slave_addr,
					 uint8_t reg_addr, uint8_t *data_ptr)
{
	HAL_StatusTypeDef status = HAL_OK;

	status = HAL_I2C_Mem_Write(hi2c, slave_addr, reg_addr, I2C_MEMADD_SIZE_8BIT,
							   data_ptr, 0x1, 0xff);
	if (status != HAL_OK) {
		printf("I2Cx_write_Error(%x) reg %x; status %x\r\n", slave_addr, reg_addr, status);
		hf_i2c_reinit(hi2c);
		return status;
	}
	return status;
}

int hf_i2c_reg_read(I2C_HandleTypeDef *hi2c, uint8_t slave_addr,
					uint8_t reg_addr, uint8_t *data_ptr)
{
	HAL_StatusTypeDef status = HAL_OK;
	status = HAL_I2C_Mem_Read(hi2c, slave_addr, reg_addr, I2C_MEMADD_SIZE_8BIT,
							  data_ptr, 0x1, 0xff);
	if (status != HAL_OK){
		printf("I2Cx_read_Error(%x) reg %x; status %x\r\n", slave_addr, reg_addr, status);
		hf_i2c_reinit(hi2c);
		return status;
	}
	return status;
}

int hf_i2c_mem_read(I2C_HandleTypeDef *hi2c, uint8_t slave_addr,
					uint8_t reg_addr, uint8_t *data_ptr, uint32_t len)
{
	HAL_StatusTypeDef status = HAL_OK;
	uint32_t retry_cnt = 0;

	do {
		retry_cnt++;
		status = HAL_I2C_Mem_Read(hi2c, slave_addr, reg_addr, I2C_MEMADD_SIZE_8BIT,
					data_ptr, len, 1000);
		if ((status != HAL_OK))
		{
			printf("I2Cx_read_Error(%x) reg %x; status %x, tried times: %ld\r\n", slave_addr, reg_addr, status, retry_cnt);
			osDelay(pdMS_TO_TICKS(50));
			hf_i2c_reinit(hi2c);
		}
	}while((HAL_OK != status) && (retry_cnt < 10));

	return status;
}

int hf_i2c_mem_write(I2C_HandleTypeDef *hi2c, uint8_t slave_addr,
					  uint8_t reg_addr, uint8_t *data_ptr, uint32_t len)
{
	int i, offset;
	int unaligned_bytes = 0, aligned_bytes; // the reg_addr offset that is not aligned with 8Bytes
	HAL_StatusTypeDef status = HAL_OK;
	uint32_t retry_cnt = 6;

	// printf("slave_addr %x, reg_addr %x len %ld\n",slave_addr, reg_addr, len);
	if (hi2c->Instance == I2C1)
		HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_RESET);

	// unaligned addr
	unaligned_bytes = ALIGN(reg_addr, 8) - reg_addr;
	unaligned_bytes = MIN(unaligned_bytes, len);
	for (i = 0; i < unaligned_bytes; i++) {
		retry_cnt = 0;
		do {
			retry_cnt++;
			status = HAL_I2C_Mem_Write(hi2c, slave_addr, reg_addr + i,
									I2C_MEMADD_SIZE_8BIT, data_ptr + i, 1,
									1000);
			if (status != HAL_OK)
			{
				printf("I2Cx_write_Error(%x) reg %x; status %x, tried times: %ld\r\n", slave_addr, reg_addr + i, status, retry_cnt);
				osDelay(pdMS_TO_TICKS(50));
				hf_i2c_reinit(hi2c);
			}
		}while((HAL_OK != status) && (retry_cnt < 6));
		// while (HAL_I2C_IsDeviceReady(hi2c, slave_addr, 0xff, 0xff) == HAL_TIMEOUT);
		if (HAL_OK != status)
			goto out;

		/* tWR=5ms is the time from a valid Stop condition of a write sequence to the end of the internal clear/write cycle*/
		osDelay(pdMS_TO_TICKS(5));
	}
	offset = i;
	// 8 bytes page write
	aligned_bytes = ((len - unaligned_bytes) / 8) * 8;
	for (i = 0; i < aligned_bytes;) {
		retry_cnt = 0;
		do {
			retry_cnt++;
			status = HAL_I2C_Mem_Write(hi2c, slave_addr, reg_addr + offset + i,
									I2C_MEMADD_SIZE_8BIT, data_ptr + offset + i, 8,
									1000);
			if (status != HAL_OK)
			{
				printf("I2Cx_write_Error(%x) reg %x; status %x, tried times: %ld\r\n", slave_addr, reg_addr + offset + i, status, retry_cnt);
				osDelay(pdMS_TO_TICKS(50));
				hf_i2c_reinit(hi2c);
			}
		}while((HAL_OK != status) && (retry_cnt < 6));
		// while (HAL_I2C_IsDeviceReady(hi2c, slave_addr, 0xff, 0xff) == HAL_TIMEOUT);
		if (HAL_OK != status)
			goto out;

		i = i + 8;
		osDelay(pdMS_TO_TICKS(5));
	}
	offset = offset + i;
	// the left bytes
	unaligned_bytes = len - unaligned_bytes - aligned_bytes;
	for (i = 0; i < unaligned_bytes; i++) {
		retry_cnt = 0;
		do {
			retry_cnt++;
			status = HAL_I2C_Mem_Write(hi2c, slave_addr, reg_addr + offset + i,
									I2C_MEMADD_SIZE_8BIT, data_ptr + offset + i, 1,
									1000);
			if (status != HAL_OK)
			{
				printf("I2Cx_write_Error(%x) reg %x; status %x, tried times: %ld\r\n", slave_addr, reg_addr + offset + i, status, retry_cnt);
				osDelay(pdMS_TO_TICKS(50));
				hf_i2c_reinit(hi2c);
			}
		}while((HAL_OK != status) && (retry_cnt < 6));
		// while (HAL_I2C_IsDeviceReady(hi2c, slave_addr, 0xff, 0xff) == HAL_TIMEOUT);
		if (HAL_OK != status)
			goto out;

		osDelay(pdMS_TO_TICKS(5));
	}
out:
	if (hi2c->Instance == I2C1)
		HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_SET);
	return status;
}

int hf_i2c_reg_write_block(I2C_HandleTypeDef *hi2c, uint8_t slave_addr,
					 uint8_t reg_addr, uint8_t *data_ptr, uint8_t len)
{
	HAL_StatusTypeDef status = HAL_OK;

	status = HAL_I2C_Mem_Write(hi2c, slave_addr, reg_addr, I2C_MEMADD_SIZE_8BIT,
							   data_ptr, len, 0xff);
	if (status != HAL_OK) {
		printf("I2Cx_write_Error(%x) reg %x; status %x\r\n", slave_addr, reg_addr, status);
		hf_i2c_reinit(hi2c);
		return status;
	}
	return status;
}

int hf_i2c_reg_read_block(I2C_HandleTypeDef *hi2c, uint8_t slave_addr,
					uint8_t reg_addr, uint8_t *data_ptr, uint8_t len)
{
	HAL_StatusTypeDef status = HAL_OK;
	status = HAL_I2C_Mem_Read(hi2c, slave_addr, reg_addr, I2C_MEMADD_SIZE_8BIT,
							  data_ptr, len, pdMS_TO_TICKS(100));
	if (status != HAL_OK){
		printf("I2Cx_read_Error(%x) reg %x; status %x\r\n", slave_addr, reg_addr, status);
		hf_i2c_reinit(hi2c);
		return status;
	}
	return status;
}
