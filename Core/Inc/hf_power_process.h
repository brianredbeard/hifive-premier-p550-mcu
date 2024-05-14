#ifndef __HF_POWER_PROCESS_H
#define __HF_POWER_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif
#include "hf_common.h"

int get_board_power(uint32_t *volt, uint32_t *curr, uint32_t *power);
int get_som_power(uint32_t *volt, uint32_t *curr, uint32_t *power);
#endif /* __HF_I2C_H */