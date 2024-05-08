#ifndef __HF_SPI_SLV_H
#define __HF_SPI_SLV_H

#ifdef __cplusplus
 extern "C" {
#endif

#define SPI2_FLASH_CS_LOW()                                                  \
	do {                                                                     \
		HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_RESET); \
	} while (0);

#define SPI2_FLASH_CS_HIGH()                                               \
	do {                                                                   \
		HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_SET); \
	} while (0);

int es_spi_write(unsigned char *buf, unsigned long addr, int len);
int es_spi_read(unsigned char *dst, unsigned long src, int len);

#ifdef __cplusplus
}
#endif

#endif /* __HF_SPI_SLV_H */
