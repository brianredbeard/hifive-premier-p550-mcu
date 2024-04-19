/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "httpd.h"
#include "dhcp.h"
extern struct netif gnetif;
struct dhcp *dhcp;

void hf_http_task(void *argument)
{
  /* init code for LWIP */
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