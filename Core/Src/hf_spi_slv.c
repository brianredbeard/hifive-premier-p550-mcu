#include "main.h"
#include <stdio.h>
#include "hf_spi_slv.h"
#include "stm32f4xx_hal.h"

unsigned int get_regval(unsigned char reg);

HAL_StatusTypeDef spi_transmit(uint8_t *pData, uint16_t Size)
{
	HAL_StatusTypeDef ret;
	SPI2_FLASH_CS_LOW();
	ret = HAL_SPI_Transmit(&hspi2, pData, Size, 0xff);
	SPI2_FLASH_CS_HIGH();
	return ret;
}

HAL_StatusTypeDef spi_transmit_recive(uint8_t *pSndData, uint16_t SndSize, uint8_t *pRcvData, uint16_t RcvSize)
{
	HAL_StatusTypeDef ret;
	SPI2_FLASH_CS_LOW();
	ret = HAL_SPI_Transmit(&hspi2, pSndData, SndSize, 0xff);
	if (ret != HAL_OK) {
		return ret;
	}
	ret = HAL_SPI_Receive(&hspi2, pRcvData, RcvSize, 0xff);
	SPI2_FLASH_CS_HIGH();
	return ret;
}

HAL_StatusTypeDef spi_recive(uint8_t *pData, uint16_t Size)
{
	HAL_StatusTypeDef ret;
	SPI2_FLASH_CS_LOW();
	ret = HAL_SPI_Receive(&hspi2, pData, Size, 0xff);
	SPI2_FLASH_CS_HIGH();
	return ret;
}
void spi_addr_cfg(unsigned char *cmd, unsigned long addr)
{
	cmd[5] = (addr & 0xff);
	cmd[4] = ((addr >> 8) & 0xff);
	cmd[3] = ((addr >> 16) & 0xff);
	cmd[2] = ((addr >> 24) & 0xff);
	cmd[1] = ((addr >> 32) & 0xff);
	cmd[0] = ((addr >> 40) & 0xff);
}

void spi_data_cfg(unsigned char *cmd, unsigned char *data, int len)
{
	int i;
	unsigned char xdata;
	switch (len) {
	case 1:
		cmd[0] = data[0];
		break;
	case 2:
	case 4:
		for (i = 0; i < len; i++) {
			cmd[i] = data[len - i - 1];
		}
		break;
	case 32:
		for (i = 0; i < len; i += 4) {
			cmd[i] = data[i + 3];
			cmd[i + 1] = data[i + 2];
			cmd[i + 2] = data[i + 1];
			cmd[i + 3] = data[i];
		}
		break;
	}
}

int eswin_ww(unsigned long addr, unsigned int data)
{
	unsigned char sndBuf[32];
	HAL_StatusTypeDef ret;
	// AXI word write: cmd(1B)+addr(6B)+data(4B)
	sndBuf[0] = 0x1;
	spi_addr_cfg(&sndBuf[1], addr);
	spi_data_cfg(&sndBuf[7], (unsigned char *)&data, 4);
	ret = spi_transmit(sndBuf, 11);
	return ret;
}

int eswin_wb(unsigned long addr, unsigned char data)
{
	int ret;
	unsigned char sndBuf[32];

	// AXI word write: cmd(1B)+addr(6B)+data(1B)
	sndBuf[0] = 0xb;
	spi_addr_cfg(&sndBuf[1], addr);
	spi_data_cfg(&sndBuf[7], (unsigned char *)&data, 1);

	ret = spi_transmit(sndBuf, 8);
	return ret;
}

int eswin_wh(unsigned long addr, unsigned short data)
{
	int ret;
	unsigned char sndBuf[32];

	// AXI word write: cmd(1B)+addr(6B)+data(2B)
	sndBuf[0] = 0x9;
	spi_addr_cfg(&sndBuf[1], addr);
	spi_data_cfg(&sndBuf[7], (unsigned char *)&data, 2);

	ret = spi_transmit(sndBuf, 9);
	return ret;
}

int eswin_burst_write(unsigned long addr, unsigned char *data)
{
	// AXI burst write: cmd(1B)+addr(6B)+data(32B)
	int ret;
	unsigned char sndBuf[39];

	sndBuf[0] = 0x3;
	spi_addr_cfg(&sndBuf[1], addr);
	spi_data_cfg(&sndBuf[7], data, 32);
	ret = spi_transmit(sndBuf, 39);
	return ret;
}



void format_big_2_little(unsigned char *buf, int len)
{
	unsigned char data;
	int left = len, i = 0;

	if (len == 1)
		return;

	switch (len) {
	case 2:
	case 4:
		for (i = 0; i < len / 2; i++) {
			data = buf[i];
			buf[i] = buf[len - i - 1];
			buf[len - i - 1] = data;
		}
		break;
	case 32:
		for (i = 0; i < len; i += 4) {
			data = buf[i];
			buf[i] = buf[i + 3];
			buf[i + 3] = data;
			data = buf[i + 1];
			buf[i + 1] = buf[i + 2];
			buf[i + 2] = data;
		}
		break;
	}
}

