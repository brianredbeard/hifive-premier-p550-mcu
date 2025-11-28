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

#include <stdlib.h>
#include <string.h>
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "lwip/api.h"
#include "telnet_server.h"
#include "telnet_mcu_console.h"
#include "telnet_mcu_server.h"


static const char *pcConnFaile = "The number of connections exceeds the limit of 3.\r\n";
/*server connection define.*/
telnet_server_t telnet_server;

// Task and attributes
static const osThreadAttr_t server_listening_attributes = {
  .name = "TelnetListen",
  .attr_bits = osThreadDetached,
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};

static const osThreadAttr_t server_senssion_attributes = {
  .name = "TelnetSession",
  .attr_bits = osThreadDetached,
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = (256) * 6
};

// Task functions
static void telnet_mcu_server_listening (void *arg);
static void telnet_mcu_server_senssion (void *arg);

/**
 * @brief Create a new telnet server in a defined TCP port
 *
 * @param port               Number of the TCP connection Port
 */
void telnet_mcu_create( uint16_t port)
{
	err_t err;
create_conn_again:
	vTaskDelay( 1000 ); // Delay for a while to make sure connection has been deleted eventually
	telnet_server.s_conn = netconn_new(NETCONN_TCP);
	if(telnet_server.s_conn == NULL ) {
		printf("%s: failed to create TCP connection!!!\n", __func__);
		goto create_conn_again;
	}

	err = netconn_bind(telnet_server.s_conn, NULL, port); 
	if ( err != ERR_OK ) {
		printf("%s: err %d, failed to bind TCP!!!\n", __func__, err);
		netconn_delete(telnet_server.s_conn);
		goto create_conn_again;
	}

	netconn_listen(telnet_server.s_conn);

	osThreadNew(telnet_mcu_server_listening, NULL, &server_listening_attributes);
}

/*
 * Telnet server connection listening task ()
 *
 * Listen for connection requests from client telnet ports.
 * Once connection request arrives, it is accepted and an instance is created.
 *
 */
static void telnet_mcu_server_listening (void *arg)
{
	struct netconn *newconn;
	err_t accept_err;
	telnet_server.s_mutex = xSemaphoreCreateMutex();
	telnet_server.connections = 0;
  	for(;;)
  	{
		/* Keep listening until accept the next connection. */
		accept_err = netconn_accept(telnet_server.s_conn, &newconn);
		if( accept_err == ERR_OK )
		{
			if(telnet_server.connections >= MAX_TELNET_CONN_NUM) {
				netconn_write(newconn, pcConnFaile, strlen(pcConnFaile), NETCONN_COPY);
				vTaskDelay(1000); 
				netconn_close (newconn);
				netconn_delete(newconn);
				continue;
			}

			telnet_console_t *c_instance = 
					( telnet_console_t*) pvPortMalloc( sizeof(telnet_console_t) );
			if( c_instance != NULL ) {
				c_instance->s_mutex = telnet_server.s_mutex;
				c_instance->newconn = newconn;

				if(osThreadNew(telnet_mcu_server_senssion, c_instance, 
						&server_senssion_attributes) == NULL) {
					netconn_close (newconn);
					netconn_delete(newconn);
					vPortFree(c_instance);
					continue;
				}
			} else {
				netconn_close (newconn);
				netconn_delete(newconn);
				continue;
			}

			xSemaphoreTake(telnet_server.s_mutex, portMAX_DELAY);
			telnet_server.connections ++;
			xSemaphoreGive(telnet_server.s_mutex);
		}
  	}
}

/*
 * telnet mcu server session task.
 *
 * This task waits and process input bytes from the client.
 *
 */
static void telnet_mcu_server_senssion(void *arg)
{
	struct netbuf *rx_netbuf;
	void *rx_data;
	uint16_t rx_data_len;
	err_t recv_err;
	telnet_console_t *instance = (telnet_console_t *)arg;

	telenet_mcu_console_init(instance);
	netconn_set_recvtimeout(instance->newconn, TELNET_NO_OPERATION_TIME);

	for(;;)
	{
		// Iteratively reads all the available data
		recv_err = netconn_recv(instance->newconn, &rx_netbuf);
		if ( recv_err == ERR_OK)
		{
			if(netbuf_data(rx_netbuf, &rx_data, &rx_data_len) == ERR_OK ) {
				if((rx_data_len > 0) && (rx_data_len < MAX_PC_IN_STR_LEN))
					telenet_mcu_console(rx_data, rx_data_len, instance);
				else
					printf("Telnet console get a invalid PKT! len = %d\n", rx_data_len);
			}
			netbuf_delete( rx_netbuf );
		} else {
			/*If no client commands are received within a certain amount of time 
			  or the client closes the connection,
			  then the local server resources should be released.*/
			printf("net connection closed by err = %d\n", recv_err);
			xSemaphoreTake(telnet_server.s_mutex, portMAX_DELAY);
			telnet_server.connections --;
			xSemaphoreGive(telnet_server.s_mutex);

			netconn_close (instance->newconn);
			netconn_delete(instance->newconn);
			vPortFree(instance);
			osThreadExit();
		}
	}
}

/**
 * @brief A wrapper function that transmit data to client from port 25.
 *
 * @param data          A pointer to the data buffer that needs to be sent.
 * @param inst_ptr      Telnet console instance.
 * @retval              void.
 */
void telnet_mcu_transmit(const char *data, telnet_console_t* inst_ptr)
{
	err_t err = ERR_ARG;
	size_t data_len;
	data_len = strlen(data);
	if (data_len < MAX_PC_OUT_STR_LEN) {
		err = netconn_write(inst_ptr->newconn, data, data_len, NETCONN_COPY);
	}
	if (err != ERR_OK) {
		printf("Telnet console send data failed! err = %d \n",err);
	}
}
