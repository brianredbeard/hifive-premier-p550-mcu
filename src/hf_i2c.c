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

/* Power monitoring IC definitions (INA226, PAC1934) */
#define INA226_12V_ADDR (0X44U << 1)
#define PAC1934_ADDR (0X10U << 1)
#define INA2XX_CONFIG 0x00
#define INA2XX_SHUNT_VOLTAGE 0x01 /* readonly */
#define INA2XX_BUS_VOLTAGE 0x02	  /* readonly */
#define INA2XX_POWER 0x03		  /* readonly */
#define INA2XX_CURRENT 0x04		  /* readonly */
#define INA2XX_CALIBRATION 0x05
#define INA226_BUS_LSB 1250 /*uV*/
#define INA226_SHUNT_RESISTOR 1000							 /*uOhm*/
#define INA226_CURRENT_LSB (2500000 / INA226_SHUNT_RESISTOR) /*uA */
#define INA226_POWER_LSB_FACTOR 25
#define PAC193X_CMD_CTRL 0x1
#define PAC193X_CMD_VBUS1 0x7
#define PAC193X_CMD_VSENSE1 0xb
#define PAC193X_CMD_VPOWER1 0x17
#define PAC193X_CMD_REFRESH_V 0x1F
#define PAC193X_CMD_NEG_PWR_ACT 0x23
#define PAC193X_COSTANT_PWR_M 3200000000ull /* 3.2V^2*1000mO*/
#define PAC193X_COSTANT_CURRENT_M 100000	/* 100mv*1000mO*/
#define PAC193X_SHUNT_RESISTOR_M 4			/* mO*/
#define DIV_ROUND_CLOSEST(x, divisor) (        \
	{                                          \
		typeof(x) __x = x;                     \
		typeof(divisor) __d = divisor;         \
		(((typeof(x))-1) > 0 ||                \
		 ((typeof(divisor))-1) > 0 ||          \
		 (((__x) > 0) == ((__d) > 0)))         \
			? (((__x) + ((__d) / 2)) / (__d))  \
			: (((__x) - ((__d) / 2)) / (__d)); \
	})
#define SWAP16(w) ((((w) & 0xff) << 8) | (((w) & 0xff00) >> 8))
#define SWAP32(w) ((((w) & 0xff) << 24) | (((w) & 0xff00) << 8) | (((w) & 0xff0000) >> 8) | (((w) & 0xff000000) >> 24))

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

/* Power monitoring functions */
static int ina226_init(void)
{
	uint16_t default_cfg = SWAP16(0x4527);
	uint16_t calibration_value = SWAP16(2048);
	int ret = 0;

	ret = hf_i2c_reg_write_block(&hi2c3, INA226_12V_ADDR, INA2XX_CONFIG, (uint8_t *)&default_cfg, 2);
	if (ret)
	{
		printf("init ina226 error1\n");
		return -1;
	}
	ret = hf_i2c_reg_write_block(&hi2c3, INA226_12V_ADDR, INA2XX_CALIBRATION, (uint8_t *)&calibration_value, 2);
	if (ret)
	{
		printf("init ina226 error2\n");
		return -1;
	}

	return 0;
}

int get_board_power(uint32_t *volt, uint32_t *curr, uint32_t *power)
{
	uint32_t reg_bus = 0x0;
	uint32_t reg_power = 0x0;
	uint32_t reg_curr = 0x0;
	int ret = 0;
	static int is_first = 1;

	if (1 == is_first)
	{
		/*Write failure, not find wrong reason*/
		ret = ina226_init();
		if (ret)
		{
			return -1;
		}
		is_first = 0;
	}
	ret = hf_i2c_reg_read_block(&hi2c3, INA226_12V_ADDR, INA2XX_BUS_VOLTAGE, (uint8_t *)&reg_bus, 2);
	if (ret)
	{
		printf("get board volt error\n");
		return -1;
	}
	ret = hf_i2c_reg_read_block(&hi2c3, INA226_12V_ADDR, INA2XX_POWER, (uint8_t *)&reg_power, 2);
	if (ret)
	{
		printf("get board power error\n");
		return -1;
	}
	ret = hf_i2c_reg_read_block(&hi2c3, INA226_12V_ADDR, INA2XX_CURRENT, (uint8_t *)&reg_curr, 2);
	if (ret)
	{
		printf("get board current error\n");
		return -1;
	}
	reg_bus = SWAP16(reg_bus);
	reg_curr = SWAP16(reg_curr);
	reg_power = SWAP16(reg_power);

	*volt = reg_bus * INA226_BUS_LSB;
	*volt = DIV_ROUND_CLOSEST(*volt, 1000);

	*power = reg_power * INA226_CURRENT_LSB * INA226_POWER_LSB_FACTOR;
	*curr = reg_curr * INA226_CURRENT_LSB;
	*curr = DIV_ROUND_CLOSEST(*curr, 1000);

	return 0;
}