int spi_axi_read(unsigned char *rcvBuf, int len)
{
	int ret;
	unsigned char sndBuf;

	if (len == 1) {
		sndBuf = 0x1a;
	} else if (len == 2) {
		sndBuf = 0x18;
	} else if ((len == 4) || (len == 32)) {
		sndBuf = 0x16;
	}
	ret = spi_transmit_recive(sndBuf, 1, rcvBuf, len);
	return ret;
}

int eswin_rx(unsigned char *rcvBuf, unsigned long addr, int len)
{
	int ret, xlen, offset = 0;
	unsigned char sndBuf[32];
	unsigned int rvdata = 0;
	printf("%s %x %x\n",__func__, addr, len);
	while (len) {
		if (len >= 32) {
			sndBuf[0] = 0x2;
			xlen = 32;
			len -= 32;
		} else if (len >= 4) {
			sndBuf[0] = 0x0;
			xlen = 4;
			len -= 4;
		} else if (len >= 2) {
			sndBuf[0] = 0x0;
			xlen = 2;
			len -= 2;
		} else {
			sndBuf[0] = 0x0;
			xlen = 1;
			len -= 1;
		}

		spi_addr_cfg(&sndBuf[1], addr);
		for(int i = 0; i<7; i++) {
			printf("sndBuf[%d] %x\n", i, sndBuf[i]);
		}
		if (xlen == 1) {
			sndBuf[7] = 0x1a;
		} else if (xlen == 2) {
			sndBuf[7] = 0x18;
		} else if ((xlen == 4) || (xlen == 32)) {
			sndBuf[7] = 0x16;
		}
		ret = spi_transmit(sndBuf, 7);
		if (HAL_OK != ret) {
			goto fail;
		}
		ret = spi_transmit_recive(&sndBuf[7], 1, rcvBuf + offset, xlen);
		if (ret != HAL_OK) {
			goto fail;
		}
		format_big_2_little(rcvBuf + offset, xlen);
		addr += xlen;
		offset += xlen;
	}
fail:
	return ret;
}

int es_spi_writeb(unsigned long addr, unsigned char val)
{
	return eswin_wb(addr, val);
}
int es_spi_writew(unsigned long addr, unsigned short val)
{
	return eswin_wh(addr, val);
}
int es_spi_writel(unsigned long addr, unsigned int val)
{
	return eswin_ww(addr, val);
}

int es_spi_write(unsigned char *buf, unsigned long addr, int len)
{
	int xlen, offset = 0, ret;
	printf("addr %x len %x\n",addr, len);
	while (len) {
		if (len >= 32) {
			len -= 32;
			xlen = 32;
			ret = eswin_burst_write(addr, buf + offset);
			if (ret)
				goto fail;
		} else if (len >= 4) {
			xlen = 4;
			len -= 4;
			ret = es_spi_writel(addr, *(unsigned int *)(buf + offset));
			if (ret)
				goto fail;
		} else if (len >= 2) {
			xlen = 2;
			len -= 2;
			ret = es_spi_writew(addr, *(unsigned short *)(buf + offset));
			if (ret)
				goto fail;
		} else {
			xlen = 1;
			len -= 1;
			ret = es_spi_writeb(addr, *(buf + offset));
			if (ret)
				goto fail;
		}

		addr += xlen;
		offset += xlen;
	}
fail:
	return ret;
}

unsigned char es_spi_readb(unsigned long addr)
{
	unsigned char buf[1];
	eswin_rx(buf, addr, 1);

	return *buf;
}

unsigned short es_spi_readw(unsigned long addr)
{
	unsigned char buf[2];
	eswin_rx(buf, addr, 2);

	return *(unsigned short *)buf;
}

unsigned int es_spi_readl(unsigned long addr)
{
	unsigned char buf[4];
	eswin_rx(buf, addr, 4);

	return *(unsigned int *)buf;
}

int es_spi_read(unsigned char *dst, unsigned long src, int len)
{
	return eswin_rx(dst, src, len);
}

unsigned int get_regval(unsigned char reg)
{
	unsigned char sndBuf[2];
	unsigned char rcvBuf[4];
	int ret;
	sndBuf[0] = 0x4;
	sndBuf[1] = reg;
	ret = spi_transmit_recive(sndBuf, 2, rcvBuf, 4);
	format_big_2_little(rcvBuf,4);
	// printf("reg %x 0x%08x\n",reg>>4,*(unsigned int *)rcvBuf);
	return *(unsigned int *)rcvBuf;
}