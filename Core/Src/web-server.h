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
* Author: yuanjunhui@eswincomputing.com
* 
* File Description: web-server.h
*  Header file of the BMC web server
************************************************************************************/
#ifndef __HTTPSERVER_NETCONN_H__
#define __HTTPSERVER_NETCONN_H__

#define TRUE 1
#define FALSE 0
#define LOCAL_PORT 80

#define LED1_ON  do {printf("LED ON\n");} while(0)
#define LED1_OFF  do {printf("LED OFF\n");} while(0)

typedef struct {
	uint32_t magic;
	uint8_t version;
	uint16_t id;
	uint8_t pcb;
	uint8_t bom_revision;
	uint8_t bom_variant;
	uint8_t sn[18];		// 18 bytes of serial number, excluding string terminator
	uint8_t status;
	uint32_t crc;
} __attribute__((packed)) som_info;

typedef struct  {
    char ipaddr[16];    // IP地址，假设为IPv4，足够容纳xxx.xxx.xxx.xxx
    char macaddr[18];   // MAC地址，足够容纳xx:xx:xx:xx:xx:xx
    char subnetwork[16]; // 子网掩码
    char gateway[16];    // 网关地址
} NETInfo;

typedef struct {
	int cpu_temp;
	int npu_temp;
	int fan_speed;
} PVTInfo;

void httpserver_init(void);

NETInfo get_net_info(void);

#endif /* __HTTPSERVER_NETCONN_H__ */