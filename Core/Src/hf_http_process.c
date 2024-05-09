/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "httpd.h"
#include "dhcp.h"
#include "hf_common.h"
#include "web-server.h"
extern struct netif gnetif;
struct dhcp *dhcp;
uint8_t ip_address[4] = {0};
uint8_t netmask_address[4] = {0};
uint8_t getway_address[4] = {0};
uint8_t mac_address[6] = {0};
void eth_get_address(void)
{
  es_get_mcu_ipaddr(ip_address);

  es_get_mcu_gateway(getway_address);

  es_get_mcu_netmask(netmask_address);

  es_get_mcu_mac(mac_address);
}

extern struct netif gnetif;
extern ip4_addr_t ipaddr;
extern ip4_addr_t netmask;
extern ip4_addr_t gw;

void dynamic_change_eth(void)
{

  IP4_ADDR(&ipaddr, ip_address[0], ip_address[1], ip_address[2],
            ip_address[3]);
  IP4_ADDR(&netmask, netmask_address[0], netmask_address[1],
            netmask_address[2], netmask_address[3]);
  IP4_ADDR(&gw, getway_address[0], getway_address[1], getway_address[2],
            getway_address[3]);

  netif_set_addr(&gnetif, &ipaddr, &netmask, &gw);

  netif_set_default(&gnetif);

  netif_set_up(&gnetif);
}

void hf_http_task(void *argument)
{
	printf("hf_http_task started!!!\r\n");
  osDelay(5000);
  /* get board info from eeprom where the MAC is stored */
  if(es_init_info_in_eeprom()) {
    printf("severe error: get info from eeprom failed!!!");
    while(1);
  }
  #if ES_EEPROM_INFO_TEST
  es_eeprom_info_test();
  #endif
  /* init code for LWIP */
  eth_get_address();
  MX_LWIP_Init();
  // httpd_init();
  // /* Infinite loop */
  osDelay(5000);
  dhcp = netif_dhcp_data(&gnetif);
  printf("Static IP address: %s\n", ip4addr_ntoa(&gnetif.ip_addr));
  printf("Subnet mask: %s\n", ip4addr_ntoa(&gnetif.netmask));
  printf("Default gateway: %s\n", ip4addr_ntoa(&gnetif.gw));
  memcpy((uint8_t *)ip_address, &gnetif.ip_addr, 4);
  memcpy((uint8_t *)netmask_address, &gnetif.netmask, 4);
  memcpy((uint8_t *)getway_address, &gnetif.gw, 4);
  httpserver_init();
  for(;;)
  {
    osDelay(1000);
  }
}