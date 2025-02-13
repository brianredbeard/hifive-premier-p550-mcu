// SPDX-License-Identifier: GPL-2.0-only
/*
 * operation on the information stored in the eeprom
 *
 * Copyright 2024 Beijing ESWIN Computing Technology Co., Ltd.
 *   Authors:
 *    LinMin<linmin@eswincomputing.com>
 *
 */
/* Includes ------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
#include "lwip.h"
#include "hf_common.h"
#include "hf_crc32.h"

#include "cmsis_os.h"
#include "semphr.h"
#include "main.h"
#include "hf_i2c.h"
/* typedef -----------------------------------------------------------*/
/* define ------------------------------------------------------------*/
#define EEPROM_DEBUG_EN	0
#define eeprom_fmt(fmt)	"[%s-EEPROM]: " fmt
#define dbg_fmt(fmt)	eeprom_fmt("%s[%d]: " fmt), "DEBUG",	\
		        __func__, __LINE__
#if EEPROM_DEBUG_EN
#define eeprom_debug(fmt, args...) \
	do {							\
		printf(dbg_fmt(fmt), ##args);	\
	} while (0)
#else
#define eeprom_debug(fmt, args...)
#endif
/* macro -------------------------------------------------------------*/
#define EEPROM_TEST_DEBUG 0
#define SET_MAGIC_NUM_DEBUG 0


/* variables ---------------------------------------------------------*/
static CarrierBoardInfo gCarrier_Board_Info;

static MCUServerInfo gMCU_Server_Info;
static SomPwrMgtDIPInfo gSOM_PwgMgtDIP_Info;
SemaphoreHandle_t gEEPROM_Mutex;

static int gSOM_ConsoleCfg = 0; //0: The default console of SOM is uart; 1: Telnet SOM Console

/* function prototypes -----------------------------------------------*/
#if EEPROM_TEST_DEBUG
#define PRIME_SEED_A	0x7091 // 29917
#define PRIME_SEED_B	0x75B5 // 30133
static uint32_t lcg_parkmiller(uint32_t *state)
{
	uint64_t product = (uint64_t)*state * 48271;
	uint32_t x = (product & 0x7fffffff) + (product >> 31);

	x = (x & 0x7fffffff) + (x >> 31);
	return *state = x;
}

static void buf_fillin_random(void *buf, uint32_t size, uint32_t prime_seed)
{
	uint32_t *tmpbuf = (uint32_t *)buf;
	uint32_t size_remaining = size;
	uint32_t seed = prime_seed;
	uint32_t i = 0;

	memset(buf, 0, size);
	while (size_remaining > 0) {
		tmpbuf[i] = lcg_parkmiller(&seed);
		// printf("(uint32_t)buf[%d]=0x%x\n", i, tmpbuf[i]);
		size_remaining -= 4;
		i++;
	}
}

static int buf_compwith_random(void *buf, uint32_t size, uint32_t prime_seed)
{
	uint32_t *tmpbuf = (uint32_t *)buf;
	uint32_t size_remaining = size;
	uint32_t random, seed = prime_seed;
	uint32_t i = 0;

	while (size_remaining > 0) {
		random = lcg_parkmiller(&seed);
		if (tmpbuf[i] != random)	{
			printf("Err Comp, (uint32_t)buf[%ld]:0x%lx is NOT equal to expected val:0x%lx\n",
				i, tmpbuf[i], random);
			return -1;
		}
		size_remaining -= 4;
		i++;
	}

	return 0;
}
#endif

static void print_data(uint8_t *p_buf, int len)
{
	#if EEPROM_DEBUG_EN
	int i;
	for (i = 0; i < len; i++) {
		if (0 == (i%8))
			printf("0x%02x:", i);

		printf(" %02x", p_buf[i]);
		if (0 == ((i+1)%8))
			printf("\n");
	}
	printf("\n");
	#endif
}

static u32_t ntohl_seq(uint8_t *p_nlmask)
{
	u32_t hlmask = 0, tmp = 0;

	hlmask = p_nlmask[0];
	tmp = p_nlmask[1];
	hlmask |= (tmp << 8);

	tmp = p_nlmask[2];
	hlmask |= (tmp << 16);

	tmp = p_nlmask[3];
	hlmask |= (tmp << 24);

	return hlmask;
}

static int hex2num(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

void hexstr2mac(uint8_t *mac, const char *hexstr)
{
	int i = 0;

	while (i < 6) {
		if (' ' == *hexstr || ':' == *hexstr || '"' == *hexstr || '\'' == *hexstr) {
			hexstr++;
			continue;
		}
		*(mac + i) = (hex2num(*hexstr) <<4) | hex2num(*(hexstr + 1));
		i++;
		hexstr += 2;
	}
}

uint64_t atoh(const char *in, uint32_t len)
{
	uint64_t sum = 0;
	uint64_t mult = 1;
	unsigned char c;

	while (len) {
		int value;

		c = in[len - 1];
		value = hex2num(c);
		if (value >= 0)
			sum += mult * value;
		mult *= 16;
		--len;
	}
	return sum;
}

int _write(int fd, char *ch, int len)
{
	uint8_t val = '\r';
	uint16_t length = 0;
	for (length = 0; length < len; length++) {
		if (ch[length] == '\n') {
			HAL_UART_Transmit(&huart3, (uint8_t *)&val, 1, 0xFFFF);
		}
		HAL_UART_Transmit(&huart3, (uint8_t *)&ch[length], 1, 0xFFFF);
	}
	return length;
}

void es_eeprom_wp(uint8_t flag)
{
	if(flag) {
		HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_SET);
	}
	else {
		HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_RESET);
	}
}

typedef enum {
	cbinfo_main = 0,
	cbinfo_backup
}CbinfoPart;

static int read_cbinfo(CarrierBoardInfo *pCarrier_Board_Info, CbinfoPart cbinfo_part)
{
	int ret = 0;
	uint8_t reg_addr = 0;

	reg_addr = (cbinfo_part == cbinfo_main)?CARRIER_BOARD_INFO_EEPROM_MAIN_OFFSET:
			CARRIER_BOARD_INFO_EEPROM_BACKUP_OFFSET;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	ret = hf_i2c_mem_read(&hi2c1, AT24C_ADDR, reg_addr,
				(uint8_t *)pCarrier_Board_Info, sizeof(CarrierBoardInfo));
	esEXIT_CRITICAL(gEEPROM_Mutex);
	if(ret) {
		return -1;
	}

	return 0;
}
static int write_cbinfo(CarrierBoardInfo *pCarrier_Board_Info, CbinfoPart cbinfo_part)
{
	uint8_t reg_addr = 0;

	reg_addr = (cbinfo_part == cbinfo_main)?CARRIER_BOARD_INFO_EEPROM_MAIN_OFFSET:
			CARRIER_BOARD_INFO_EEPROM_BACKUP_OFFSET;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	hf_i2c_mem_write(&hi2c1, AT24C_ADDR, reg_addr,
				(uint8_t *)pCarrier_Board_Info, sizeof(CarrierBoardInfo));
	esEXIT_CRITICAL(gEEPROM_Mutex);
	eeprom_debug("Updated CarrierBoardInfo in EEPROM!\n");

	return 0;
}

static int restore_cbinfo_to_factory(CarrierBoardInfo *pCarrier_Board_Info)
{
	char sn[] = "sn1234567890abcdef";

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);

	memset(pCarrier_Board_Info, 0, sizeof(CarrierBoardInfo));
	pCarrier_Board_Info->magicNumber = MAGIC_NUMBER;

	pCarrier_Board_Info->formatVersionNumber = 0x3;
	pCarrier_Board_Info->productIdentifier = 0x4;
	pCarrier_Board_Info->pcbRevision = 0x10;
	pCarrier_Board_Info->bomRevision = 0x10;
	pCarrier_Board_Info->bomVariant = 0x10;

	memcpy(pCarrier_Board_Info->boardSerialNumber, sn, sizeof(pCarrier_Board_Info->boardSerialNumber));

	pCarrier_Board_Info->manufacturingTestStatus = 0x10;

	pCarrier_Board_Info->ethernetMAC3[0] = MAC_ADDR0;
	pCarrier_Board_Info->ethernetMAC3[1] = MAC_ADDR1;
	pCarrier_Board_Info->ethernetMAC3[2] = MAC_ADDR2;
	pCarrier_Board_Info->ethernetMAC3[3] = MAC_ADDR3;
	pCarrier_Board_Info->ethernetMAC3[4] = MAC_ADDR4;
	pCarrier_Board_Info->ethernetMAC3[5] = MAC_ADDR5;

	pCarrier_Board_Info->crc32Checksum = hf_crc32((uint8_t *)pCarrier_Board_Info, sizeof(CarrierBoardInfo) - 4);
