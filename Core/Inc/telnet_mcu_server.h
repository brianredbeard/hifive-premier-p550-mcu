/*
 * MIT License
 *
 * Copyright (c) 2022 André Cascadan and Bruno Augusto Casu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 * This file is part of the lwIP based telnet server.
 *
 */

#ifndef __TELNET_MCU_H__
#define __TELNET_MCU_H__
#include "FreeRTOS.h"
#include "semphr.h"

#define TELNET_NO_OPERATION_TIME 300000  /*unit is ms*/
#define MAX_TELNET_CONN_NUM      3

typedef struct
{
    struct netconn *s_conn;
    uint8_t connections;   /*always less than MAX_CONN_NUM*/
    SemaphoreHandle_t s_mutex;
} telnet_server_t;
/**
 * @brief Create a new telnet server in a defined TCP port
 *
 * @param port               Number of the TCP connection Port
 */
void telnet_mcu_create( uint16_t port);


/**
 * @brief A wrapper function that transmit data to client from port 25.
 *
 * @param data          A pointer to the data buffer that needs to be sent.
 * @param inst_ptr      Telnet console connect instance pointer.
 * @retval              How much data was actually sent.
 */
void telnet_mcu_transmit(const char *data, telnet_console_t* inst_ptr);

#endif /* __TELNET_H__ */
