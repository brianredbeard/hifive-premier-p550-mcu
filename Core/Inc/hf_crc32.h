// SPDX-License-Identifier: GPL-2.0-only
/*
 * Declaration of the crc32 API
 *
 * Copyright 2024 Beijing ESWIN Computing Technology Co., Ltd.
 *   Authors:
 *    LinMin<linmin@eswincomputing.com>
 *
 */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HF_CRC32_H
#define __HF_CRC32_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>

/* types ------------------------------------------------------------*/

uint32_t hf_crc32(const uint8_t *p, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __HF_COMMON_H */
