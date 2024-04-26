#ifndef __HTTPSERVER_NETCONN_H__
#define __HTTPSERVER_NETCONN_H__

#define TRUE 1
#define FALSE 0
#define LOCAL_PORT 80

#define LED1_ON  do {printf("LED ON\n");} while(0)
#define LED1_OFF  do {printf("LED OFF\n");} while(0)

void httpserver_init(void);

#endif /* __HTTPSERVER_NETCONN_H__ */