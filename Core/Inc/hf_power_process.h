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
* File Description: hf_power_process.h
*  Header file for the hf_power_process.c
************************************************************************************/
#ifndef __HF_POWER_PROCESS_H
#define __HF_POWER_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif
#include "hf_common.h"

int get_board_power(uint32_t *volt, uint32_t *curr, uint32_t *power);
int get_som_power(uint32_t *volt, uint32_t *curr, uint32_t *power);
#endif /* __HF_I2C_H */