int get_som_power(uint32_t *volt, uint32_t *curr, uint32_t *power)
{
	uint32_t reg_bus = 0x0;
	uint32_t reg_power = 0x0;
	uint32_t reg_curr = 0x0;
	int ret = 0;
	static int is_first = 1;
	uint8_t act_val = 0;
	uint8_t is_neg = 0;
	uint8_t ctrl_value = 0x8;

	taskENTER_CRITICAL();
	if (1 == is_first)
	{
		/*Write failure, not find wrong reason*/
		hf_i2c_reg_write(&hi2c3, PAC1934_ADDR, PAC193X_CMD_CTRL, &ctrl_value);

		if (ret)
		{
			ret = -1;
			goto out;
		}
		is_first = 0;
	}

	hf_i2c_reg_write_block(&hi2c3, PAC1934_ADDR, PAC193X_CMD_REFRESH_V, (uint8_t *)&ctrl_value, 0);
	osDelay(1);
	ret = hf_i2c_reg_read(&hi2c3, PAC1934_ADDR, PAC193X_CMD_NEG_PWR_ACT, &act_val);
	if (ret)
	{
		printf("get PAC1934 act_val error\n");
		ret = -1;
		goto out;
	}
	ret = hf_i2c_reg_read_block(&hi2c3, PAC1934_ADDR, PAC193X_CMD_VBUS1, (uint8_t *)&reg_bus, 2);
	if (ret)
	{
		printf("get PAC1934 voltage error\n");
		ret = -1;
		goto out;
	}
	reg_bus = reg_bus & 0XFFFF;
	reg_bus = SWAP16(reg_bus);
	if (0x1 == ((act_val >> 3) & 0x1))
	{
		*volt = reg_bus * 1000 / 1024;
		is_neg = 1;
	}
	else
	{
		*volt = reg_bus * 1000 / 2048;
	}
	ret = hf_i2c_reg_read_block(&hi2c3, PAC1934_ADDR, PAC193X_CMD_VSENSE1, (uint8_t *)&reg_curr, 2);
	reg_curr = SWAP16(reg_curr);
	if (ret)
	{
		printf("get PAC1934 current error\n");
		ret = -1;
		goto out;
	}
	reg_curr = reg_curr & 0XFFFF;
	if (0x1 == ((act_val >> 7) & 0x1))
	{
		*curr = reg_curr * PAC193X_COSTANT_CURRENT_M / (32768 * PAC193X_SHUNT_RESISTOR_M);
		is_neg = 1;
	}
	else
	{
		*curr = reg_curr * PAC193X_COSTANT_CURRENT_M / (65536 * PAC193X_SHUNT_RESISTOR_M);
	}
	ret = hf_i2c_reg_read_block(&hi2c3, PAC1934_ADDR, PAC193X_CMD_VPOWER1, (uint8_t *)&reg_power, 4);
	reg_power = SWAP32(reg_power);
	if (ret)
	{
		printf("get PAC1934 power error\n");
		ret = -1;
		goto out;
	}
	reg_power = reg_power >> 4;
	if (1 == is_neg)
	{
		*power = reg_power * PAC193X_COSTANT_PWR_M / (PAC193X_SHUNT_RESISTOR_M * 134217728ULL);
	}
	else
	{
		*power = reg_power * PAC193X_COSTANT_PWR_M / (PAC193X_SHUNT_RESISTOR_M * 268435456ULL);
	}

out:
	taskEXIT_CRITICAL();
	return ret;
}
