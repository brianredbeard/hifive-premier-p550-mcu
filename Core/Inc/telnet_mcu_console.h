// SPDX-License-Identifier: GPL-2.0-only
/*
 * Declaration of the crc32 API
 *
 * Copyright 2024 Beijing ESWIN Computing Technology Co., Ltd.
 *   Authors:
 *    GengZonglin<gengzonglin@eswincomputing.com>
 *
 */

#ifndef __TELNET_MCU_CONSOLE_H
#define __TELNET_MCU_CONSOLE_H
#include "FreeRTOS.h"
#include "semphr.h"

#define MAX_PC_IN_STR_LEN                          64
#define MAX_PC_OUT_STR_LEN                         600
#define MAX_PC_USERNAME_PSW_LEN                    MAX_PC_IN_STR_LEN
/*telnet console handle instance*/
typedef struct
{
    SemaphoreHandle_t s_mutex;
    struct netconn *newconn;

    char pc_Input_string[MAX_PC_IN_STR_LEN];
    char pc_prev_input_string[MAX_PC_IN_STR_LEN];
    char pc_output_string[MAX_PC_OUT_STR_LEN];

    uint8_t pc_input_char_Idx;

    enum
    {
        MCU_CONSOLE_STATUS_INIT = 0,
        MCU_CONSOLE_STATUS_LOGIN_NAME,
        MCU_CONSOLE_STATUS_LOGIN_PASSWORD,
        MCU_CONSOLE_STATUS_VERIFIED,
        MCU_CONSOLE_STATUS_CLOSING
    }mcu_console_status;

} telnet_console_t;

/**
* @brief Task to handle user commands via serial communication.
*
* @param *buff        Store the command data that get from netconn.
* @param *buff        Command data length.
 *@param *inst_ptr    Telnet console connect instance pointer.
* @retval void
*/
void telenet_mcu_console(uint8_t* buff, uint16_t buflen, telnet_console_t* inst_ptr);

/**
* @brief Initialize the console variable values and print welcome message.
*
* @param *inst_ptr    Telnet console connect instance pointer.
* @retval void
*/
void telenet_mcu_console_init(telnet_console_t* inst_ptr);

#endif
