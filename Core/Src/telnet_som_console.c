// SPDX-License-Identifier: GPL-2.0-only
/*
 * Transfer the som uart console through telenet
 *
 * Copyright 2024 Beijing ESWIN Computing Technology Co., Ltd.
 *   Authors:
 *    LinMin<linmin@eswincomputing.com>
 *
 */
#include "hf_common.h"
#include "telnet_server.h"

#define SOM_TEL_CON_MAX_RX_QUEUE_LEN                        300

char cTelSomConcRxData;
QueueHandle_t telSomConQueueRxHandle = NULL;
UART_HandleTypeDef *pTelSomConUartDevHandle;

/**
* @brief Write to UART TX
* @param *buff buffer to be written.
* @retval HAL status
*/
static HAL_StatusTypeDef vTelSomConsoleWrite(uint8_t *buff, size_t len)
{
    HAL_StatusTypeDef status;

    if (pTelSomConUartDevHandle == NULL || *buff == '\0' || len < 1)
    {
        return HAL_ERROR;
    }

    status = HAL_UART_Transmit(pTelSomConUartDevHandle, (uint8_t *)buff, len, portMAX_DELAY);
    if (status != HAL_OK)
    {
        return HAL_ERROR;
    }
    return status;
}

void telenet_receiver_callback(uint8_t* buff, uint16_t len )
{
    // printf("len[%d],hex:%x,%x,%x:%s\n", len, buff[0], buff[1], buff[2], buff);
    /* Forward the telnet data to SOM via uart6 tx port */
    vTelSomConsoleWrite(buff, len);
}

/**
* @brief Reads from UART RX buffer. Reads one bye at the time.
* @param *cReadChar pointer to where data will be stored.
* @retval FreeRTOS status
*/
static BaseType_t telSomConsoleRead(uint8_t *cReadChar, size_t xLen)
{
    BaseType_t xRetVal = pdFALSE;

    if (telSomConQueueRxHandle == NULL || cReadChar == NULL)
    {
        return xRetVal;
    }

    /* Block until the there is input from the user */
    return xQueueReceive(telSomConQueueRxHandle, cReadChar, portMAX_DELAY);
}

/**
* @brief Enables UART RX reception.
* @param void
* @retval void
*/
void telSomConsEnableRxInterrupt(void)
{
    if (pTelSomConUartDevHandle == NULL)
    {
        return;
    }
    /* UART Rx IT is enabled by reading a character */
    HAL_UART_Receive_IT(pTelSomConUartDevHandle,(uint8_t*)&cTelSomConcRxData, 1);
}

/**
* @brief Task to handle user commands via serial communication.
* @param *pvParams Data passed at task creation.
* @retval void
*/
void vTaskTelenetSomConsole(void *pvParams)
{
    char cReadCh = '\0';

    /* Create a queue to store characters from RX ISR */
    telSomConQueueRxHandle = xQueueCreate(SOM_TEL_CON_MAX_RX_QUEUE_LEN, sizeof(char));
    if (telSomConQueueRxHandle == NULL)
    {
        goto out_task_console;
    }
    telSomConsEnableRxInterrupt();
 
    while(1)
    {
        /* Block until there is a new character in RX buffer */
        telSomConsoleRead((uint8_t*)(&cReadCh), sizeof(cReadCh));
        /* Forward the som debug data from uart6 rx port to the telnet client */
        telnet_transmit((uint8_t *)&cReadCh, sizeof(cReadCh));
    }

out_task_console:
    if (telSomConQueueRxHandle)
    {
        vQueueDelete(telSomConQueueRxHandle);
    }
    vTaskDelete(NULL);
}

/**
* @brief Initialize the console by registering all commands and creating a task.
* @param usStackSize Task console stack size
* @param uxPriority Task console priority
* @param *pxUartHandle Pointer for uart handle.
* @retval FreeRTOS status
*/
BaseType_t telnetSomConsoleInit(uint16_t usStackSize, UBaseType_t uxPriority, UART_HandleTypeDef *pxUartHandle)
{
    if (pxUartHandle == NULL)
    {
        return pdFALSE;
    }
    pTelSomConUartDevHandle = pxUartHandle;

    return xTaskCreate(vTaskTelenetSomConsole,"TELNET_SOM_CLI", usStackSize, NULL, uxPriority, NULL);
}

