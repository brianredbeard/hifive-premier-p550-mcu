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
* Author: linmin@eswincomputing.com
* 
* File Description: sifive_crc32.h
*  Declaration of the crc32 API
************************************************************************************/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32_CRC32_H
#define __STM32_CRC32_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>

/* types ------------------------------------------------------------*/

uint32_t sifive_crc32(const uint8_t *p, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __HF_COMMON_H */
