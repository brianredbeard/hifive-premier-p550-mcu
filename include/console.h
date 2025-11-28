/**
 ******************************************************************************
 * @file    console.h
 * @author  Aaron Escoboza, Github account: https://github.com/aaron-ev
 * @brief   Console header file: APIs to handle the console.
 ******************************************************************************
 */

#ifndef __CONSOLE__H
#define __CONSOLE__H

#include "FreeRTOS.h"

/* ASCII code definition */
#define ASCII_TAB                               '\t'  /* Tabulate              */
#define ASCII_CR                                '\r'  /* Carriage return       */
#define ASCII_LF                                '\n'  /* Line feed             */
#define ASCII_BACKSPACE                         '\b'  /* Back space            */
#define ASCII_FORM_FEED                         '\f'  /* Form feed             */
#define ASCII_DEL                               127   /* Delete                */
#define ASCII_CTRL_PLUS_C                         3   /* CTRL + C              */
#define ASCII_CTRL_PLUS_A                         1   /* CTRL + A              */
#define ASCII_NACK                               21   /* Negative acknowledge  */
#define ASCII_SPACE                              32   /* Space                 */

BaseType_t xbspConsoleInit(uint16_t usStackSize, UBaseType_t uxPriority, UART_HandleTypeDef *pxUartHandle);

#endif
