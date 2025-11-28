/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : LWIP.c
 * Description        : This file provides initialization code for LWIP
 *                      middleWare.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "lwip.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#if defined(__CC_ARM) /* MDK ARM Compiler */
#include "lwip/sio.h"
#endif /* MDK ARM Compiler */
#include "ethernetif.h"
#include <string.h>

/* USER CODE BEGIN 0 */
uint8_t lwip_dhcp_enable = 0;
/* USER CODE END 0 */
/* Private function prototypes -----------------------------------------------*/
static void ethernet_link_status_updated(struct netif *netif);
/* ETH Variables initialization ----------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN 1 */
extern uint8_t ip_address[4];
extern uint8_t netmask_address[4];
extern uint8_t getway_address[4];
/* USER CODE END 1 */

/* Variables Initialization */
struct netif gnetif;
ip4_addr_t ipaddr;
ip4_addr_t netmask;
ip4_addr_t gw;
#if LWIP_IPV6
ip6_addr_t ip6addr;
#endif

/* USER CODE BEGIN OS_THREAD_ATTR_CMSIS_RTOS_V2 */
#define INTERFACE_THREAD_STACK_SIZE (1024)
osThreadAttr_t attributes;
/* USER CODE END OS_THREAD_ATTR_CMSIS_RTOS_V2 */

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
/**
 * LwIP initialization function
 */
void MX_LWIP_Init(void) {
  /* IP addresses initialization */
  uint8_t IP_ADDRESS[4];
  uint8_t NETMASK_ADDRESS[4];
  uint8_t GATEWAY_ADDRESS[4];
  if (!lwip_dhcp_enable) {
    IP_ADDRESS[0] = ip_address[0];
    IP_ADDRESS[1] = ip_address[1];
    IP_ADDRESS[2] = ip_address[2];
    IP_ADDRESS[3] = ip_address[3];
    NETMASK_ADDRESS[0] = netmask_address[0];
    NETMASK_ADDRESS[1] = netmask_address[1];
    NETMASK_ADDRESS[2] = netmask_address[2];
    NETMASK_ADDRESS[3] = netmask_address[3];

    GATEWAY_ADDRESS[0] = getway_address[0];
    GATEWAY_ADDRESS[1] = getway_address[1];
    GATEWAY_ADDRESS[2] = getway_address[2];
    GATEWAY_ADDRESS[3] = getway_address[3];
  }
  /* USER CODE BEGIN IP_ADDRESSES */
  /* USER CODE END IP_ADDRESSES */

  /* Initilialize the LwIP stack with RTOS */
  tcpip_init(NULL, NULL);

  if (!lwip_dhcp_enable) {
    /* IP addresses initialization without DHCP (IPv4) */
    IP4_ADDR(&ipaddr, IP_ADDRESS[0], IP_ADDRESS[1], IP_ADDRESS[2],
             IP_ADDRESS[3]);
    IP4_ADDR(&netmask, NETMASK_ADDRESS[0], NETMASK_ADDRESS[1],
             NETMASK_ADDRESS[2], NETMASK_ADDRESS[3]);
    IP4_ADDR(&gw, GATEWAY_ADDRESS[0], GATEWAY_ADDRESS[1], GATEWAY_ADDRESS[2],
             GATEWAY_ADDRESS[3]);
  } else {
    /* IP addresses initialization to zero with DHCP (IPv4) */
    ipaddr.addr = 0;
    netmask.addr = 0;
    gw.addr = 0;
  }

  /* add the network interface (IPv4/IPv6) with RTOS */
  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init,
            &tcpip_input);

#if LWIP_IPV6
  /* Create IPv6 local address */
  netif_create_ip6_linklocal_address(&gnetif, 0);
  netif_ip6_addr_set_state(&gnetif, 0, IP6_ADDR_VALID);
  gnetif.ip6_autoconfig_enabled = 1;
