// SPDX-License-Identifier: GPL-2.0-only
/*
 * Transfer the mcu console through telenet
 *
 * Copyright 2024 Beijing ESWIN Computing Technology Co., Ltd.
 *   Authors:
 *    GengZonglin<gengzonglin@eswincomputing.com>
 *
 */
#include <stdlib.h>
#include <string.h>
#include "hf_common.h"
#include "telnet_server.h"
#include "telnet_mcu_console.h"
#include "telnet_mcu_server.h"
#include "console.h"
#include "FreeRTOS_CLI.h"

#define TEMP_LOGIN_USER_NAME_SPACE  (inst_ptr->pc_prev_input_string)

static const char *pcWelcomeMsg = "Welcome to the console. Please enter your login username and password.\r\n";

static const char *pcLoginUsername = "user: ";
static const char *pcLoginPassword = "password: ";
static const char *pcLoginFail = "Invalid username or password. Please try again.\r\n";
static const char *pcLoginSucess = "Login success. Enter 'help' to view a list of available commands.\r\n";

static const char *prvpcPrompt = "\r\n#cmd: ";

static void telenet_mcu_console_username(telnet_console_t* inst_ptr)
{
    if(inst_ptr->pc_input_char_Idx != 0) {
        /* use prev input buffer to store login user name. */
        strncpy(TEMP_LOGIN_USER_NAME_SPACE, inst_ptr->pc_Input_string, MAX_PC_USERNAME_PSW_LEN);
        telnet_mcu_transmit("\r\n", inst_ptr);
        telnet_mcu_transmit(pcLoginPassword, inst_ptr);
        inst_ptr->mcu_console_status = MCU_CONSOLE_STATUS_LOGIN_PASSWORD;
    } else {
        // telnet_mcu_transmit("\r\n");
        telnet_mcu_transmit(pcLoginUsername, inst_ptr);
        inst_ptr->mcu_console_status = MCU_CONSOLE_STATUS_LOGIN_NAME;
    }
}

static void telenet_mcu_console_login_verify(telnet_console_t* inst_ptr)
{
    BaseType_t loginSuccess;
    char loginPassword[MAX_PC_USERNAME_PSW_LEN];
    char sys_username[MAX_PC_USERNAME_PSW_LEN]="admin";
	char sys_password[MAX_PC_USERNAME_PSW_LEN]="123456";
    if(inst_ptr->pc_input_char_Idx != 0) {
        strncpy(loginPassword, inst_ptr->pc_Input_string, MAX_PC_USERNAME_PSW_LEN);

        es_get_username_password(sys_username, sys_password);
        loginSuccess = (strcmp(TEMP_LOGIN_USER_NAME_SPACE, sys_username) == 0 && 
                        strcmp(loginPassword, sys_password) == 0)?0:1;
        if(loginSuccess) {
            inst_ptr->mcu_console_status = MCU_CONSOLE_STATUS_LOGIN_NAME;
            telnet_mcu_transmit("\r\n", inst_ptr);
            telnet_mcu_transmit(pcLoginFail, inst_ptr);
            telnet_mcu_transmit(pcLoginUsername, inst_ptr);
        } else {
            inst_ptr->mcu_console_status = MCU_CONSOLE_STATUS_VERIFIED;
            telnet_mcu_transmit("\r\n", inst_ptr);
            telnet_mcu_transmit(pcLoginSucess, inst_ptr);
            telnet_mcu_transmit(prvpcPrompt, inst_ptr);
        }
    } else {
        // telnet_mcu_transmit("\r\n");
        telnet_mcu_transmit(pcLoginPassword, inst_ptr);
        inst_ptr->mcu_console_status = MCU_CONSOLE_STATUS_LOGIN_PASSWORD;
    }
}

static void telenet_mcu_console_normal(telnet_console_t* inst_ptr)
{
    BaseType_t more_data_to_process;

    if(inst_ptr->pc_input_char_Idx != 0)
    {
        strncpy(inst_ptr->pc_prev_input_string, inst_ptr->pc_Input_string, MAX_PC_IN_STR_LEN);
        FreeRTOS_CLILock();
        do
        {
            more_data_to_process = FreeRTOS_CLIProcessCommand
                                (
                                    inst_ptr->pc_Input_string,    /* Command string*/
                                    inst_ptr->pc_output_string,   /* Output buffer */
                                    MAX_PC_OUT_STR_LEN   /* Output buffer size */
                                );
            telnet_mcu_transmit(inst_ptr->pc_output_string, inst_ptr);
        } while (more_data_to_process != pdFALSE);
        FreeRTOS_CLIUnLock();
    }

    telnet_mcu_transmit(prvpcPrompt, inst_ptr);
}

