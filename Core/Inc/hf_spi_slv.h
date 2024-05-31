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

int es_spi_write(uint8_t *buf, uint64_t addr, int len);
int es_spi_read(uint8_t *dst, uint64_t src, int len);
int eswin_rx(uint8_t *rcvBuf, uint64_t addr, int len);

#ifdef __cplusplus
}
#endif

#endif /* __HF_SPI_SLV_H */
