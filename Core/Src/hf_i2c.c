#include "stm32f4xx_hal.h"
#include "main.h"
#include <stdio.h>

int hf_i2c_reg_write(I2C_HandleTypeDef *hi2c, uint8_t slave_addr,
					 uint8_t reg_addr, uint8_t *data_ptr)
{
	HAL_StatusTypeDef status = HAL_OK;

	status = HAL_I2C_Mem_Write(hi2c, slave_addr, reg_addr, I2C_MEMADD_SIZE_8BIT,
							   data_ptr, 0x1, 0xff);
	if (status != HAL_OK) {
		printf("I2Cx_write_Error(%x) reg %x; status %x\r\n", slave_addr, reg_addr, status);
	}
	while (HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY);
	while (HAL_I2C_IsDeviceReady(hi2c, slave_addr, 0xff, 0xff) == HAL_TIMEOUT);
	while (HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY);
	return status;
}

int hf_i2c_reg_read(I2C_HandleTypeDef *hi2c, uint8_t slave_addr,
					uint8_t reg_addr, uint8_t *data_ptr)
{
	HAL_StatusTypeDef status = HAL_OK;
	while (HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY);
	status = HAL_I2C_Mem_Read(hi2c, slave_addr, reg_addr, I2C_MEMADD_SIZE_8BIT,
							  data_ptr, 0x1, 0xff);
	if (status != HAL_OK)
		printf("I2Cx_read_Error(%x) reg %x; status %x\r\n", slave_addr, reg_addr, status);
	while (HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY);
	while (HAL_I2C_IsDeviceReady(hi2c, slave_addr, 0xff, 0xff) == HAL_TIMEOUT);
	while (HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY);
	return status;
}

int hf_i2c_mem_read(I2C_HandleTypeDef *hi2c, uint8_t slave_addr,
					uint8_t reg_addr, uint8_t *data_ptr, uint32_t len)
{
	HAL_StatusTypeDef status = HAL_OK;

	while (HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY);
	status = HAL_I2C_Mem_Read(hi2c, slave_addr, reg_addr, I2C_MEMADD_SIZE_8BIT,
					data_ptr, len, 1000);
	if (status != HAL_OK)
		printf("I2Cx_read_Error(%x);\r\n", slave_addr);
	while (HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY);
	while (HAL_I2C_IsDeviceReady(hi2c, slave_addr, 0xff, 0xff) == HAL_TIMEOUT);
	return status;
}

int hf_i2c_mem_write(I2C_HandleTypeDef *hi2c, uint8_t slave_addr,
					  uint8_t reg_addr, uint8_t *data_ptr, uint32_t len)
{
	int i, j = 0, offset;
	int unaligned_bytes = 0, aligned_bytes; // the reg_addr offset that is not aligned with 8Bytes
	HAL_StatusTypeDef status = HAL_OK;
	printf("slave_addr %x, reg_addr %x len %ld\n",slave_addr, reg_addr, len);
	if (hi2c->Instance == I2C1)
		HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_RESET);

	// unaligned addr
	unaligned_bytes = ALIGN(reg_addr, 8) - reg_addr;
	unaligned_bytes = MIN(unaligned_bytes, len);
	for (i = 0; i < unaligned_bytes; i++) {
		status = HAL_I2C_Mem_Write(hi2c, slave_addr, reg_addr + i,
								I2C_MEMADD_SIZE_8BIT, data_ptr + i, 1,
								1000);
		if (status != HAL_OK)
			printf("I2Cx_write_Error(slave_addr:%x, errcode:%d) %d;\r\n", slave_addr, status, j);
		while (HAL_I2C_IsDeviceReady(hi2c, slave_addr, 0xff, 0xff) == HAL_TIMEOUT);
		while (HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY);
	}
	offset = i;
	// 8 bytes page write
	aligned_bytes = ((len - unaligned_bytes) / 8) * 8;
	for (i = 0; i < aligned_bytes;) {
		status = HAL_I2C_Mem_Write(hi2c, slave_addr, reg_addr + offset + i,
								I2C_MEMADD_SIZE_8BIT, data_ptr + offset + i, 8,
								1000);
		if (status != HAL_OK)
			printf("I2Cx_write_Error(slave_addr:%x, errcode:%d) %d;\r\n", slave_addr, status, j);
		while (HAL_I2C_IsDeviceReady(hi2c, slave_addr, 0xff, 0xff) == HAL_TIMEOUT);
		while (HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY);
		i = i + 8;
	}
	offset = offset + i;
	// the left bytes
	unaligned_bytes = len - unaligned_bytes - aligned_bytes;
	for (i = 0; i < unaligned_bytes; i++) {
		status = HAL_I2C_Mem_Write(hi2c, slave_addr, reg_addr + offset + i,
								I2C_MEMADD_SIZE_8BIT, data_ptr + offset + i, 1,
								1000);
		if (status != HAL_OK)
			printf("I2Cx_write_Error(slave_addr:%x, errcode:%d) %d;\r\n", slave_addr, status, j);
		while (HAL_I2C_IsDeviceReady(hi2c, slave_addr, 0xff, 0xff) == HAL_TIMEOUT);
		while (HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY);
	}

	if (hi2c->Instance == I2C1)
		HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_SET);
	return status;
}

void AT24C_eeprom_rw(void)
{
	#define AT24C_ADDR (0x50<<1)

    #define BufferSize 256
    uint8_t WriteBuffer[BufferSize],ReadBuffer[BufferSize];
    uint16_t i;
    for(i=0; i<BufferSize; i++)
      WriteBuffer[i]=i+0xf0;    /* WriteBuffer init */
    hf_i2c_mem_write(&hi2c1, AT24C_ADDR,
					  0, WriteBuffer, BufferSize);
    hf_i2c_mem_read(&hi2c1, AT24C_ADDR,
					0, ReadBuffer, BufferSize);
    for(i=0; i<BufferSize; i++)
      printf("0x%02X  ",ReadBuffer[i]);
	printf("read_finash\n");
}