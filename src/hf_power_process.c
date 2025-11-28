/* Includes ------------------------------------------------------------------*/
#include <stdio.h>

#include "cmsis_os.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
#include "hf_common.h"
#include "hf_i2c.h"
/* Private typedef -----------------------------------------------------------*/
 #define AUTO_BOOT
/* Private define ------------------------------------------------------------*/
#define ATX_POWER_GOOD GPIO_PIN_RESET
#define ATX_POWER_FAIL GPIO_PIN_SET

#define DC_POWER_GOOD GPIO_PIN_SET
#define DC_POWER_FAIL GPIO_PIN_RESET

/* Private macro -------------------------------------------------------------*/
#define PCA9450_ADDR (0x25u << 1)
/* Private variables ---------------------------------------------------------*/
power_switch_t som_power_state = SOM_POWER_OFF;

static uint8_t get_dc_power_status(void);
static void pmic_status_led_on(uint8_t turnon);
static void atx_power_on(uint8_t turnon);
static void dc_power_on(uint8_t turnon);
static void power_led_on(uint8_t turnon);
static int pmic_b6out_105v(void);

void hf_power_task(void *parameter)
{
	HAL_StatusTypeDef status = HAL_OK;
	power_state_t power_state = IDLE_STATE;
	GPIO_PinState pin_state = GPIO_PIN_RESET;
	printf("hf_power_task started!!!\r\n");

	#ifdef AUTO_BOOT
	power_state = ATX_PS_ON_STATE;
	som_power_state = SOM_POWER_ON;
	#endif
	power_led_on(pdFALSE);
	while (1) {
		switch (power_state) {
		case ATX_PS_ON_STATE:
			printf("ATX_PS_ON_STATE\r\n");
			atx_power_on(pdTRUE);
			power_state = DC_PWR_ON_STATE;
			break;
		case DC_PWR_ON_STATE:
			printf("DC_PWR_ON_STATE\r\n");
			dc_power_on(pdTRUE);
			power_state = DC_PWR_GOOD_STATE;
			break;
		case DC_PWR_GOOD_STATE:
		printf("DC_PWR_GOOD_STATE\r\n");
		{
			int retry_count = 10;
			pin_state = GPIO_PIN_RESET;

			/* Wait for DC power good with retry */
			while (retry_count > 0) {
				osDelay(200);
				pin_state = get_dc_power_status();
				if (pin_state == pdTRUE) {
					break;
				}
				retry_count--;
			}


		/* Only initialize I2C if DC power is good */
		if (retry_count > 0) {
			i2c_init(I2C3);
			osDelay(100);
			i2c_init(I2C1);
			power_state = SOM_STATUS_CHECK_STATE;
		} else {
			printf("DC power good timeout, stopping power\r\n");
			power_state = STOP_POWER;
		}
	}
	break;
	case SOM_STATUS_CHECK_STATE:
		printf("SOM_STATUS_CHECK_STATE\r\n");
		// pmic_power_on(pdTRUE);  // Removed in patch 0078
		som_reset_control(pdFALSE);
		pmic_status_led_on(pdTRUE);
		power_led_on(pdTRUE);
		power_state = POWERON;
		printf("POWERON\r\n");
		break;
		case POWERON:
			if (som_power_state == SOM_POWER_OFF) {
				power_state = STOP_POWER;
			}
			break;
		case STOP_POWER:
			printf("STOP_POWER\r\n");
			i2c_deinit(I2C3);

			// pmic_power_on(pdFALSE);  // Removed in patch 0078
			dc_power_on(pdFALSE);
			atx_power_on(pdFALSE);
			pmic_status_led_on(pdFALSE);
			power_led_on(pdFALSE);
			som_power_state = SOM_POWER_OFF;
			power_state = IDLE_STATE;
			break;
		case IDLE_STATE:
			if (som_power_state == SOM_POWER_ON) {
				power_state = ATX_PS_ON_STATE;
			}
			break;
		}
		osDelay(100);
	}
}

/**
 * @brief  atx power switch .
 * @param  turnon turn on/off atx power 1:turnon; 0:turnoff.
 * @retval None
 */
static void atx_power_on(uint8_t turnon)
{
	if (turnon)
		HAL_GPIO_WritePin(ATX_PS_ON_GPIO_Port, ATX_PS_ON_Pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(ATX_PS_ON_GPIO_Port, ATX_PS_ON_Pin, GPIO_PIN_RESET);
}

/**
 * @brief  DC power switch.
 * @param  turnon turn on/off DC power 1:turnon; 0:turnoff.
 * @retval None
 */
static void dc_power_on(uint8_t turnon)
{
	if (turnon) {
		HAL_GPIO_WritePin(DC_POWER_EN0_GPIO_Port, DC_POWER_EN0_Pin, GPIO_PIN_SET);
	} else {
		HAL_GPIO_WritePin(DC_POWER_EN0_GPIO_Port, DC_POWER_EN0_Pin, GPIO_PIN_RESET);
	}
}

// Removed in patch 0078 - hardware changed, these GPIO pins no longer exist
#if 0
/**
 * @brief  pmic power switch.
 * @param  turnon turn on/off pmic power 1:turnon; 0:turnoff.
 * @retval None
 */
static void pmic_power_on(uint8_t turnon)
{
	if (turnon)
		HAL_GPIO_WritePin(SOM_PMIC_ON_REQ_GPIO_Port, SOM_PMIC_ON_REQ_Pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(SOM_PMIC_ON_REQ_GPIO_Port, SOM_PMIC_ON_REQ_Pin, GPIO_PIN_RESET);
}

/**
 * @brief  pmic power switch.
 * @param  None
 * @retval uint8_t atx power status. pdTRUE : atx power on; pdFALSE : atx power off
 */
static uint8_t get_atx_power_status(void)
{
	if (ATX_POWER_GOOD == HAL_GPIO_ReadPin(ATX_PWR_OK_GPIO_Port, ATX_PWR_OK_Pin))
		return pdTRUE;
	else
		return pdFALSE;
}
#endif

/**
 * @brief  dcdc power switch.
 * @param  None
 * @retval uint8_t dcdc power  status. pdTRUE : dcdc power  on; pdFALSE : dcdc power off
 */
static uint8_t get_dc_power_status(void)
{
	if (DC_POWER_GOOD == HAL_GPIO_ReadPin(DCDC_PWR_OK_GPIO_Port, DCDC_PWR_OK_Pin))
		return pdTRUE;
	else
		return pdFALSE;
}

/**
 * @brief  pmic status led switch.
 * @param  turnon pmic status led on/off; pdTRUE : led on; pdFALSE : led off
 * @retval None
 */
static void pmic_status_led_on(uint8_t turnon)
{
	if (turnon) {
		if (HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3) != HAL_OK)
			printf("HAL_TIM_PWM_Start(&htim4,TIM_CHANNEL_3) err\n");
	} else {
		if (HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_3) != HAL_OK)
			printf("HAL_TIM_PWM_Start(&htim4,TIM_CHANNEL_3) err\n");
	}
}