/**
* @brief mcu console command processing.
*
* @param *inst_ptr     Telnet console connect instance pointer.
* @retval void
*/
void telenet_mcu_console_processing(telnet_console_t* inst_ptr)
{
    switch (inst_ptr->mcu_console_status)
    {
        case MCU_CONSOLE_STATUS_LOGIN_NAME:
            telenet_mcu_console_username(inst_ptr);
            break;
        case MCU_CONSOLE_STATUS_LOGIN_PASSWORD:
            telenet_mcu_console_login_verify(inst_ptr);
            break;
        case MCU_CONSOLE_STATUS_VERIFIED:
            telenet_mcu_console_normal(inst_ptr);
            break;
        default:
            break;
    }
    inst_ptr->pc_input_char_Idx = 0;
    memset(inst_ptr->pc_Input_string, 0x00, MAX_PC_IN_STR_LEN);
    memset(inst_ptr->pc_output_string, 0x00, MAX_PC_OUT_STR_LEN);
}

/**
* @brief Task to handle user commands via serial communication.
*
* @param *buff        Store the command data that get from netconn.
* @param *buff        Command data length.
* @param *inst_ptr    Telnet console connect instance pointer.
* @retval void
*/
void telenet_mcu_console(uint8_t* buff, uint16_t buflen, telnet_console_t* inst_ptr)
{
    char read_char = '\0';
    uint8_t buf_char_idx = 0;

    while(buf_char_idx < buflen)
    {
        read_char = buff[buf_char_idx];
        buf_char_idx++;
        switch (read_char)
        {
            case ASCII_CR:
                /*ingore line feed. case ASCII_LF:*/
                telenet_mcu_console_processing(inst_ptr);
                break;
            case ASCII_FORM_FEED:
                telnet_mcu_transmit("\x1b[2J\x1b[0;0H", inst_ptr);
                telnet_mcu_transmit("\n", inst_ptr);
                telnet_mcu_transmit(prvpcPrompt, inst_ptr);
                break;
            case ASCII_CTRL_PLUS_C:
            case ASCII_CTRL_PLUS_A:
                inst_ptr->pc_input_char_Idx = 0;
                memset(inst_ptr->pc_Input_string, 0x00, MAX_PC_IN_STR_LEN);
                telnet_mcu_transmit("\n", inst_ptr);
                telnet_mcu_transmit(prvpcPrompt, inst_ptr);
                break;
            case ASCII_DEL:
            case ASCII_NACK:
            case ASCII_BACKSPACE:
                if (inst_ptr->pc_input_char_Idx > 0)
                {
                    inst_ptr->pc_input_char_Idx--;
                    inst_ptr->pc_Input_string[inst_ptr->pc_input_char_Idx] = '\0';
                    telnet_mcu_transmit("\b \b", inst_ptr);
                }
                break;
            case ASCII_SPACE:
                /*ingore first SPACE*/
                if (inst_ptr->pc_input_char_Idx > 0)
                {
                    if (inst_ptr->pc_input_char_Idx < (MAX_PC_IN_STR_LEN - 1 ))
                    {
                        inst_ptr->pc_Input_string[inst_ptr->pc_input_char_Idx] = read_char;
                        inst_ptr->pc_input_char_Idx++;
                    }
                }
                break;
            case ASCII_TAB:
                while (inst_ptr->pc_input_char_Idx)
                {
                    inst_ptr->pc_input_char_Idx--;
                    telnet_mcu_transmit("\b \b", inst_ptr);
                }
                strncpy(inst_ptr->pc_Input_string, inst_ptr->pc_prev_input_string, MAX_PC_IN_STR_LEN);
                inst_ptr->pc_input_char_Idx = (unsigned char)strlen(inst_ptr->pc_Input_string);
                telnet_mcu_transmit(inst_ptr->pc_Input_string, inst_ptr);
                break;
            default:
                /* Check if read character is between [Space] and [~] in ASCII table */
                if (inst_ptr->pc_input_char_Idx < (MAX_PC_IN_STR_LEN - 1 ) && (read_char > 32 && read_char <= 126))
                {
                    inst_ptr->pc_Input_string[inst_ptr->pc_input_char_Idx] = read_char;
                    inst_ptr->pc_input_char_Idx++;
                }
                break;
        }
    }
}

/**
* @brief Initialize the console variable values and print welcome message.
*
* @param *inst_ptr     Telnet console connect instance pointer.
* @retval void
*/
void telenet_mcu_console_init(telnet_console_t* inst_ptr)
{
    memset(inst_ptr->pc_Input_string, 0x00, MAX_PC_IN_STR_LEN);
    memset(inst_ptr->pc_prev_input_string, 0x00, MAX_PC_IN_STR_LEN);
    memset(inst_ptr->pc_output_string, 0x00, MAX_PC_OUT_STR_LEN);
    inst_ptr->pc_input_char_Idx = 0;

    inst_ptr->mcu_console_status = MCU_CONSOLE_STATUS_LOGIN_NAME;

    telnet_mcu_transmit("\r\n", inst_ptr);
    telnet_mcu_transmit(pcWelcomeMsg, inst_ptr);
    telnet_mcu_transmit(pcLoginUsername, inst_ptr);
}
