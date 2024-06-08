/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32_CRC32_H
#define __STM32_CRC32_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>

/* types ------------------------------------------------------------*/

uint32_t GetCRC32(uint8_t* pbuffer, uint32_t length);
uint32_t sifive_crc32(const uint8_t *p, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __HF_COMMON_H */