#if 0
	/* write to main partition */
	write_cbinfo(pCarrier_Board_Info, cbinfo_main);

	/* write to backup partition */
	write_cbinfo(pCarrier_Board_Info, cbinfo_backup);
#endif
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

static int restore_cbinfo_to_factory_without_mutext(CarrierBoardInfo *pCarrier_Board_Info)
{
	char sn[] = "sn1234567890abcdef";

	memset(pCarrier_Board_Info, 0, sizeof(CarrierBoardInfo));
	pCarrier_Board_Info->magicNumber = MAGIC_NUMBER;

	pCarrier_Board_Info->formatVersionNumber = 0x3;
	pCarrier_Board_Info->productIdentifier = 0x4;
	pCarrier_Board_Info->pcbRevision = 0x10;
	pCarrier_Board_Info->bomRevision = 0x10;
	pCarrier_Board_Info->bomVariant = 0x10;

	memcpy(pCarrier_Board_Info->boardSerialNumber, sn, sizeof(pCarrier_Board_Info->boardSerialNumber));

	pCarrier_Board_Info->manufacturingTestStatus = 0x10;

	pCarrier_Board_Info->ethernetMAC3[0] = MAC_ADDR0;
	pCarrier_Board_Info->ethernetMAC3[1] = MAC_ADDR1;
	pCarrier_Board_Info->ethernetMAC3[2] = MAC_ADDR2;
	pCarrier_Board_Info->ethernetMAC3[3] = MAC_ADDR3;
	pCarrier_Board_Info->ethernetMAC3[4] = MAC_ADDR4;
	pCarrier_Board_Info->ethernetMAC3[5] = MAC_ADDR5;

	pCarrier_Board_Info->crc32Checksum = hf_crc32((uint8_t *)pCarrier_Board_Info, sizeof(CarrierBoardInfo) - 4);


	return 0;
}

static int print_cbinfo(CarrierBoardInfo *pCarrierBoardInfo)
{
	char boardSn[19] = {0};

	eeprom_debug("[Carrierboard Information:]\n");

	eeprom_debug("magicNumber:0x%lx\n", pCarrierBoardInfo->magicNumber);

	eeprom_debug("formatVersionNumber:0x%x\n", pCarrierBoardInfo->formatVersionNumber);

	eeprom_debug("productIdentifier:0x%x\r\n", pCarrierBoardInfo->productIdentifier);

	eeprom_debug("pcbRevision:0x%x\r\n", pCarrierBoardInfo->pcbRevision);

	eeprom_debug("bomRevision:0x%x\r\n", pCarrierBoardInfo->bomRevision);

	eeprom_debug("bomVariant:0x%x\r\n", pCarrierBoardInfo->bomVariant);


	memcpy(boardSn, pCarrierBoardInfo->boardSerialNumber, sizeof(pCarrierBoardInfo->boardSerialNumber));
	eeprom_debug("SN:%s\n", boardSn);

	eeprom_debug("manufacturingTestStatus:0x%x\n", pCarrierBoardInfo->manufacturingTestStatus);
	eeprom_debug("checksum:0x%lx\n", pCarrierBoardInfo->crc32Checksum);


	return 0;
}
/* This API is called only once at start up time */
static int get_carrier_board_info(void)
{
	int ret = 0;
	uint32_t crc32Checksum;
	CarrierBoardInfo CbinfoBackup;

	eeprom_debug("&gCarrier_Board_Info addr:0x%p, size:%d\n", &gCarrier_Board_Info, sizeof(CarrierBoardInfo));
	memset((uint8_t *)&gCarrier_Board_Info, 0, sizeof(CarrierBoardInfo));
	/* read main partintion */
	ret = read_cbinfo(&gCarrier_Board_Info, cbinfo_main);
	if(ret) {
		return -1;
	}
	print_cbinfo(&gCarrier_Board_Info);
	print_data((uint8_t *)&gCarrier_Board_Info, sizeof(CarrierBoardInfo));
	/* calculate crc32 checksum of main partition */
	crc32Checksum = hf_crc32((uint8_t *)&gCarrier_Board_Info, sizeof(CarrierBoardInfo) - 4);
	if (crc32Checksum != gCarrier_Board_Info.crc32Checksum) { //main partition is bad
		printf("Bad main checksum,0x%lx is NOT equal to calculated value:0x%lx\n", gCarrier_Board_Info.crc32Checksum, crc32Checksum);
		eeprom_debug("%s:%d\n", __func__, __LINE__);
		print_data((uint8_t *)&gCarrier_Board_Info, sizeof(CarrierBoardInfo));
		/* try to read and check the backup partition */
		ret = read_cbinfo(&gCarrier_Board_Info, cbinfo_backup);
		if(ret) {
			return -1;
		}

		crc32Checksum = hf_crc32((uint8_t *)&gCarrier_Board_Info, sizeof(CarrierBoardInfo) - 4);
		if (crc32Checksum != gCarrier_Board_Info.crc32Checksum) { // backup patition is also bad
			/* restore to factory settings */
			printf("Bad backup checksum,0x%lx is NOT equal to calculated value:0x%lx\n", gCarrier_Board_Info.crc32Checksum, crc32Checksum);
			restore_cbinfo_to_factory(&gCarrier_Board_Info);
			printf("Restored cbinfo to factory settings\n");
		}
		else { // backup partion is ok
			/* recover the main partition with the backup value */
			printf("Recover main with backup settings\n");
			write_cbinfo(&gCarrier_Board_Info, cbinfo_main);
		}
	}
	else { // main partition is ok
		/* check backup partion, if it's bad, recover it with main value */
		ret = read_cbinfo(&CbinfoBackup, cbinfo_backup);
		if(ret) {
			return -1;
		}
		print_cbinfo(&CbinfoBackup);
		crc32Checksum = hf_crc32((uint8_t *)&CbinfoBackup, sizeof(CarrierBoardInfo) - 4);
		if (crc32Checksum != CbinfoBackup.crc32Checksum) {
			/* recover the backup partition with the main value */
			printf("Bad backup checksum,0x%lx is NOT equal to calculated value:0x%lx\n", CbinfoBackup.crc32Checksum, crc32Checksum);
			printf("Recover backup with main settings\n");
			write_cbinfo(&gCarrier_Board_Info, cbinfo_backup);
		}
		else {
			printf("cbinfo checksum ok!!!!\n");
		}
	}
	return 0;
}