/**
 * @brief  mcu reset som control.
 * @param  reset pdTRUE : reset; pdFALSE : release
 * @retval None
 */
// HAL_GPIO_WritePin(MCU_RESET_SOM_N_GPIO_Port,
// MCU_RESET_SOM_N_Pin, GPIO_PIN_SET); GPIO_InitTypeDef
// GPIO_InitStruct = {0}; GPIO_InitStruct.Pin =
// MCU_RESET_SOM_N_Pin; GPIO_InitStruct.Pull = GPIO_NOPULL;
// GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
// GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
// HAL_GPIO_Init(MCU_RESET_SOM_N_GPIO_Port, &GPIO_InitStruct);

power_switch_t get_som_power_state(void)
{
	power_switch_t state;

	taskENTER_CRITICAL();
	state = som_power_state;
	taskEXIT_CRITICAL();
	return state;
}

void change_som_power_state(power_switch_t newState)
{
	taskENTER_CRITICAL();
	som_power_state = newState;
	taskEXIT_CRITICAL();
}

void som_reset_control(uint8_t reset)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	if (reset) {
		GPIO_InitStruct.Pin = MCU_RESET_SOM_N_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(MCU_RESET_SOM_N_GPIO_Port, &GPIO_InitStruct);
		HAL_GPIO_WritePin(MCU_RESET_SOM_N_GPIO_Port, MCU_RESET_SOM_N_Pin, GPIO_PIN_RESET);
	} else {
		HAL_GPIO_WritePin(MCU_RESET_SOM_N_GPIO_Port, MCU_RESET_SOM_N_Pin, GPIO_PIN_SET);
		GPIO_InitStruct.Pin = MCU_RESET_SOM_N_Pin;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(MCU_RESET_SOM_N_GPIO_Port, &GPIO_InitStruct);
	}
}

/**
 * @brief  power led control.
 * @param  turnon pdTRUE : power led on; pdFALSE : power led off
 * @retval None
 */
static void power_led_on(uint8_t turnon)
{
	if (turnon) {
		HAL_GPIO_WritePin(PWR_LED_GPIO_Port, PWR_LED_Pin, GPIO_PIN_SET);
	} else {
		HAL_GPIO_WritePin(PWR_LED_GPIO_Port, PWR_LED_Pin, GPIO_PIN_RESET);
	}
}

/**
 * @brief  set pmic buck6 output voltage to 1.05v.
 * @param  None
 * @retval int
 */
static int pmic_b6out_105v(void)
{
	uint8_t reg_add = 0x1e, reg_dat = 0x12;
	return hf_i2c_reg_write(&hi2c3, PCA9450_ADDR, reg_add, &reg_dat);
}

/**
 * @brief  power led control.
 * @param  sel pdTRUE : power led on; pdFALSE : power led off
 * @retval None
 */
void bootsel(uint8_t sel)
{
	/*Configure GPIO pins : BOOT_SEL0_Pin BOOT_SEL1_Pin BOOT_SEL2_Pin BOOT_SEL3_Pin*/
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = BOOT_SEL0_Pin | BOOT_SEL1_Pin | BOOT_SEL2_Pin | BOOT_SEL3_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
	uint16_t pin_n = BOOT_SEL0_Pin;
	for (int i = 0; i < 4; i++) {
		if (sel & 0x1)
			HAL_GPIO_WritePin(BOOT_SEL0_GPIO_Port, BOOT_SEL0_Pin, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(BOOT_SEL0_GPIO_Port, BOOT_SEL0_Pin, GPIO_PIN_RESET);
		sel = sel >> 1;
		pin_n = pin_n << 1;
	}
	// HAL_GPIO_WritePin(BOOT_SEL0_GPIO_Port,BOOT_SEL0_Pin,GPIO_PIN_SET);
	// HAL_GPIO_WritePin(BOOT_SEL1_GPIO_Port,BOOT_SEL1_Pin,GPIO_PIN_RESET);
	// HAL_GPIO_WritePin(BOOT_SEL2_GPIO_Port,BOOT_SEL2_Pin,GPIO_PIN_RESET);
	// HAL_GPIO_WritePin(BOOT_SEL3_GPIO_Port,BOOT_SEL3_Pin,GPIO_PIN_RESET);
}