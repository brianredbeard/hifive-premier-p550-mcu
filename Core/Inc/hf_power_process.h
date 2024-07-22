// SPDX-License-Identifier: GPL-2.0-only
/*
 * Header file for the hf_power_process.c
 *
 * Copyright 2024 Beijing ESWIN Computing Technology Co., Ltd.
 *   Authors:
 *    XuXiang<xuxiang@eswincomputing.com>
 *
 */
#ifndef __HF_POWER_PROCESS_H
#define __HF_POWER_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif
#include "hf_common.h"

int get_board_power(uint32_t *volt, uint32_t *curr, uint32_t *power);
int get_som_power(uint32_t *volt, uint32_t *curr, uint32_t *power);
#endif /* __HF_I2C_H */