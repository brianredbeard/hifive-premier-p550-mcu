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
#ifndef __TELNET_SOM_CONSOLE_H
#define __TELNET_SOM_CONSOLE_H

extern char cTelSomConcRxData;
extern QueueHandle_t telSomConQueueRxHandle;
extern UART_HandleTypeDef *pTelSomConUartDevHandle;

void telenet_receiver_callback(uint8_t* buff, uint16_t len );
void telSomConsEnableRxInterrupt(void);
BaseType_t telnetSomConsoleInit(uint16_t usStackSize, UBaseType_t uxPriority, UART_HandleTypeDef *pxUartHandle);

#endif
