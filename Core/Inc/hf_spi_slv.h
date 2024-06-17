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
* File Description: hf_spi_slv.h
*  Header file for the hf_spi_slv.c
************************************************************************************/
#ifndef __HF_SPI_SLV_H
#define __HF_SPI_SLV_H

#ifdef __cplusplus
 extern "C" {
#endif

#define SPI2_MASTER_CS_LOW()                                                  \
	do {                                                                     \
		HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_RESET); \
	} while (0);

#define SPI2_MASTER_CS_HIGH()                                               \
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
