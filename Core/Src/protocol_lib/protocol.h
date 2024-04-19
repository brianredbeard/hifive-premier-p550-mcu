#ifndef __ES_PROTOCOL_CORE_H__
#define __ES_PROTOCOL_CORE_H__

#include "string.h"
#include "stdint.h"
#include "ringbuffer.h"
//协议描述
typedef struct{
	const uint8_t *pname;
    uint16_t head_len; /* 帧头长度 */
    uint16_t end_len;  /* 帧尾长度 */
    const char *head;  /* 指向帧头数据 */
    const char *end;   /* 指向帧尾数据 */
}b_frame_t;

struct gpio_cmd {
	uint8_t group;
	uint16_t pin_num;
	uint8_t driection;
	uint8_t value;
};

struct ip_set_t {
    uint8_t ip_addr0;
    uint8_t ip_addr1;
    uint8_t ip_addr2;
    uint8_t ip_addr3;
};
struct frame_data {
	uint8_t is_valid;
	uint8_t cmd;
	union {
		uint32_t value;
		struct gpio_cmd gpio;
        struct ip_set_t ip;
	}data;
};

typedef struct
{
    b_frame_t frame_info;
    ring_buf_t _frame_ring;
    uint32_t _in_frame_buffer_size;
    struct frame_data frame;
} b_frame_class_t;

#endif