static int get_mcu_server_info(void)
{
	int ret = 0;
	int skip_update_eeprom = 1;
	uint32_t crc32Checksum;

	memset((uint8_t *)&gMCU_Server_Info, 0, sizeof(MCUServerInfo));
	eeprom_debug("print MCUServerInfo:\n");
	ret = hf_i2c_mem_read(&hi2c1, AT24C_ADDR, MCU_SERVER_INFO_EEPROM_OFFSET,
				(uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));
	if(ret) {
		printf("Err to read mcu_server_info from EEPROM!!!\n");
		return -1;
	}
	eeprom_debug("print MCUServerInfo:\n");
	print_data((uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));
	crc32Checksum = hf_crc32((uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);

	if (crc32Checksum != gMCU_Server_Info.crc32Checksum) {
		skip_update_eeprom = 0;
		memset(gMCU_Server_Info.AdminName, 0, sizeof(gMCU_Server_Info.AdminName));
		strcpy(gMCU_Server_Info.AdminName, DEFAULT_ADMIN_NAME);

		memset(gMCU_Server_Info.AdminPassword, 0, sizeof(gMCU_Server_Info.AdminPassword));
		strcpy(gMCU_Server_Info.AdminPassword, DEFAULT_ADMIN_PASSWORD);

		gMCU_Server_Info.ip_address[0] = IP_ADDR0;
		gMCU_Server_Info.ip_address[1] = IP_ADDR1;
		gMCU_Server_Info.ip_address[2] = IP_ADDR2;
		gMCU_Server_Info.ip_address[3] = IP_ADDR3;

		gMCU_Server_Info.netmask_address[0] = NETMASK_ADDR0;
		gMCU_Server_Info.netmask_address[1] = NETMASK_ADDR1;
		gMCU_Server_Info.netmask_address[2] = NETMASK_ADDR2;
		gMCU_Server_Info.netmask_address[3] = NETMASK_ADDR3;

		gMCU_Server_Info.gateway_address[0] = GATEWAY_ADDR0;
		gMCU_Server_Info.gateway_address[1] = GATEWAY_ADDR1;
		gMCU_Server_Info.gateway_address[2] = GATEWAY_ADDR2;
		gMCU_Server_Info.gateway_address[3] = GATEWAY_ADDR3;

		gMCU_Server_Info.crc32Checksum = hf_crc32((uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);
		printf("Invalid checksum of mcu server info, update it with default value!\n");
	}

	if (skip_update_eeprom == 0) {
		eeprom_debug("Update admin_name:%s, password:%s\n",
			gMCU_Server_Info.AdminName, gMCU_Server_Info.AdminPassword);
		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, MCU_SERVER_INFO_EEPROM_OFFSET,
			(uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));
		// osDelay(100);
		ret = hf_i2c_mem_read(&hi2c1, AT24C_ADDR, MCU_SERVER_INFO_EEPROM_OFFSET,
				(uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));
		eeprom_debug("Updated MCUServerInfo in EEPROM!, admin_name:%s, password:%s\n",
			gMCU_Server_Info.AdminName, gMCU_Server_Info.AdminPassword);
	}
	eeprom_debug("print MCUServerInfo:\n");
	print_data((uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));

	printf("get_mcu_server_info Successfully!\n");

	return 0;
}

static int get_som_pwrmgt_dip_info(void)
{
	int ret = 0;
	int skip_update_eeprom = 1;
	uint32_t crc32Checksum;

	memset((uint8_t *)&gSOM_PwgMgtDIP_Info,  0, sizeof(SomPwrMgtDIPInfo));
	ret = hf_i2c_mem_read(&hi2c1, AT24C_ADDR, SOM_PWRMGT_DIP_INFO_EEPROM_OFFSET,
				(uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(SomPwrMgtDIPInfo));
	if(ret) {
		printf("Err to read SomPwrMgtDIPInfo from EEPROM!!!\n");
		return -1;
	}

	crc32Checksum = hf_crc32((uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(gSOM_PwgMgtDIP_Info) - 4);

	if (crc32Checksum != gSOM_PwgMgtDIP_Info.crc32Checksum) {
		printf("Invalid checksum of SomPwrMgtDIPInfo, init with default value!!!\n");
		skip_update_eeprom = 0;
		gSOM_PwgMgtDIP_Info.som_pwr_lost_resume_attr = SOM_PWR_LOST_RESUME_DISABLE;
		gSOM_PwgMgtDIP_Info.som_pwr_last_state = SOM_PWR_LAST_STATE_OFF;
		gSOM_PwgMgtDIP_Info.som_dip_switch_soft_ctl_attr = SOM_DIP_SWITCH_SOFT_CTL_DISABLE;
		gSOM_PwgMgtDIP_Info.som_dip_switch_soft_state = SOM_DIP_SWITCH_STATE_EMMC;
		gSOM_PwgMgtDIP_Info.crc32Checksum = hf_crc32((uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(gSOM_PwgMgtDIP_Info) - 4);
	}

	if (skip_update_eeprom == 0) {
		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, SOM_PWRMGT_DIP_INFO_EEPROM_OFFSET,
			(uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(SomPwrMgtDIPInfo));
		eeprom_debug("Updated SomPwrMgtDIPInfo in EEPROM!\n");

		ret = hf_i2c_mem_read(&hi2c1, AT24C_ADDR, SOM_PWRMGT_DIP_INFO_EEPROM_OFFSET,
					(uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(SomPwrMgtDIPInfo));
		eeprom_debug("Read SomPwrMgtDIPInfo again, resume_attr:0x%x, last_state:0x%x, dip_attr:0x%x, dip_state:0x%x\n",
					gSOM_PwgMgtDIP_Info.som_pwr_lost_resume_attr,
					gSOM_PwgMgtDIP_Info.som_pwr_last_state,
					gSOM_PwgMgtDIP_Info.som_dip_switch_soft_ctl_attr,
					gSOM_PwgMgtDIP_Info.som_dip_switch_soft_state);
	}

	return 0;
}

/* This function must be called before other es_get/set_xxx function in this file */
int es_init_info_in_eeprom(void)
{
	#if EEPROM_TEST_DEBUG
	return 0;
	#else
	int ret = 0;

	if (USER_DATA_USED_SIZE > USER_MAX_SIZE) {
		printf("Error, the USER_DATA_USED_SIZE(%d) exceed the USER_MAX_SIZE(%d)!!!\n",
			USER_DATA_USED_SIZE, USER_MAX_SIZE);
		return -1;
	}

	gEEPROM_Mutex = xSemaphoreCreateMutex();
	if (gEEPROM_Mutex == NULL) {
		printf("Failed to xSemaphoreCreateMutex for gEEPROM_Mutex!!!\n");
		return -1;
	}

	ret = get_carrier_board_info();
	if (ret) {
		printf("Failed to get_carrier_board_info!!!\n");
		return ret;
	}

	ret = get_mcu_server_info();
	if (ret) {
		printf("Failed to get_mcu_server_info!!!\n");
		return ret;
	}

	ret = get_som_pwrmgt_dip_info();
	if (ret) {
		printf("Failed to get_som_pwrmgt_dip_info!!!\n");
		return ret;
	}
	printf("es init info from epprom ok!\n");
	return 0;
	#endif
}

// int es_get_borad_info(CarrierBoardInfo *pCarrier_board_info)
int es_get_carrier_borad_info(CarrierBoardInfo *pCarrier_board_info)
{
	if (NULL == pCarrier_board_info)
		return -1;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	memcpy(pCarrier_board_info, &gCarrier_Board_Info, sizeof(CarrierBoardInfo));
	esEXIT_CRITICAL(gEEPROM_Mutex);
	return 0;
}

int es_set_carrier_borad_info(CarrierBoardInfo *pCarrier_Board_Info)
{
	int skip_update_eeprom = 1;

	if (NULL == pCarrier_Board_Info)
		return -1;

	pCarrier_Board_Info->crc32Checksum = hf_crc32((uint8_t *)pCarrier_Board_Info, sizeof(CarrierBoardInfo) - 4);

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	if (0 != memcmp(pCarrier_Board_Info, &gCarrier_Board_Info, sizeof(CarrierBoardInfo))) {
		memcpy(&gCarrier_Board_Info, pCarrier_Board_Info, sizeof(CarrierBoardInfo));
		skip_update_eeprom = 0;
		eeprom_debug("Updated CarrierBoardInfo in EEPROM!\n");
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);

	if (!skip_update_eeprom) {
		/* write to main partition */
		write_cbinfo(pCarrier_Board_Info, cbinfo_main);

		/* write to backup partition */
		write_cbinfo(pCarrier_Board_Info, cbinfo_backup);
	}

	return 0;
}

int es_get_mcu_mac(uint8_t *p_mac_address, uint8_t index)
{
	if (NULL == p_mac_address)
		return -1;

	if (index > 2) {
		printf("Invalid mac index!!! Should be within(0-2)\n");
		return -1;
	}
	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	switch (index) {
		case 0:
			memcpy(p_mac_address, gCarrier_Board_Info.ethernetMAC1, sizeof(gCarrier_Board_Info.ethernetMAC1));
			break;
		case 1:
			memcpy(p_mac_address, gCarrier_Board_Info.ethernetMAC2, sizeof(gCarrier_Board_Info.ethernetMAC2));
			break;
		case 2:
			memcpy(p_mac_address, gCarrier_Board_Info.ethernetMAC3, sizeof(gCarrier_Board_Info.ethernetMAC3));
			break;
		default:
			break;
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

int es_set_mcu_mac(uint8_t *p_mac_address, uint8_t index)
{
	int update_mac = 0;

	if (NULL == p_mac_address)
		return -1;

	if (1 != is_valid_ethaddr(p_mac_address)) {
		return -1;
	}

	if (index > 2) {
		printf("Invalid mac index!!! Should be within(0-2)\n");
		return -1;
	}

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	switch (index) {
		case 0:
			if (0 != memcmp(gCarrier_Board_Info.ethernetMAC1, p_mac_address, sizeof(gCarrier_Board_Info.ethernetMAC1))) {
				memcpy(gCarrier_Board_Info.ethernetMAC1, p_mac_address, sizeof(gCarrier_Board_Info.ethernetMAC1));
				update_mac = 1;
			}
			break;
		case 1:
		{
			if (0 != memcmp(gCarrier_Board_Info.ethernetMAC2, p_mac_address, sizeof(gCarrier_Board_Info.ethernetMAC2))) {
				memcpy(gCarrier_Board_Info.ethernetMAC2, p_mac_address, sizeof(gCarrier_Board_Info.ethernetMAC2));
				update_mac = 1;
			}
			break;
		}
		case 2:
		{
			if (0 != memcmp(gCarrier_Board_Info.ethernetMAC3, p_mac_address, sizeof(gCarrier_Board_Info.ethernetMAC3))) {
				memcpy(gCarrier_Board_Info.ethernetMAC3, p_mac_address, sizeof(gCarrier_Board_Info.ethernetMAC3));
				update_mac = 1;
			}
			break;
		}
		default:
			break;
	}
	if (update_mac) {
		gCarrier_Board_Info.crc32Checksum = hf_crc32((uint8_t *)&gCarrier_Board_Info, sizeof(CarrierBoardInfo) - 4);

		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, CARRIER_BOARD_INFO_EEPROM_MAIN_OFFSET,
					(uint8_t *)&gCarrier_Board_Info, sizeof(CarrierBoardInfo));

		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, CARRIER_BOARD_INFO_EEPROM_BACKUP_OFFSET,
					(uint8_t *)&gCarrier_Board_Info, sizeof(CarrierBoardInfo));
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

// get and set ipaddr
int es_get_mcu_ipaddr(uint8_t *p_ip_address)
{
	if (NULL == p_ip_address)
		return -1;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	memcpy(p_ip_address, gMCU_Server_Info.ip_address, sizeof(gMCU_Server_Info.ip_address));
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

int es_set_mcu_ipaddr(uint8_t *p_ip_address)
{
	if (NULL == p_ip_address)
		return -1;

	if (0 == p_ip_address[0]) {
		return -1;
	}

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	if (0 != memcmp(gMCU_Server_Info.ip_address, p_ip_address, sizeof(gMCU_Server_Info.ip_address))) {
		memcpy(gMCU_Server_Info.ip_address, p_ip_address, sizeof(gMCU_Server_Info.ip_address));
		gMCU_Server_Info.crc32Checksum = hf_crc32((uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);
		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, MCU_SERVER_INFO_EEPROM_OFFSET,
			(uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

// get and set netmask
int es_get_mcu_netmask(uint8_t *p_netmask_address)
{
	if (NULL == p_netmask_address)
		return -1;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	memcpy(p_netmask_address, gMCU_Server_Info.netmask_address, sizeof(gMCU_Server_Info.netmask_address));
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

int es_set_mcu_netmask(uint8_t *p_netmask_address)
{
	u32_t hlmask;

	if (NULL == p_netmask_address)
		return -1;

	hlmask = ntohl_seq(p_netmask_address);
	if (!ip4_addr_netmask_valid(hlmask)) {
		return -1;
	}

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	if (0 != memcmp(gMCU_Server_Info.netmask_address, p_netmask_address, sizeof(gMCU_Server_Info.netmask_address))) {
		memcpy(gMCU_Server_Info.netmask_address, p_netmask_address, sizeof(gMCU_Server_Info.netmask_address));
		gMCU_Server_Info.crc32Checksum = hf_crc32((uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);
		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, MCU_SERVER_INFO_EEPROM_OFFSET,
			(uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

// get and set gateway address
int es_get_mcu_gateway(uint8_t *p_gateway_address)
{
	if (NULL == p_gateway_address)
		return -1;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	memcpy(p_gateway_address, gMCU_Server_Info.gateway_address, sizeof(gMCU_Server_Info.gateway_address));
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

int es_set_mcu_gateway(uint8_t *p_gateway_address)
{
	if (NULL == p_gateway_address)
		return -1;

	if (0 == p_gateway_address[0])
		return -1;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	if (0 != memcmp(gMCU_Server_Info.gateway_address, p_gateway_address, sizeof(gMCU_Server_Info.gateway_address))) {
		memcpy(gMCU_Server_Info.gateway_address, p_gateway_address, sizeof(gMCU_Server_Info.gateway_address));
		gMCU_Server_Info.crc32Checksum = hf_crc32((uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);
		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, MCU_SERVER_INFO_EEPROM_OFFSET,
			(uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

// get and set admin info
int es_get_username_password(char *p_admin_name, char *p_admin_password)
{
	if (NULL == p_admin_name)
		return -1;

	if (NULL == p_admin_password)
		return -1;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	strcpy(p_admin_name, gMCU_Server_Info.AdminName);
	strcpy(p_admin_password, gMCU_Server_Info.AdminPassword);
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

int es_set_username_password(const char *p_admin_name, const char *p_admin_password)
{
	if (NULL == p_admin_name)
		return -1;

	if (NULL == p_admin_password)
		return -1;

	if (0 == strlen(p_admin_name)) {
		return -1;
	}

	if (0 == strlen(p_admin_password)) {
		return -1;
	}

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	if ((0 != strcmp(gMCU_Server_Info.AdminName, p_admin_name)) || (0 != strcmp(gMCU_Server_Info.AdminPassword, p_admin_password))) {
		strcpy(gMCU_Server_Info.AdminName, p_admin_name);
		strcpy(gMCU_Server_Info.AdminPassword, p_admin_password);
		gMCU_Server_Info.crc32Checksum = hf_crc32((uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);
		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, MCU_SERVER_INFO_EEPROM_OFFSET,
			(uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

/* set and get som power lost resume enable attribute*/
// return 1 if resume the power to the lost state
int is_som_pwr_lost_resume(void)
{
	int isResumePwrLost = 0;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	if (gSOM_PwgMgtDIP_Info.som_pwr_lost_resume_attr == SOM_PWR_LOST_RESUME_ENABLE)
		isResumePwrLost = 1;
	else
		isResumePwrLost = 0;
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return isResumePwrLost;
}

/* isResumePwrLost:
	1: resume the power to the lost state
	0: Do NOT resume the power
*/
int es_set_som_pwr_lost_resume_attr(int isResumePwrLost)
{
	uint8_t som_pwr_lost_resume_attr;

	if ((isResumePwrLost != 1) && (isResumePwrLost != 0)) {
		return -1;
	}

	if (isResumePwrLost == 1)
		som_pwr_lost_resume_attr = SOM_PWR_LOST_RESUME_ENABLE;
	else
		som_pwr_lost_resume_attr = SOM_PWR_LOST_RESUME_DISABLE;

	eeprom_debug("old_resume_attr=0x%x, new_resume_attr=0x%x\n",
			gSOM_PwgMgtDIP_Info.som_pwr_lost_resume_attr, som_pwr_lost_resume_attr);

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	if (som_pwr_lost_resume_attr != gSOM_PwgMgtDIP_Info.som_pwr_lost_resume_attr) {
		gSOM_PwgMgtDIP_Info.som_pwr_lost_resume_attr = som_pwr_lost_resume_attr;
		gSOM_PwgMgtDIP_Info.crc32Checksum = hf_crc32((uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(gSOM_PwgMgtDIP_Info) - 4);
		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, SOM_PWRMGT_DIP_INFO_EEPROM_OFFSET,
			(uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(SomPwrMgtDIPInfo));
		eeprom_debug("Update SomPwrMgtDIPInfo in EEPROM for lost_resume_attr\n");
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

/* get the som power last state
   *p_som_pwr_last_state: return the som power last state, 1 means ON, 0 means OFF
*/
int es_get_som_pwr_last_state(int *p_som_pwr_last_state)
{
	if (NULL == p_som_pwr_last_state)
		return -1;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	*p_som_pwr_last_state = (gSOM_PwgMgtDIP_Info.som_pwr_last_state == SOM_PWR_LAST_STATE_ON) ? 1 : 0;
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

/* set the som power last state
   som_pwr_last_state: 1 means ON, 0 means OFF
*/
int es_set_som_pwr_last_state(int som_pwr_last_state)
{
	int som_pwr_last_state_internal_fmt;

	if (som_pwr_last_state) {
		som_pwr_last_state_internal_fmt = SOM_PWR_LAST_STATE_ON;
	}
	else {
		som_pwr_last_state_internal_fmt = SOM_PWR_LAST_STATE_OFF;
	}

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	if (som_pwr_last_state_internal_fmt != gSOM_PwgMgtDIP_Info.som_pwr_last_state) {
		gSOM_PwgMgtDIP_Info.som_pwr_last_state = som_pwr_last_state_internal_fmt;
		gSOM_PwgMgtDIP_Info.crc32Checksum = hf_crc32((uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(gSOM_PwgMgtDIP_Info) - 4);
		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, SOM_PWRMGT_DIP_INFO_EEPROM_OFFSET,
			(uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(SomPwrMgtDIPInfo));
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

/* get the som dip switch soft ctl attribute
  *p_som_dip_switch_soft_ctl_attr: return the attribute, 1 means soft ctrl, 0 means hardware ctrl
*/
int es_get_som_dip_switch_soft_ctl_attr(int *p_som_dip_switch_soft_ctl_attr)
{
	if (NULL == p_som_dip_switch_soft_ctl_attr)
		return -1;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	*p_som_dip_switch_soft_ctl_attr = (gSOM_PwgMgtDIP_Info.som_dip_switch_soft_ctl_attr == SOM_DIP_SWITCH_SOFT_CTL_ENABLE) ? 1 : 0;
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

/* set the som dip switch soft ctl attribute
  som_dip_switch_soft_ctl_attr: 1 means soft ctrl, 0 means hardware ctrl
*/
int es_set_som_dip_switch_soft_ctl_attr(int som_dip_switch_soft_ctl_attr)
{
	int som_dip_swtich_soft_ctl_attr_internal_fmt;

	if (som_dip_switch_soft_ctl_attr) {
		som_dip_swtich_soft_ctl_attr_internal_fmt = SOM_DIP_SWITCH_SOFT_CTL_ENABLE;
	}
	else {
		som_dip_swtich_soft_ctl_attr_internal_fmt = SOM_DIP_SWITCH_SOFT_CTL_DISABLE;
	}

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	if (som_dip_swtich_soft_ctl_attr_internal_fmt != gSOM_PwgMgtDIP_Info.som_dip_switch_soft_ctl_attr) {
		gSOM_PwgMgtDIP_Info.som_dip_switch_soft_ctl_attr = som_dip_swtich_soft_ctl_attr_internal_fmt;
		gSOM_PwgMgtDIP_Info.crc32Checksum = hf_crc32((uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(gSOM_PwgMgtDIP_Info) - 4);
		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, SOM_PWRMGT_DIP_INFO_EEPROM_OFFSET,
			(uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(SomPwrMgtDIPInfo));
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

/* get the som dip switch soft state
  *p_som_dip_switch_soft_state: return the dip state. bit3-bit0 stands for bootsel3-0
*/
int es_get_som_dip_switch_soft_state(uint8_t *p_som_dip_switch_soft_state)
{
	if (NULL == p_som_dip_switch_soft_state)
		return -1;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	*p_som_dip_switch_soft_state = 0xF & gSOM_PwgMgtDIP_Info.som_dip_switch_soft_state;
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

int es_set_som_dip_switch_soft_state(uint8_t som_dip_switch_soft_state)
{
	uint8_t som_dip_switch_soft_state_internal_fmt;

	som_dip_switch_soft_state_internal_fmt = 0xE0 | (0xF & som_dip_switch_soft_state);

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	if (som_dip_switch_soft_state_internal_fmt != gSOM_PwgMgtDIP_Info.som_dip_switch_soft_state) {
		gSOM_PwgMgtDIP_Info.som_dip_switch_soft_state = som_dip_switch_soft_state_internal_fmt;
		gSOM_PwgMgtDIP_Info.crc32Checksum = hf_crc32((uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(gSOM_PwgMgtDIP_Info) - 4);
		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, SOM_PWRMGT_DIP_INFO_EEPROM_OFFSET,
			(uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(SomPwrMgtDIPInfo));
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}


int es_get_som_dip_switch_soft_state_all(int *p_som_dip_switch_soft_ctl_attr, uint8_t *p_som_dip_switch_soft_state)
{
	if (NULL == p_som_dip_switch_soft_state)
		return -1;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	*p_som_dip_switch_soft_ctl_attr = (gSOM_PwgMgtDIP_Info.som_dip_switch_soft_ctl_attr == SOM_DIP_SWITCH_SOFT_CTL_ENABLE) ? 1 : 0;
	*p_som_dip_switch_soft_state = 0xF & gSOM_PwgMgtDIP_Info.som_dip_switch_soft_state;
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

int es_set_som_dip_switch_soft_state_all(int som_dip_switch_soft_ctl_attr, uint8_t som_dip_switch_soft_state)
{
	int som_dip_swtich_soft_ctl_attr_internal_fmt;
	uint8_t som_dip_switch_soft_state_internal_fmt;

	/* convert to internal ctl attr */
	if (som_dip_switch_soft_ctl_attr) {
		som_dip_swtich_soft_ctl_attr_internal_fmt = SOM_DIP_SWITCH_SOFT_CTL_ENABLE;
	}
	else {
		som_dip_swtich_soft_ctl_attr_internal_fmt = SOM_DIP_SWITCH_SOFT_CTL_DISABLE;
	}

	/* convert to internal soft sate */

	som_dip_switch_soft_state_internal_fmt = 0xE0 | (0xF & som_dip_switch_soft_state);

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	if ((som_dip_swtich_soft_ctl_attr_internal_fmt != gSOM_PwgMgtDIP_Info.som_dip_switch_soft_ctl_attr) ||
	    (som_dip_switch_soft_state_internal_fmt != gSOM_PwgMgtDIP_Info.som_dip_switch_soft_state)) {
		gSOM_PwgMgtDIP_Info.som_dip_switch_soft_ctl_attr = som_dip_swtich_soft_ctl_attr_internal_fmt;
		gSOM_PwgMgtDIP_Info.som_dip_switch_soft_state = som_dip_switch_soft_state_internal_fmt;
		gSOM_PwgMgtDIP_Info.crc32Checksum = hf_crc32((uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(gSOM_PwgMgtDIP_Info) - 4);
		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, SOM_PWRMGT_DIP_INFO_EEPROM_OFFSET,
			(uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(SomPwrMgtDIPInfo));
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

#if EEPROM_TEST_DEBUG
static int es_eeprom_test(void)
{
	int ret = 0;
	int err = 0;
	uint32_t test_buf_A[64], test_buf_B[64];
	uint32_t len = sizeof(test_buf_A);
	int i = 0, offset = 0;
	uint8_t *pbuf = (uint8_t *)test_buf_A;
	osDelay(100);

	memset(test_buf_A, 0, sizeof(test_buf_A));
	memset(test_buf_B, 0, sizeof(test_buf_B));
	buf_fillin_random(test_buf_A, len, PRIME_SEED_B);
	eeprom_debug("random data(len=%ld,hex):\n", len);
	print_data((uint8_t *)test_buf_A, len);
	for (i = 0; i < (len/21); i++) {
		eeprom_debug("i=%d\n", i);
		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, i*21, pbuf + i*21, 21);
	}
	offset = i*21;
	if (len%21) {
		eeprom_debug("i=%d\n", i++);
		hf_i2c_mem_write(&hi2c1, AT24C_ADDR, offset, pbuf + offset, len%21);
	}

	/* */
	for (i = 4; i <= len; i += 4) {
		// hf_i2c_mem_write(&hi2c1, AT24C_ADDR, 0, (uint8_t *)test_buf_A, i);
		eeprom_debug("---read len %d---\n", i);
		ret = hf_i2c_mem_read(&hi2c1, AT24C_ADDR, 0, (uint8_t *)test_buf_B, i);
		if (ret) {
			eeprom_debug("Err:EEPROM read failed!!!\n");
			err++;
			break;
		}
		if (0 != memcmp(test_buf_A, test_buf_B, i)) {
			err++;
			eeprom_debug("Err:compared len %d failed!!!\n", i);
			eeprom_debug("read %d data(hex):\n", i);
			print_data((uint8_t *)test_buf_B, i);
		}
	}


	if (!err) {
		eeprom_debug("OK:EEPROM write & read test successfully!!!\n");
		print_data((uint8_t *)test_buf_B, len);
	}
	else
		eeprom_debug("Err:EEPROM write & read test failed!!!\n");

	return 0;

}
#endif
int es_eeprom_info_test(void)
{
	#if EEPROM_TEST_DEBUG
	es_eeprom_test();
	#else
	uint8_t mac[6];
	uint8_t ipaddr[4];
	uint8_t netmask[4];
	uint8_t gateway[4];
	char admin_name[32] = {0}, admin_password[32]={0};
	int pwr_last_state = 0;
	int dip_switch_soft_ctl_attr = 0;
	uint8_t dip_switch_soft_state = 0;
	CarrierBoardInfo carrier_board_info;

	eeprom_debug("\n----es_eeprom_info_test-----\n");
	eeprom_debug("CarrierBoardInfo offset:0x%x\n", CARRIER_BOARD_INFO_EEPROM_MAIN_OFFSET);
	eeprom_debug("MCUServerInfo offset:0x%x\n", MCU_SERVER_INFO_EEPROM_OFFSET);
	eeprom_debug("SomPwrMgtDIPInfo offset:0x%x\n", SOM_PWRMGT_DIP_INFO_EEPROM_OFFSET);
	/* carrierboad info test */
	es_get_carrier_borad_info(&carrier_board_info);
	eeprom_debug("Get magicNumber:0x%lx\n", carrier_board_info.magicNumber);

	/* admin info test */
	es_get_username_password(admin_name, admin_password);
	eeprom_debug("Get admin_name:%s, admin_password:%s\n", admin_name, admin_password);

	/* mac test */
	es_get_mcu_mac(mac, MCU_MAC_IDX);
	eeprom_debug("Get mac:%x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	mac[0] = 0xde; // original MAC_ADDR0 0x94U
	es_set_mcu_mac(mac, MCU_MAC_IDX);
	eeprom_debug("Set mac:%x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	es_get_mcu_mac(mac, MCU_MAC_IDX);
	eeprom_debug("Get mac:%x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	/* ipaddr test */
	es_get_mcu_ipaddr(ipaddr);
	eeprom_debug("Get ipaddr:%d:%d:%d:%d\n", ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);

	ipaddr[3] = 68; // original IP_ADDR3 30U
	es_set_mcu_ipaddr(ipaddr);
	eeprom_debug("Set ipaddr:%d:%d:%d:%d\n", ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
	es_get_mcu_ipaddr(ipaddr);
	eeprom_debug("Get ipaddr:%d:%d:%d:%d\n", ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);

	/* netmask test */
	es_get_mcu_netmask(netmask);
	eeprom_debug("Get netmask:%d:%d:%d:%d\n", netmask[0], netmask[1], netmask[2], netmask[3]);

	netmask[3] = 1; // original NETMASK_ADDR3 0U
	es_set_mcu_netmask(netmask);
	eeprom_debug("Set netmask:%d:%d:%d:%d\n", netmask[0], netmask[1], netmask[2], netmask[3]);
	es_get_mcu_netmask(netmask);
	eeprom_debug("Get netmask:%d:%d:%d:%d\n", netmask[0], netmask[1], netmask[2], netmask[3]);

	/* gateway test */
	es_get_mcu_gateway(gateway);
	eeprom_debug("Get gateway:%d:%d:%d:%d\n", gateway[0], gateway[1], gateway[2], gateway[3]);

	gateway[3] = 2;
	es_set_mcu_gateway(gateway);
	eeprom_debug("Set gateway:%d:%d:%d:%d\n", gateway[0], gateway[1], gateway[2], gateway[3]);
	es_get_mcu_gateway(gateway);
	eeprom_debug("Get gateway:%d:%d:%d:%d\n", gateway[0], gateway[1], gateway[2], gateway[3]);

	/* pwr lost resume attr test */
	if (is_som_pwr_lost_resume()) {
		eeprom_debug("isSomPwrLostResume==TRUE\n");
		es_set_som_pwr_lost_resume_attr(0);
		eeprom_debug("Set lost resume attr to FALSE\n");
		if (is_som_pwr_lost_resume()) {
			eeprom_debug("Err!!! lost resume attr should be FALSE\n");
		}
	}
	else {
		eeprom_debug("isSomPwrLostResume==FALSE\n");
		es_set_som_pwr_lost_resume_attr(1);
		eeprom_debug("Set lost resume attr to TRUE\n");
		if (!is_som_pwr_lost_resume()) {
			eeprom_debug("Err!!! lost resume attr should be TRUE\n");
		}
	}

	/* pwr last state test */
	es_get_som_pwr_last_state(&pwr_last_state);
	eeprom_debug("Get pwr last state:%d\n", pwr_last_state);
	if (pwr_last_state)
		pwr_last_state = 0;
	else
		pwr_last_state = 1;
	es_set_som_pwr_last_state(pwr_last_state);
	eeprom_debug("Set pwr last state:%d\n", pwr_last_state);
	es_get_som_pwr_last_state(&pwr_last_state);
	eeprom_debug("Get pwr last state:%d\n", pwr_last_state);

	/* dip switch soft ctl attr test */
	es_get_som_dip_switch_soft_ctl_attr(&dip_switch_soft_ctl_attr);
	eeprom_debug("Get dip switch soft ctl attr:%d\n", dip_switch_soft_ctl_attr);
	if (dip_switch_soft_ctl_attr)
		dip_switch_soft_ctl_attr = 0;
	else
		dip_switch_soft_ctl_attr = 1;
	es_set_som_dip_switch_soft_ctl_attr(dip_switch_soft_ctl_attr);
	eeprom_debug("Set dip switch soft ctl attr:%d\n", dip_switch_soft_ctl_attr);
	es_get_som_dip_switch_soft_ctl_attr(&dip_switch_soft_ctl_attr);
	eeprom_debug("Get dip switch soft ctl attr:%d\n", dip_switch_soft_ctl_attr);

	/* dip swtich soft state test */
	es_get_som_dip_switch_soft_state(&dip_switch_soft_state);
	eeprom_debug("Get dip switch soft state:0x%x\n", dip_switch_soft_state);
	if (dip_switch_soft_state)
		dip_switch_soft_state = 0x0;
	else
		dip_switch_soft_state = 0x1;
	es_set_som_dip_switch_soft_state(dip_switch_soft_state);
	eeprom_debug("Set dip switch soft state:0x%x\n", dip_switch_soft_state);
	es_get_som_dip_switch_soft_state(&dip_switch_soft_state);
	eeprom_debug("Get dip switch soft state:0x%x\n", dip_switch_soft_state);

	#if SET_MAGIC_NUM_DEBUG
	carrier_board_info.magicNumber = 0;
	es_set_carrier_borad_info(&carrier_board_info);
	eeprom_debug("Set magicNumber:0x%lx\n", carrier_board_info.magicNumber);
	#endif

	#endif
	return 0;
}

/**
 * @brief  eic7700 boot sel.
 * @param  sel 4'b, bit3:bootsel3 ,bit2:bootsel2; bit1:bootsel2; bit0:bootsel0
 * @retval None
 */
void set_bootsel(uint8_t is_soft_crtl, uint8_t sel)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	if(is_soft_crtl){
		/*Configure GPIO pins : BOOT_SEL0_Pin BOOT_SEL1_Pin BOOT_SEL2_Pin BOOT_SEL3_Pin*/
		GPIO_InitStruct.Pin = BOOT_SEL0_Pin | BOOT_SEL1_Pin | BOOT_SEL2_Pin | BOOT_SEL3_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
		uint16_t pin_n = BOOT_SEL0_Pin;
		for (int i = 0; i < 4; i++) {
			if (sel & 0x1)
				HAL_GPIO_WritePin(BOOT_SEL0_GPIO_Port, pin_n, GPIO_PIN_SET);
			else
				HAL_GPIO_WritePin(BOOT_SEL0_GPIO_Port, pin_n, GPIO_PIN_RESET);
			sel = sel >> 1;
			pin_n = pin_n << 1;
		}
	}
	else {
		/*Configure GPIO pins : BOOT_SEL0_Pin BOOT_SEL1_Pin BOOT_SEL2_Pin BOOT_SEL3_Pin*/
		GPIO_InitStruct.Pin = BOOT_SEL0_Pin | BOOT_SEL1_Pin | BOOT_SEL2_Pin | BOOT_SEL3_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);
}

int get_bootsel(int *pCtl_attr, uint8_t *pSel)
{
	uint8_t bootsel = 0;

	es_get_som_dip_switch_soft_state_all(pCtl_attr, pSel);

	/* if it's controlled by hardware, then read the gpio pin */
	if (*pCtl_attr == 0) {
		*pSel = 0;
		uint16_t pin_n = BOOT_SEL0_Pin;
		for (int i = 0; i < 4; i++) {
			bootsel = HAL_GPIO_ReadPin(BOOT_SEL0_GPIO_Port, pin_n);
			*pSel |= (bootsel << i);
			pin_n = pin_n << 1;
		}
	}

	return 0;
}

/* This API must be called after es_init_info_in_eeprom() */
int init_bootsel(void)
{
	int dip_switch_soft_ctl_attr;
	uint8_t dip_switch_soft_state;

	es_get_som_dip_switch_soft_state_all(&dip_switch_soft_ctl_attr, &dip_switch_soft_state);

	set_bootsel(dip_switch_soft_ctl_attr, dip_switch_soft_state);

	return 0;
}

int get_dip_switch(DIPSwitchInfo *dipSwitchInfo)
{
	uint8_t som_dip_switch_state;

	get_bootsel(&dipSwitchInfo->swctrl, &som_dip_switch_state);

	dipSwitchInfo->dip01 = 0x1 & som_dip_switch_state;
	dipSwitchInfo->dip02 = (0x2 & som_dip_switch_state) >> 1;
	dipSwitchInfo->dip03 = (0x4 & som_dip_switch_state) >> 2;
	dipSwitchInfo->dip04 = (0x8 & som_dip_switch_state) >> 3;

	return 0;
}

int set_dip_switch(DIPSwitchInfo dipSwitchInfo)
{
	uint8_t som_dip_switch_state;
	int dip_switch_soft_ctl_attr = 0;

	som_dip_switch_state =  ((0x1 & dipSwitchInfo.dip04) << 3) | ((0x1 & dipSwitchInfo.dip03) << 2)
				| ((0x1 & dipSwitchInfo.dip02) << 1)| (0x1 & dipSwitchInfo.dip01);

	es_get_som_dip_switch_soft_ctl_attr(&dip_switch_soft_ctl_attr);

	if (dipSwitchInfo.swctrl != dip_switch_soft_ctl_attr) {
		/* if it's controlled by hardware */
		if (dipSwitchInfo.swctrl == 0) {
			set_bootsel(0, 0);
			es_set_som_dip_switch_soft_ctl_attr(0);
		}
		else {
			set_bootsel(1, som_dip_switch_state);
			es_set_som_dip_switch_soft_state_all(dipSwitchInfo.swctrl, som_dip_switch_state);
		}
	}
	else {
		/* if it's controlled by software */
		if (dipSwitchInfo.swctrl == 1) {
			set_bootsel(1, som_dip_switch_state);
			/* since ctl attr is not changed, only needs to update software state */
			es_set_som_dip_switch_soft_state(som_dip_switch_state);
		}
	}

	return 0;
}

int32_t es_set_rtc_date(struct rtc_date_t *sdate)
{
	RTC_DateTypeDef sDate = {0};
	sDate.Year = sdate->Year - 2000;
	sDate.Month = sdate->Month;
	sDate.Date = sdate->Date;
	sDate.WeekDay = sdate->WeekDay;

	printf("yy/mm/dd  %04d/%02d/%02d %02d\r\n", sDate.Year + 2000, sDate.Month, sDate.Date,sDate.WeekDay);
	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
		esEXIT_CRITICAL(gEEPROM_Mutex);
		return HAL_ERROR;
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);
	return HAL_OK;
}

int32_t es_set_rtc_time(struct rtc_time_t *stime)
{
	RTC_TimeTypeDef sTime = {0};
	sTime.Hours = stime->Hours;
	sTime.Minutes = stime->Minutes;
	sTime.Seconds = stime->Seconds;
	sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTime.StoreOperation = RTC_STOREOPERATION_SET;
	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	// printf("%s hh:mm:ss %02d:%02d:%02d\r\n", __func__, sTime.Hours, sTime.Minutes,sTime.Seconds);
	if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
		esEXIT_CRITICAL(gEEPROM_Mutex);
		return HAL_ERROR;
	}
	esEXIT_CRITICAL(gEEPROM_Mutex);
	return HAL_OK;
}

int32_t es_get_rtc_date(struct rtc_date_t *sdate)
{
	RTC_DateTypeDef GetData;
	RTC_TimeTypeDef GetTime;
	if (HAL_RTC_GetTime(&hrtc, &GetTime, RTC_FORMAT_BIN) != HAL_OK)
		return HAL_ERROR;
	if (HAL_RTC_GetDate(&hrtc, &GetData, RTC_FORMAT_BIN) != HAL_OK)
		return HAL_ERROR;
	// printf("yy/mm/dd  %02d/%02d/%02d\r\n", 2000 + GetData.Year, GetData.Month, GetData.Date);
	sdate->Year = 2000 + GetData.Year;
	sdate->Month = GetData.Month;
	sdate->Date = GetData.Date;
	sdate->WeekDay = GetData.WeekDay;
	return HAL_OK;
}

int32_t es_get_rtc_time(struct rtc_time_t *stime)
{
	RTC_TimeTypeDef GetTime;
	RTC_DateTypeDef GetData;

	if (HAL_RTC_GetTime(&hrtc, &GetTime, RTC_FORMAT_BIN) != HAL_OK)
		return HAL_ERROR;
	if (HAL_RTC_GetDate(&hrtc, &GetData, RTC_FORMAT_BIN) != HAL_OK)
		return HAL_ERROR;
	stime->Hours = GetTime.Hours;
	stime->Minutes = GetTime.Minutes;
	stime->Seconds = GetTime.Seconds;
	// printf("%s hh:mm:ss %02d:%02d:%02d\r\n", __func__, stime->Hours, stime->Minutes, stime->Seconds);
	return HAL_OK;
}

uint32_t es_autoboot(void)
{
	int som_pwr_last_state = 0;
	if(is_som_pwr_lost_resume() && !es_get_som_pwr_last_state(&som_pwr_last_state)) {
		if (som_pwr_last_state){
			printf("pwr enable and last state is power on\n");
			return 1;
		}
		else {
			printf("pwr enable and last state is power off\n");

		}
	}
	return 0;
}
led_status_t led_status_type = LED_MCU_RUNING;
int get_mcu_led_status(void)
{
	return led_status_type;
}

void set_mcu_led_status(led_status_t type)
{
	led_status_type = type;
	if( LED_MCU_RUNING == led_status_type) {
		HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
		HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
		HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);
	} else if( LED_SOM_BOOTING == type) {
		HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
		HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
		HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);
	} else if( LED_SOM_KERNEL_RUNING == led_status_type) {
		if((HAL_TIM_CHANNEL_STATE_BUSY == HAL_TIM_GetChannelState(&htim1, TIM_CHANNEL_2)) || 
			    HAL_TIM_CHANNEL_STATE_BUSY == HAL_TIM_GetChannelState(&htim1, TIM_CHANNEL_1)) {
			HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
			HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
			HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
		} else {
			if(HAL_TIM_CHANNEL_STATE_BUSY == HAL_TIM_GetChannelState(&htim1, TIM_CHANNEL_3))
				HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);
			else
				HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
		}
	} else if( LED_USER_INFO_RESET == led_status_type) {
		HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
		HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
		HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
	}
}

int es_restore_userdata_to_factory(void)
{
	struct ip_t ip;
	struct netmask_t netmask;
	struct getway_t gw;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);

	/* restore MCU server info */
	memset(&gMCU_Server_Info, 0, sizeof(gMCU_Server_Info));
	strcpy(gMCU_Server_Info.AdminName, DEFAULT_ADMIN_NAME);
	strcpy(gMCU_Server_Info.AdminPassword, DEFAULT_ADMIN_PASSWORD);

	gMCU_Server_Info.ip_address[0] = IP_ADDR0;
	gMCU_Server_Info.ip_address[1] = IP_ADDR1;
	gMCU_Server_Info.ip_address[2] = IP_ADDR2;
	gMCU_Server_Info.ip_address[3] = IP_ADDR3;

	gMCU_Server_Info.netmask_address[0] = NETMASK_ADDR0;
	gMCU_Server_Info.netmask_address[1] = NETMASK_ADDR1;
	gMCU_Server_Info.netmask_address[2] = NETMASK_ADDR2;
	gMCU_Server_Info.netmask_address[3] = NETMASK_ADDR3;

	gMCU_Server_Info.gateway_address[0] = GATEWAY_ADDR0;
	gMCU_Server_Info.gateway_address[1] = GATEWAY_ADDR1;
	gMCU_Server_Info.gateway_address[2] = GATEWAY_ADDR2;
	gMCU_Server_Info.gateway_address[3] = GATEWAY_ADDR3;

	gMCU_Server_Info.crc32Checksum = hf_crc32((uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);

	hf_i2c_mem_write(&hi2c1, AT24C_ADDR, MCU_SERVER_INFO_EEPROM_OFFSET,
		(uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));

	/* restore power and dip info */
	gSOM_PwgMgtDIP_Info.som_pwr_lost_resume_attr = SOM_PWR_LOST_RESUME_DISABLE;
	gSOM_PwgMgtDIP_Info.som_pwr_last_state = SOM_PWR_LAST_STATE_OFF;
	gSOM_PwgMgtDIP_Info.som_dip_switch_soft_ctl_attr = SOM_DIP_SWITCH_SOFT_CTL_DISABLE;
	gSOM_PwgMgtDIP_Info.som_dip_switch_soft_state = SOM_DIP_SWITCH_STATE_EMMC;
	gSOM_PwgMgtDIP_Info.crc32Checksum = hf_crc32((uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(gSOM_PwgMgtDIP_Info) - 4);

	hf_i2c_mem_write(&hi2c1, AT24C_ADDR, SOM_PWRMGT_DIP_INFO_EEPROM_OFFSET,
		(uint8_t *)&gSOM_PwgMgtDIP_Info, sizeof(SomPwrMgtDIPInfo));


	/* set bootsel to factor setting: controlled by hardware*/
	set_bootsel(0, 0x1);

	/* dynamically validate network settings */
	ip.ip_addr0 = 0xff & IP_ADDR0;
	ip.ip_addr1 = 0xff & (IP_ADDR1 >> 8);
	ip.ip_addr2 = 0xff & (IP_ADDR2 >> 16);
	ip.ip_addr3 = 0xff & (IP_ADDR3 >> 24);

	netmask.netmask_addr0 = 0xff & NETMASK_ADDR0;
	netmask.netmask_addr1 = 0xff & (NETMASK_ADDR1 >> 8);
	netmask.netmask_addr2 = 0xff & (NETMASK_ADDR2 >> 16);
	netmask.netmask_addr3 = 0xff & (NETMASK_ADDR3 >> 24);

	gw.getway_addr0 = 0xff & GATEWAY_ADDR0;
	gw.getway_addr1 = 0xff & (GATEWAY_ADDR1 >> 8);
	gw.getway_addr2 = 0xff & (GATEWAY_ADDR2 >> 16);
	gw.getway_addr3 = 0xff & (GATEWAY_ADDR3 >> 24);

	es_set_eth(&ip, &netmask, &gw, NULL);

	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

/* get the som console configuration
   *p_som_console_cfg: return the som console cfg, 0 means via uart, 1 means via telnet
*/
int es_get_som_console_cfg(int *p_som_console_cfg)
{
	if (NULL == p_som_console_cfg)
		return -1;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	*p_som_console_cfg = gSOM_ConsoleCfg;
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

/* set the som console configuration
   som_console_cfg: som console cfg, 0 means via uart, 1 means via telnet
*/
int es_set_som_console_cfg(int som_console_cfg)
{
	if (som_console_cfg == 1) {
		#if 0
		if (SOM_POWER_ON != get_som_power_state()) {
			printf("Can't set SOM console to telnet, please power on SOM first!!!\n");
			return -1;
		}
		#endif
		/* Mux High Level: som debug uart is connected with mcu uart6, i.e. output to telnet */
		HAL_GPIO_WritePin(UART_MUX_SEL_GPIO_Port, UART_MUX_SEL_Pin, GPIO_PIN_SET);
	}
	else {
		/* Mux Low  Level: som debug uart is connect with FT4232 UART, i.e. output to serial port */
		HAL_GPIO_WritePin(UART_MUX_SEL_GPIO_Port, UART_MUX_SEL_Pin, GPIO_PIN_RESET);
	}

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	gSOM_ConsoleCfg = som_console_cfg;
	esEXIT_CRITICAL(gEEPROM_Mutex);

	return 0;
}

/* This API is called every time when som is powered up */
int es_check_carrier_board_info(void)
{
	int ret = 0;
	uint32_t crc32Checksum;
	CarrierBoardInfo CbinfoMain;
	CarrierBoardInfo CbinfoBackup;
	uint8_t reg_addr = 0;

	esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
	/* read main partintion */
	reg_addr = CARRIER_BOARD_INFO_EEPROM_MAIN_OFFSET;
	ret = hf_i2c_mem_read(&hi2c1, AT24C_ADDR, reg_addr,
				(uint8_t *)&CbinfoMain, sizeof(CarrierBoardInfo));
	if(ret) {
		printf("Failed to read cbinfo_main\n");
		goto out;
	}
	print_cbinfo(&CbinfoMain);
	print_data((uint8_t *)&CbinfoMain, sizeof(CarrierBoardInfo));
	/* calculate crc32 checksum of main partition */
	crc32Checksum = hf_crc32((uint8_t *)&CbinfoMain, sizeof(CarrierBoardInfo) - 4);
	if (crc32Checksum != CbinfoMain.crc32Checksum) { //main partition is bad
		printf("Bad main checksum,0x%lx is NOT equal to calculated value:0x%lx\n", CbinfoMain.crc32Checksum, crc32Checksum);
		eeprom_debug("%s:%d\n", __func__, __LINE__);
		print_data((uint8_t *)&CbinfoMain, sizeof(CarrierBoardInfo));
		/* try to read and check the backup partition */
		reg_addr = CARRIER_BOARD_INFO_EEPROM_BACKUP_OFFSET;
		ret = hf_i2c_mem_read(&hi2c1, AT24C_ADDR, reg_addr,
					(uint8_t *)&CbinfoBackup, sizeof(CarrierBoardInfo));
		if(ret) {
			printf("Failed to read cbinfo_backup\n");
			goto out;
		}

		crc32Checksum = hf_crc32((uint8_t *)&CbinfoBackup, sizeof(CarrierBoardInfo) - 4);
		if (crc32Checksum != CbinfoBackup.crc32Checksum) { // backup patition is also bad
			/* restore to factory settings */
			printf("Bad backup checksum,0x%lx is also NOT equal to calculated value:0x%lx\n", CbinfoBackup.crc32Checksum, crc32Checksum);
			restore_cbinfo_to_factory_without_mutext(&gCarrier_Board_Info);
			printf("Restored cbinfo to factory settings\n");
		}
		else { // backup partion is ok
			/* recover the main partition with the backup value */
			printf("Recover main with backup settings\n");
			/* update the global variable gCarrier_Board_Info with the backup value */
			memcpy(&gCarrier_Board_Info, &CbinfoBackup, sizeof(CarrierBoardInfo));
			/* recover main partion with the values of backup */
			reg_addr = CARRIER_BOARD_INFO_EEPROM_MAIN_OFFSET;
			ret = hf_i2c_mem_write(&hi2c1, AT24C_ADDR, reg_addr,
						(uint8_t *)&CbinfoBackup, sizeof(CarrierBoardInfo));
			if(ret) {
				printf("Failed to write cbinfo_main\n");
				goto out;
			}
		}
	}
	else { // main partition is ok
		/* check backup partion, if it's bad, recover it with main value */
		reg_addr = CARRIER_BOARD_INFO_EEPROM_BACKUP_OFFSET;
		ret = hf_i2c_mem_read(&hi2c1, AT24C_ADDR, reg_addr,
					(uint8_t *)&CbinfoBackup, sizeof(CarrierBoardInfo));
		if(ret) {
			printf("Failed to read cbinfo_backup\n");
			goto out;
		}
		print_cbinfo(&CbinfoBackup);
		crc32Checksum = hf_crc32((uint8_t *)&CbinfoBackup, sizeof(CarrierBoardInfo) - 4);
		if (crc32Checksum != CbinfoBackup.crc32Checksum) {
			/* recover the backup partition with the main value */
			printf("Bad backup checksum,0x%lx is NOT equal to calculated value:0x%lx\n", CbinfoBackup.crc32Checksum, crc32Checksum);
			printf("Recover backup with main settings\n");
			reg_addr = CARRIER_BOARD_INFO_EEPROM_BACKUP_OFFSET;
			ret = hf_i2c_mem_write(&hi2c1, AT24C_ADDR, reg_addr,
						(uint8_t *)&CbinfoMain, sizeof(CarrierBoardInfo));
			if(ret) {
				printf("Failed to write cbinfo_backup\n");
				goto out;
			}
		}
		else {
			printf("cbinfo checksum ok!!!!\n");
		}
	}
out:
	esEXIT_CRITICAL(gEEPROM_Mutex);
	return ret;
}