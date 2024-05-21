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