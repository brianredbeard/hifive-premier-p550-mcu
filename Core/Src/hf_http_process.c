/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "httpd.h"
#include "dhcp.h"
extern struct netif gnetif;
struct dhcp *dhcp;
uint8_t ip_address[4] = {0};
uint8_t netmask_address[4] = {0};
uint8_t getway_address[4] = {0};
uint8_t mac_address[6] = {0};
void eth_get_address(void)
{
  if(!ip_address[0])
  {
    ip_address[0] = IP_ADDR0;
    ip_address[1] = IP_ADDR1;
    ip_address[2] = IP_ADDR2;
    ip_address[3] = IP_ADDR3;
  }
  if(!getway_address[0])
  {
    getway_address[0] = GATEWAY_ADDR0;
    getway_address[1] = GATEWAY_ADDR1;
    getway_address[2] = GATEWAY_ADDR2;
    getway_address[3] = GATEWAY_ADDR3;
  }
  if(!ip4_addr_netmask_valid(netmask_address))
  {
    netmask_address[0] = NETMASK_ADDR0;
    netmask_address[1] = NETMASK_ADDR1;
    netmask_address[2] = NETMASK_ADDR2;
    netmask_address[3] = NETMASK_ADDR3;
  }
  if (!is_valid_ethaddr(mac_address)) {
    mac_address[0] = MAC_ADDR0;
    mac_address[1] = MAC_ADDR1;
    mac_address[2] = MAC_ADDR2;
    mac_address[3] = MAC_ADDR3;
    mac_address[4] = MAC_ADDR4;
    mac_address[5] = MAC_ADDR5;
  }
}
void hf_http_task(void *argument)
{
  /* init code for LWIP */
  eth_get_address();
  MX_LWIP_Init();
  httpd_init();
  // /* Infinite loop */
  osDelay(5000); 
  dhcp = netif_dhcp_data(&gnetif);
  printf("Static IP address: %s\n", ip4addr_ntoa(&gnetif.ip_addr));
  printf("Subnet mask: %s\n", ip4addr_ntoa(&gnetif.netmask));
  printf("Default gateway: %s\n", ip4addr_ntoa(&gnetif.gw));
  for(;;)
  {
    osDelay(1000);
  }
}