#endif

  /* Registers the default network interface */
  netif_set_default(&gnetif);

  /* We must always bring the network interface up connection or not... */
  netif_set_up(&gnetif);

  /* Set the link callback function, this function is called on change of link
   * status*/
  netif_set_link_callback(&gnetif, ethernet_link_status_updated);

  /* Create the Ethernet link handler thread */
  /* USER CODE BEGIN H7_OS_THREAD_NEW_CMSIS_RTOS_V2 */
  memset(&attributes, 0x0, sizeof(osThreadAttr_t));
  attributes.name = "EthLink";
  attributes.stack_size = INTERFACE_THREAD_STACK_SIZE;
  attributes.priority = osPriorityBelowNormal;
  osThreadNew(ethernet_link_thread, &gnetif, &attributes);
  /* USER CODE END H7_OS_THREAD_NEW_CMSIS_RTOS_V2 */

/* Start DHCP negotiation for a network interface (IPv4) */
  if (lwip_dhcp_enable) {
    dhcp_start(&gnetif);
  }
  /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

#ifdef USE_OBSOLETE_USER_CODE_SECTION_4
/* Kept to help code migration. (See new 4_1, 4_2... sections) */
/* Avoid to use this user section which will become obsolete. */
/* USER CODE BEGIN 4 */
/* USER CODE END 4 */
#endif

/**
 * @brief  Notify the User about the network interface config status
 * @param  netif: the network interface
 * @retval None
 */
static void ethernet_link_status_updated(struct netif *netif) {
  if (netif_is_up(netif)) {
    /* USER CODE BEGIN 5 */
    printf("%s %d netif_is_up \n", __func__, __LINE__);
    /* USER CODE END 5 */
  } else /* netif is down */
  {
    /* USER CODE BEGIN 6 */
    printf("%s %d netif is down  \n", __func__, __LINE__);
    /* USER CODE END 6 */
  }
}

#if defined(__CC_ARM) /* MDK ARM Compiler */
/**
 * Opens a serial device for communication.
 *
 * @param devnum device number
 * @return handle to serial device if successful, NULL otherwise
 */
sio_fd_t sio_open(u8_t devnum) {
  sio_fd_t sd;

  /* USER CODE BEGIN 7 */
  sd = 0; // dummy code
          /* USER CODE END 7 */

  return sd;
}

/**
 * Sends a single character to the serial device.
 *
 * @param c character to send
 * @param fd serial device handle
 *
 * @note This function will block until the character can be sent.
 */
void sio_send(u8_t c, sio_fd_t fd) {
  /* USER CODE BEGIN 8 */
  /* USER CODE END 8 */
}

/**
 * Reads from the serial device.
 *
 * @param fd serial device handle
 * @param data pointer to data buffer for receiving
 * @param len maximum length (in bytes) of data to receive
 * @return number of bytes actually received - may be 0 if aborted by
 * sio_read_abort
 *
 * @note This function will block until data can be received. The blocking
 * can be cancelled by calling sio_read_abort().
 */
u32_t sio_read(sio_fd_t fd, u8_t *data, u32_t len) {
  u32_t recved_bytes;

  /* USER CODE BEGIN 9 */
  recved_bytes = 0; // dummy code
                    /* USER CODE END 9 */
  return recved_bytes;
}

/**
 * Tries to read from the serial device. Same as sio_read but returns
 * immediately if no data is available and never blocks.
 *
 * @param fd serial device handle
 * @param data pointer to data buffer for receiving
 * @param len maximum length (in bytes) of data to receive
 * @return number of bytes actually received
 */
u32_t sio_tryread(sio_fd_t fd, u8_t *data, u32_t len) {
  u32_t recved_bytes;

  /* USER CODE BEGIN 10 */
  recved_bytes = 0; // dummy code
                    /* USER CODE END 10 */
  return recved_bytes;
}
#endif /* MDK ARM Compiler */
