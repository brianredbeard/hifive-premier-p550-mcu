# Patch 0004: Production Test Bug Fixes - System Refinements

## Metadata
- **Patch File**: `0004-WIN2030-15279-fix-production-test-bug.patch`
- **Author**: xuxiang <xuxiang@eswincomputing.com>
- **Date**: Wed, 24 Apr 2024 10:23:30 +0800
- **Ticket**: WIN2030-15279
- **Type**: fix (Bug Fix)
- **Change-Id**: I54b261c4e50f349b9d33f40566f3eaa4afac0049

## Summary

This patch addresses several critical bugs and configuration issues discovered during production testing of patch 0002's test framework. It implements three major improvements: dynamic network configuration without task termination, RTC format standardization from BIN to BCD, and HAL timebase migration from TIM1 to TIM2 to resolve conflicts. These changes improve system stability, real-time clock accuracy, and production test reliability.

## Changelog

Official changelog from commit message:
1. **Dynamic change of ip address, netmask, and gateway** - Network reconfiguration without HTTP task restart
2. **RTC data, times format** - Standardized RTC format to BCD (Binary-Coded Decimal)
3. **Sys timebase change to TIM2** - Migrated HAL tick generation from TIM1 to TIM2

## Statistics

- **Files Changed**: 10 files
- **Insertions**: 196 lines
- **Deletions**: 68 lines
- **Net Change**: +128 lines

This represents critical bug fixes and system improvements that enhance production test reliability and runtime reconfigurability.

## Detailed Changes by Subsystem

### 1. Interrupt Handler Declarations (`Core/Inc/stm32f4xx_it.h`)

**Statistics**: 1 line added

**New Declaration**:
```c
void TIM2_IRQHandler(void);
```

**Purpose**: Declares TIM2 interrupt handler for HAL timebase tick generation

**Context**: Part of migration from TIM1 to TIM2 for system tick
- **TIM1**: Advanced timer with PWM capabilities, now freed for application use
- **TIM2**: General-purpose timer suitable for HAL timebase
- **Rationale**: Avoids conflicts with application-level TIM1 PWM usage

### 2. Board Initialization Updates (`Core/Src/hf_board_init.c`)

**Statistics**: 3 lines changed (2 deletions, 1 addition)

**Changes**:

**TIM1 Handle Declaration Added**:
```c
TIM_HandleTypeDef htim1;  // Now available for application use (PWM, etc.)
```

**Debug Print Removal**:
```c
// DELETED:
// printf("\r\n%s %d %s %s\r\n", __func__, __LINE__, __DATE__, __TIME__);
```

**Analysis**:

**TIM1 Availability**:
- TIM1 no longer used for HAL timebase (moved to TIM2)
- TIM1 now free for fan PWM control, LED dimming, or other application needs
- Advanced timer features available: complementary outputs, deadtime insertion, brake input

**Debug Cleanup**:
- Removed boot-time debug message showing function, line, compile date/time
- Reduces console noise during production testing
- Aligns with patch 0003's debug cleanup philosophy
- Information still available in firmware version string if needed

**Impact**:
- Cleaner boot sequence without unnecessary prints
- TIM1 resource freed for future enhancements (implemented in later patches for fan control)

### 3. GPIO Processing Timing Adjustment (`Core/Src/hf_gpio_process.c`)

**Statistics**: 1 line changed

**Button Debounce Timing Refinement**:
```c
// BEFORE:
#define PRESS_Time 100  // 100ms press detection window

// AFTER:
#define PRESS_Time 50   // 50ms press detection window (more responsive)
```

**Analysis**:

**Rationale for Change**:
- **User Experience**: 100ms felt sluggish during testing; 50ms more responsive
- **Debouncing**: 50ms still adequate for mechanical switch debouncing (typical bounce: 5-20ms)
- **Safety**: Short enough to detect quick button presses, long enough to filter bounce

**Context Values**:
- `SHORT_CLICK_THRESHOLD`: 50ms (unchanged) - minimum duration for valid press
- `LONG_PRESS_THRESHOLD`: 3000ms (unchanged) - force power-off after 3 seconds
- `BUTTON_ERROR_Time`: 10000ms (unchanged) - error if button stuck for 10 seconds
- **New PRESS_Time**: 50ms - polling/sampling interval

**Button State Machine Timing**:
```
Idle → Press Detected (after 50ms stable low)
     → Short Click (if released before 3000ms)
     → Long Press (if held beyond 3000ms)
```

**Production Test Impact**:
- Faster button response during automated testing
- Test execution time reduced slightly (quicker button press detection)
- More reliable press detection in noisy electrical environments

### 4. HTTP Processing - Dynamic Network Configuration (`Core/Src/hf_http_process.c`)

**Statistics**: 28 lines added (significant enhancement)

**Problem Addressed**:
In patch 0003, network configuration changes required terminating and restarting the HTTP task:
```c
// OLD APPROACH (patches 0001-0003):
if (http_task_handle)
    osThreadTerminate(http_task_handle);  // Kill HTTP task
// ... apply network changes ...
http_task_handle = osThreadNew(hf_http_task, NULL, &http_task_attributes);  // Restart
```

**Issues with Old Approach**:
1. **Active Connections Lost**: Any web sessions disconnected during restart
2. **Timing Vulnerability**: Window where HTTP server unavailable
3. **Resource Leak Risk**: Task termination may not free all resources cleanly
4. **Complexity**: Task lifecycle management error-prone

**New Solution - Dynamic Reconfiguration**:

**New Function Added**:
```c
extern struct netif gnetif;
extern ip4_addr_t ipaddr;
extern ip4_addr_t netmask;
extern ip4_addr_t gw;

void dynamic_change_eth(void)
{
    // Convert from byte array to lwIP ip4_addr_t structure
    IP4_ADDR(&ipaddr, ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    IP4_ADDR(&netmask, netmask_address[0], netmask_address[1], netmask_address[2], netmask_address[3]);
    IP4_ADDR(&gw, getway_address[0], getway_address[1], getway_address[2], getway_address[3]);

    // Apply new network configuration to active interface
    netif_set_addr(&gnetif, &ipaddr, &netmask, &gw);

    // Ensure interface is default and up
    netif_set_default(&gnetif);
    netif_set_up(&gnetif);
}
```

**lwIP API Functions Used**:
- `IP4_ADDR()`: Macro to construct `ip4_addr_t` from four octets
- `netif_set_addr()`: Atomically updates IP, netmask, and gateway on network interface
- `netif_set_default()`: Ensures this interface is default route
- `netif_set_up()`: Brings interface up if it was down

**Integration in Protocol Processing** (`hf_protocol_process.c`):
```c
int32_t es_set_eth(struct ip_t *ip, struct netmask_t *netmask,
                    struct getway_t *gw, struct eth_mac_t *mac)
{
    // REMOVED: Task termination logic
    // if (http_task_handle) osThreadTerminate(http_task_handle);

    if (ip != NULL) {
        ip_address[0] = ip->ip_addr0;
        ip_address[1] = ip->ip_addr1;
        ip_address[2] = ip->ip_addr2;
        ip_address[3] = ip->ip_addr3;
    }
    if (netmask != NULL) {
        netmask_address[0] = netmask->netmask_addr0;
        netmask_address[1] = netmask->netmask_addr1;
        netmask_address[2] = netmask->netmask_addr2;
        netmask_address[3] = netmask->netmask_addr3;
    }
    if (gw != NULL) {
        getway_address[0] = gw->getway_addr0;
        getway_address[1] = gw->getway_addr1;
        getway_address[2] = gw->getway_addr2;
        getway_address[3] = gw->getway_addr3;
    }
    if (mac != NULL) {
        // MAC address assignment (used on next interface restart)
        mac_address[0] = mac->eth_mac_addr0;
        mac_address[1] = mac->eth_mac_addr1;
        // ... (all 6 bytes)
    }

    // ADDED: Dynamic configuration without task restart
    extern void dynamic_change_eth(void);
    dynamic_change_eth();
    return HAL_OK;
}
```

**Network Initialization Enhancement** (`hf_http_task`):
```c
void hf_http_task(void *argument)
{
    eth_get_address();
    MX_LWIP_Init();
    osDelay(5000);  // Wait for DHCP negotiation

    dhcp = netif_dhcp_data(&gnetif);
    printf("Static IP address: %s\n", ip4addr_ntoa(&gnetif.ip_addr));
    printf("Subnet mask: %s\n", ip4addr_ntoa(&gnetif.netmask));
    printf("Default gateway: %s\n", ip4addr_ntoa(&gnetif.gw));

    // NEW: Store current network configuration in global variables
    memcpy((uint8_t *)ip_address, &gnetif.ip_addr, 4);
    memcpy((uint8_t *)netmask_address, &gnetif.netmask, 4);
    memcpy((uint8_t *)getway_address, &gnetif.gw, 4);

    for(;;) {
        osDelay(1000);
    }
}
```

**Benefits of Dynamic Approach**:
1. **Zero Downtime**: Web sessions remain active during network changes
2. **Immediate Effect**: New IP takes effect within milliseconds
3. **Thread Safety**: lwIP netif functions are thread-safe when called from lwIP thread context
4. **Reliability**: No task lifecycle management complexity
5. **Testing**: Simplifies production test network reconfiguration

**Limitations**:
- **MAC Address**: Requires interface down/up to change (not implemented dynamically here)
- **DHCP Switch**: Switching from static to DHCP requires more complex logic (future enhancement)
- **ARP Cache**: Old IP may remain in network ARP caches briefly

**Production Test Use Case**:
```
1. Test station configures BMC with test network (192.168.100.x)
2. Run tests
3. Reconfigure BMC to production network (customer's network)
4. All without rebooting BMC or restarting HTTP server
```

### 5. Protocol Processing Refinements (`Core/Src/hf_protocol_process.c`)

**Statistics**: 55 lines changed (significant refactoring)

**RTC Format Standardization**:

**RTC Date/Time Retrieval**:
```c
// BEFORE (patch 0003):
int32_t es_get_rtc_date(struct rtc_date_t *sdate)
{
    RTC_DateTypeDef GetData;
    if (HAL_RTC_GetDate(&hrtc, &GetData, RTC_FORMAT_BIN) != HAL_OK)  // Binary format
        return HAL_ERROR;
    sdate->Year = 2000 + GetData.Year;
    sdate->Month = GetData.Month;
    sdate->Date = GetData.Date;
    return HAL_OK;
}

// AFTER (patch 0004):
int32_t es_get_rtc_date(struct rtc_date_t *sdate)
{
    RTC_DateTypeDef GetData;
    if (HAL_RTC_GetDate(&hrtc, &GetData, RTC_FORMAT_BCD) != HAL_OK)  // BCD format
        return HAL_ERROR;
    sdate->Year = 2000 + GetData.Year;
    sdate->Month = GetData.Month;
    sdate->Date = GetData.Date;
    return HAL_OK;
}
```

**Time Retrieval Function Repositioning**:
```c
// Function moved earlier in file for better organization
int32_t es_get_rtc_time(struct rtc_time_t *stime)
{
    RTC_TimeTypeDef GetTime;
    if (HAL_RTC_GetTime(&hrtc, &GetTime, RTC_FORMAT_BCD) != HAL_OK)  // Changed to BCD
        return HAL_ERROR;
    stime->Hours = GetTime.Hours;
    stime->Minutes = GetTime.Minutes;
    stime->Seconds = GetTime.Seconds;
    return HAL_OK;
}
```

**RTC Format: BIN vs BCD**:

**BIN (Binary) Format**:
- Decimal values stored directly: 23 hours = 0x17
- Easy arithmetic: time + 1 = time_plus_one
- Human-readable in debugger

**BCD (Binary-Coded Decimal) Format**:
- Decimal digits encoded in nibbles: 23 hours = 0x23 (0010 0011)
- Each nibble represents one decimal digit (0-9)
- **Hardware Preference**: Many RTCs including STM32 internal RTC use BCD natively
- **Accuracy**: Avoids conversion errors between BIN and hardware BCD
- **Standard**: Industry standard for RTC communication

**Why BCD for STM32 RTC**:
The STM32F4 RTC peripheral stores time/date in BCD format in hardware registers. Using `RTC_FORMAT_BIN` forces HAL to:
1. Read BCD value from hardware
2. Convert BCD to binary
3. Return to application

Using `RTC_FORMAT_BCD` skips conversion:
1. Read BCD value from hardware
2. Return directly (validation only)
3. **Faster, more reliable**

**Bug Fixed by BCD**:
```c
// SCENARIO: RTC hardware shows 0x23 (23 hours in BCD)

// With RTC_FORMAT_BIN:
HAL_RTC_GetTime(&hrtc, &GetTime, RTC_FORMAT_BIN);
// HAL converts: 0x23 BCD → 2*10 + 3 = 23 decimal → returns 23
// Seems fine, BUT...

// If hardware has error/corruption and shows 0x2A (invalid BCD):
// HAL converts: 0x2A → 2*10 + 10 = 30 (INVALID! Hours only 0-23)
// OR HAL might return error, causing function failure

// With RTC_FORMAT_BCD:
HAL_RTC_GetTime(&hrtc, &GetTime, RTC_FORMAT_BCD);
// HAL validates: 0x23 is valid BCD (both nibbles 0-9), returns 0x23
// Application gets: Hours = 0x23
// Display: Hours & 0x0F + ((Hours >> 4) & 0x0F) * 10 = 23
// If 0x2A received: HAL validation catches error immediately
```

**Impact**:
- **Reliability**: Reduces conversion-related errors
- **Performance**: Eliminates BCD↔BIN conversion overhead
- **Consistency**: All RTC access now uses same format (BCD)

**Fan Duty Cycle Storage**:

**Problem**: Previous implementation returned hardcoded values instead of actual duty cycle

**Solution**:
```c
// NEW: Global variables to store actual fan duty cycles
uint32_t fan0_duty = 0;
uint32_t fan1_duty = 0;

int32_t es_set_fan_duty(struct fan_control_t *fan)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    if (fan->fan_num == 0) {
        if (HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_1) != HAL_OK)
            return HAL_ERROR;

        fan0_duty = fan->duty;  // NEW: Store actual duty cycle
        sConfigOC.OCMode = TIM_OCMODE_PWM1;
        sConfigOC.Pulse = fan->duty*pwm_period/100;  // Convert % to timer counts
        // ... configure and start PWM
    } else if (fan->fan_num == 1) {
        // Similar for fan 1
        fan1_duty = fan->duty;  // NEW: Store actual duty cycle
    }
    return HAL_OK;
}

int32_t es_get_fan_duty(struct fan_control_t *fan)
{
    // BEFORE (patch 0003): Hardcoded return values
    // if (fan->fan_num == 0) {
    //     fan->duty = 50;  // Always returned 50%!
    // } else if (fan->fan_num == 1) {
    //     fan->duty = 60;  // Always returned 60%!
    // }

    // AFTER (patch 0004): Return actual stored values
    if (fan->fan_num == 0) {
        fan->duty = fan0_duty;  // Return actual duty cycle
    } else if (fan->fan_num == 1) {
        fan->duty = fan1_duty;  // Return actual duty cycle
    } else {
        return HAL_ERROR;
    }
    return HAL_OK;
}
```

**Benefits**:
- Production tests can verify fan control actually changes duty cycle
- Web interface shows correct fan speed (not hardcoded values)
- Foundation for closed-loop fan control (read back actual duty)

### 6. Main Application RTC Format Update (`Core/Src/main.c`)

**Statistics**: 11 lines changed

**RTC Retrieval Updated**:
```c
void get_rtc_info(void)
{
    RTC_DateTypeDef GetData;
    RTC_TimeTypeDef GetTime;

    // CHANGED: BIN → BCD format
    HAL_RTC_GetTime(&hrtc, &GetTime, RTC_FORMAT_BCD);
    HAL_RTC_GetDate(&hrtc, &GetData, RTC_FORMAT_BCD);

    // Display (BCD values need conversion for printf)
    printf("yy/mm/dd  %02d/%02d/%02d\r\n",
           2000 + GetData.Year, GetData.Month, GetData.Date);
    printf("hh:mm:ss  %02d:%02d:%02d\r\n",
           GetTime.Hours, GetTime.Minutes, GetTime.Seconds);
}
```

**HAL_Delay Removal in Main Loop**:
```c
// BEFORE:
void hf_main_task(void *argument)
{
    // ... initialization ...
    for(;;)
    {
        HAL_Delay(500);  // Blocking delay - BAD in FreeRTOS task!
        // ... processing ...
    }
}

// AFTER:
void hf_main_task(void *argument)
{
    // ... initialization ...
    for(;;)
    {
        // HAL_Delay removed - task now yields properly to other tasks
        // Use osDelay() if delay needed
    }
}
```

**Why HAL_Delay Removal Critical**:
- **FreeRTOS Context**: `HAL_Delay()` is busy-wait loop, doesn't yield to scheduler
- **CPU Waste**: Blocks task for 500ms every iteration without releasing CPU
- **Priority Inversion**: High-priority task spinning prevents lower-priority tasks
- **Correct Approach**: Use `osDelay()` which yields to scheduler

**Impact**: Main task no longer hogs CPU during idle periods

### 7. HAL MSP TIM1 PWM Configuration (`Core/Src/stm32f4xx_hal_msp.c`)

**Statistics**: 79 lines added (new TIM1 MSP functions)

**New TIM1 Initialization for PWM**:

```c
/**
 * @brief TIM_PWM MSP Initialization
 * This function configures the hardware resources used in this example
 * @param htim_pwm: TIM_PWM handle pointer
 * @retval None
 */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef* htim_pwm)
{
    if(htim_pwm->Instance==TIM1)
    {
        /* Peripheral clock enable */
        __HAL_RCC_TIM1_CLK_ENABLE();

        /* TIM1 interrupt Init */
        HAL_NVIC_SetPriority(TIM1_BRK_TIM9_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(TIM1_BRK_TIM9_IRQn);
        HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM11_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn);
        HAL_NVIC_SetPriority(TIM1_CC_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);
    }
}
```

**GPIO Configuration for TIM1 PWM**:
```c
void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if(htim->Instance==TIM1)
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();

        /**TIM1 GPIO Configuration
        PE9  ------> TIM1_CH1 (Fan 0 PWM output)
        PE11 ------> TIM1_CH2 (Fan 1 PWM output or other PWM)
        */
        GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;  // Low freq sufficient for fan PWM
        GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    }
    else if(htim->Instance==TIM4)  // Existing TIM4 configuration unchanged
    {
        // ... TIM4 PWM configuration
    }
}
```

**Deinitialization Function**:
```c
void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef* htim_pwm)
{
    if(htim_pwm->Instance==TIM1)
    {
        __HAL_RCC_TIM1_CLK_DISABLE();

        HAL_NVIC_DisableIRQ(TIM1_BRK_TIM9_IRQn);
        HAL_NVIC_DisableIRQ(TIM1_TRG_COM_TIM11_IRQn);
        HAL_NVIC_DisableIRQ(TIM1_CC_IRQn);
    }
}
```

**TIM1 Application Use Cases** (enabled by freeing from HAL timebase):
1. **Fan Control**: PWM for variable-speed fan control
2. **LED Dimming**: Smooth brightness control for status LEDs
3. **Motor Control**: Brushless motor drive with complementary outputs
4. **Audio**: Piezo buzzer tone generation

**Interrupt Priority**: 5 (medium priority)
- Higher than idle task
- Lower than critical system interrupts (SPI, I2C)
- Appropriate for non-critical PWM updates

### 8. HAL Timebase Migration TIM1 → TIM2 (`Core/Src/stm32f4xx_hal_timebase_tim.c`)

**Statistics**: 63 lines changed (complete rewrite for TIM2)

**Why Migration Necessary**:

**FreeRTOS Tick Conflict**:
- FreeRTOS uses SysTick for RTOS tick (1ms typically)
- HAL also needs 1ms tick for HAL_Delay(), timeout mechanisms
- **Solution**: Use separate timer (TIM2) for HAL tick, leave SysTick for FreeRTOS

**Why TIM2 (not TIM1)**:
- **TIM1**: Advanced timer with complex features (deadtime, break input, complementary outputs)
  - Valuable for PWM applications (fan control, motor drive)
  - Wasting on simple timebase is inefficient
- **TIM2**: General-purpose 32-bit timer
  - Perfect for timebase (simple up-counter with interrupt)
  - 32-bit range provides extended periods if needed

**Clock Source Difference**:
```c
// BEFORE (TIM1 on APB2):
__HAL_RCC_TIM1_CLK_ENABLE();
uwTimclock = 2*HAL_RCC_GetPCLK2Freq();  // APB2 clock (high-speed peripherals)

// AFTER (TIM2 on APB1):
__HAL_RCC_TIM2_CLK_ENABLE();

// Get APB1 prescaler
uwAPB1Prescaler = clkconfig.APB1CLKDivider;

// Compute TIM2 clock (APB1)
if (uwAPB1Prescaler == RCC_HCLK_DIV1) {
    uwTimclock = HAL_RCC_GetPCLK1Freq();  // APB1 = AHB, timer clock = APB1
} else {
    uwTimclock = 2UL * HAL_RCC_GetPCLK1Freq();  // APB1 prescaled, timer clock = 2 * APB1
}
```

**Clock Tree Context**:
```
STM32F407 Clock Tree (typical @ 168MHz):
AHB (HCLK) = 168 MHz
├─ APB1 (PCLK1) = 42 MHz (÷4)
│  └─ APB1 Timer Clock = 84 MHz (×2 when APB1 prescaler != 1)
│     └─ TIM2, TIM3, TIM4, TIM5 (general-purpose timers)
└─ APB2 (PCLK2) = 84 MHz (÷2)
   └─ APB2 Timer Clock = 168 MHz (×2 when APB2 prescaler != 1)
      └─ TIM1 (advanced timer)
```

**TIM2 Initialization**:
```c
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    // Enable TIM2 clock
    __HAL_RCC_TIM2_CLK_ENABLE();

    // Get clock configuration
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

    // Calculate timer clock (see above)
    // ... clock calculation ...

    // Compute prescaler for 1MHz counter clock
    uwPrescalerValue = (uint32_t) ((uwTimclock / 1000000U) - 1U);

    // Initialize TIM2
    htim2.Instance = TIM2;
    htim2.Init.Period = (1000000U / 1000U) - 1U;  // 1ms period (1000 Hz)
    htim2.Init.Prescaler = uwPrescalerValue;      // Scale to 1MHz counter
    htim2.Init.ClockDivision = 0;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    status = HAL_TIM_Base_Init(&htim2);
    if (status == HAL_OK) {
        status = HAL_TIM_Base_Start_IT(&htim2);  // Start with interrupts
        if (status == HAL_OK) {
            HAL_NVIC_EnableIRQ(TIM2_IRQn);
            HAL_NVIC_SetPriority(TIM2_IRQn, TickPriority, 0U);
            uwTickPrio = TickPriority;
        }
    }
    return status;
}
```

**Calculation Example** (168 MHz system clock):
```
APB1 Prescaler = 4 (HCLK/4)
APB1 Clock (PCLK1) = 168 MHz / 4 = 42 MHz
TIM2 Clock = 2 * PCLK1 = 2 * 42 MHz = 84 MHz (rule: when prescaler != 1, timer clock = 2 * APBx)

Prescaler = (84,000,000 / 1,000,000) - 1 = 83
→ Counter increments at 84MHz / 84 = 1 MHz (1 µs per tick)

Period = (1,000,000 / 1,000) - 1 = 999
→ Interrupt fires every 1000 counts = 1000 µs = 1 ms

Result: 1 ms HAL tick ✓
```

**Suspend/Resume Functions Updated**:
```c
void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_UPDATE);  // Disable TIM2 update interrupt
}

void HAL_ResumeTick(void)
{
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);  // Re-enable TIM2 update interrupt
}
```

**Use Cases**:
- Power management: Suspend HAL tick during low-power modes
- Critical sections: Temporarily disable HAL timeouts
- Testing: Control HAL tick programmatically

### 9. Interrupt Handler Implementation (`Core/Src/stm32f4xx_it.c`)

**Statistics**: 20 lines added

**Handle Extern Declarations Updated**:
```c
extern DMA_HandleTypeDef hdma_spi1_rx;
extern DMA_HandleTypeDef hdma_spi1_tx;
extern DMA_HandleTypeDef hdma_spi2_rx;
extern DMA_HandleTypeDef hdma_spi2_tx;
extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_uart4_rx;
extern DMA_HandleTypeDef hdma_usart6_rx;
extern DMA_HandleTypeDef hdma_usart6_tx;
extern WWDG_HandleTypeDef hwwdg;

// REORDERED: TIM handles
extern TIM_HandleTypeDef htim1;  // Now before htim2
extern TIM_HandleTypeDef htim2;  // HAL timebase (new)
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim5;
```

**New TIM2 Interrupt Handler**:
```c
/**
  * @brief This function handles TIM2 global interrupt.
  */
void TIM2_IRQHandler(void)
{
    /* USER CODE BEGIN TIM2_IRQn 0 */

    /* USER CODE END TIM2_IRQn 0 */
    HAL_TIM_IRQHandler(&htim2);
    /* USER CODE BEGIN TIM2_IRQn 1 */

    /* USER CODE END TIM2_IRQn 1 */
}
```

**Interrupt Flow**:
```
Hardware: TIM2 counter reaches period value (999)
         ↓
NVIC: TIM2_IRQn fires
         ↓
Vector Table: Calls TIM2_IRQHandler()
         ↓
HAL: HAL_TIM_IRQHandler(&htim2) processes interrupt
         ↓
Callback: HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
         ↓
Check: if (htim->Instance == TIM2) → HAL_IncTick()
         ↓
HAL Tick: uwTick++ (global HAL tick counter incremented)
```

**Callback in hf_it_callback.c**:
```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    // CHANGED: TIM1 → TIM2
    if (htim->Instance == TIM2) {
        HAL_IncTick();  // Increment HAL tick counter
    }
}
```

## Integration with Previous Patches

### Builds on Patch 0003

**Continues Debug Cleanup**:
- Removes more printf debug statements (board_init function signature print)
- Aligns with patch 0003's philosophy of production-ready code

**Refines Production Test**:
- Fixes fan duty cycle readback (patch 0002 test framework now accurate)
- Network reconfiguration enables dynamic test environment setup
- RTC format standardization improves test repeatability

### Extends Patch 0002

**Production Test Framework Enhancements**:
- Fan control tests now return actual values (not hardcoded)
- Network configuration tests can verify dynamic changes
- RTC tests use consistent BCD format

### Prepares for Future Patches

**TIM1 Availability**:
- Freed TIM1 used in later patches for fan PWM control
- Foundation for LED dimming features (patch 0076)

**Dynamic Network Config**:
- Enables web interface network configuration (patch 0006 and beyond)
- Supports DHCP/static IP switching in future

**RTC Consistency**:
- All RTC access now BCD, simplifying future RTC features
- Prepares for RTC integration in web interface (patch 0017)

## Technical Deep Dive: TIM1 vs TIM2 for HAL Timebase

### Advanced Timer (TIM1) Features

**Why TIM1 is Overkill for Timebase**:

**Complex Features Unused in Timebase Role**:
1. **Complementary Outputs**: TIM1_CH1/TIM1_CH1N paired outputs with deadtime
   - Used for: Half-bridge motor drivers, preventing shoot-through
   - Wasted on: Simple 1ms interrupt generation

2. **Break Input**: Emergency shutoff via BKIN pin
   - Used for: Motor overcurrent protection
   - Wasted on: Timebase (no safety function needed)

3. **Deadtime Generation**: Programmable delay between complementary outputs
   - Used for: Power electronics (prevent simultaneous high-side/low-side conduction)
   - Wasted on: Timebase

4. **Master/Slave Synchronization**: Trigger other timers in precise sequences
   - Used for: Multi-phase motor control, ADC triggering
   - Wasted on: Timebase

**Resource Waste**:
- TIM1 is only advanced timer on STM32F407VET6
- Using for timebase prevents advanced PWM applications
- General-purpose timer (TIM2-5) equally capable for timebase

### General-Purpose Timer (TIM2) Advantages

**Perfect for Timebase**:
1. **32-bit Counter**: TIM2 has 32-bit range (vs 16-bit for most timers)
   - Enables very long periods without prescaler complexity
   - Future-proof for extended timeout measurements

2. **APB1 Bus**: Lower-speed bus appropriate for timebase
   - Reduces high-speed bus contention
   - TIM1 on APB2 better reserved for fast PWM

3. **Resource Availability**: STM32F407VET6 has TIM2, TIM3, TIM4, TIM5 (4 general-purpose timers)
   - Using one for HAL timebase leaves others for application
   - TIM1 (only advanced timer) freed for applications requiring advanced features

**Clock Tree Efficiency**:
```
TIM1 (APB2): 168 MHz max → oversized for 1ms tick
TIM2 (APB1): 84 MHz → perfect for 1ms tick (less prescaling needed)
```

## Security Implications

### Dynamic Network Configuration

**Security Considerations**:

**Unauthenticated Network Change**:
```c
// Current implementation (patch 0004):
int32_t es_set_eth(...) {
    // NO authentication check!
    // ANY protocol command can change network settings
    dynamic_change_eth();
    return HAL_OK;
}
```

**Attack Scenario**:
1. Attacker sends SPI protocol command to change IP/gateway
2. BMC reconfigures network to attacker-controlled network
3. BMC now accessible to attacker, isolated from legitimate network
4. Attacker gains management access

**Mitigation Needed** (future patches):
```c
// Secure version:
int32_t es_set_eth(...) {
    // Verify caller is authenticated (session check)
    if (!verify_authenticated_session()) {
        return HAL_ERROR;
    }

    // Validate IP addresses (no private → public changes, etc.)
    if (!validate_network_config(ip, netmask, gw)) {
        return HAL_ERROR;
    }

    // Log network change for audit
    log_security_event("Network config changed", ip);

    dynamic_change_eth();
    return HAL_OK;
}
```

**Current Vulnerability**:
- Production test commands can change network settings without auth
- If test commands left enabled in production firmware (if `ENABLE_PROD_TEST` not properly disabled):
  - Remote attacker with SPI access can reconfigure network
  - Denial of service by setting invalid network config

### RTC Format Change

**Information Disclosure Risk** (minor):
- BCD format sometimes used to fingerprint device type
- Attacker observing RTC values can identify STM32 platform
- **Impact**: Low (device type may be guessable from other sources)

**Data Integrity** (improvement):
- BCD validation catches corrupted RTC values earlier
- Prevents invalid time values propagating to logs
- **Benefit**: More reliable audit timestamps

## Testing Recommendations

### Dynamic Network Configuration

**Test Sequence**:
```bash
# 1. Initial configuration
curl http://192.168.1.100/api/network
# Expected: {"ipaddr":"192.168.1.100", "netmask":"255.255.255.0", "gateway":"192.168.1.1"}

# 2. Change network (via SPI command or web interface)
# Set IP to 192.168.100.50

# 3. Immediate verification (from BMC console)
ifconfig
# Expected: IP changed to 192.168.100.50

# 4. HTTP still accessible (from new network)
curl http://192.168.100.50/api/network
# Expected: {"ipaddr":"192.168.100.50", ...}

# 5. Verify active sessions survived
# Login session from step 1 should still be valid
curl -b cookies.txt http://192.168.100.50/info.html
# Expected: Authenticated page served (no redirect to login)
```

**Edge Cases**:
- Change to invalid IP (0.0.0.0) → should reject
- Change to unreachable gateway → should accept but warn
- Change while web requests in flight → should not crash
- Change with DHCP enabled → behavior undefined (current limitation)

### RTC Format Validation

**Test BCD Format Correctness**:
```c
// Set test time
RTC_TimeTypeDef SetTime = {0};
SetTime.Hours = 0x23;    // 23 hours (valid BCD)
SetTime.Minutes = 0x59;  // 59 minutes (valid BCD)
SetTime.Seconds = 0x45;  // 45 seconds (valid BCD)
HAL_RTC_SetTime(&hrtc, &SetTime, RTC_FORMAT_BCD);

// Read back
struct rtc_time_t stime;
es_get_rtc_time(&stime);

// Verify
assert(stime.Hours == 0x23);
assert(stime.Minutes == 0x59);
assert(stime.Seconds == 0x45);

// Display (convert BCD to decimal for printf)
printf("%02d:%02d:%02d\n",
       (stime.Hours & 0x0F) + ((stime.Hours >> 4) * 10),
       (stime.Minutes & 0x0F) + ((stime.Minutes >> 4) * 10),
       (stime.Seconds & 0x0F) + ((stime.Seconds >> 4) * 10));
// Expected: 23:59:45
```

**Invalid BCD Detection**:
```c
// Inject invalid BCD (e.g., 0x2A - invalid because A > 9)
// Should expect HAL_RTC_SetTime to return HAL_ERROR
RTC_TimeTypeDef BadTime = {0};
BadTime.Hours = 0x2A;  // Invalid BCD!
HAL_StatusTypeDef status = HAL_RTC_SetTime(&hrtc, &BadTime, RTC_FORMAT_BCD);
assert(status == HAL_ERROR);  // Should reject
```

### Fan Duty Cycle Readback

**Test Actual Storage**:
```c
// Set fan 0 to 75% duty
struct fan_control_t fan0 = {.fan_num = 0, .duty = 75};
int ret = es_set_fan_duty(&fan0);
assert(ret == HAL_OK);

// Read back
struct fan_control_t fan0_read = {.fan_num = 0};
ret = es_get_fan_duty(&fan0_read);
assert(ret == HAL_OK);
assert(fan0_read.duty == 75);  // Should match, NOT hardcoded 50!

// Change to 30%
fan0.duty = 30;
es_set_fan_duty(&fan0);
es_get_fan_duty(&fan0_read);
assert(fan0_read.duty == 30);  // Verify change reflected
```

### TIM2 HAL Timebase

**Validate 1ms Tick**:
```c
// Measure HAL tick accuracy
uint32_t tick_start = HAL_GetTick();
osDelay(1000);  // FreeRTOS delay 1000ms
uint32_t tick_end = HAL_GetTick();
uint32_t elapsed = tick_end - tick_start;

// Should be approximately 1000 (±1-2 ms tolerance)
assert(elapsed >= 999 && elapsed <= 1002);
printf("HAL tick accuracy: %lu ms\n", elapsed);
```

**Verify TIM1 Freed**:
```c
// Configure TIM1 for PWM (should now work)
TIM_HandleTypeDef htim1_test;
htim1_test.Instance = TIM1;
htim1_test.Init.Prescaler = 83;  // 84MHz / 84 = 1MHz
htim1_test.Init.CounterMode = TIM_COUNTERMODE_UP;
htim1_test.Init.Period = 999;  // 1kHz PWM
htim1_test.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
HAL_StatusTypeDef status = HAL_TIM_PWM_Init(&htim1_test);
assert(status == HAL_OK);  // Should succeed (TIM1 free)
```

## Known Limitations and Future Work

### Current Limitations

**1. Dynamic Network - No MAC Change**:
- MAC address change requires interface restart (not implemented)
- Workaround: BMC reboot required to change MAC

**2. Dynamic Network - DHCP Transition**:
- Switching from static IP to DHCP requires DHCP client restart
- Current code assumes static IP → static IP change
- Future: Add `dhcp_start()` call when needed

**3. Fan Duty Cycle - No Feedback**:
- Storage is write-only (set and store)
- No actual PWM duty cycle readback from hardware
- Future: Read TIM4 CCR register to verify PWM output

**4. RTC Format - Display Conversion Needed**:
- BCD values need conversion for human-readable display
- Every printf requires BCD→decimal conversion
- Future: Utility function for formatted RTC string

**5. TIM2 Precision**:
- Depends on APB1 clock accuracy (crystal oscillator tolerance)
- Typical: ±50 ppm (±4.3 seconds/day drift)
- Future: Use external RTC with better accuracy if critical

### Future Enhancements

**1. Network Configuration Improvements** (patches 0005-0006):
- Web interface for network configuration
- DHCP/static IP toggle
- Network configuration persistence to EEPROM
- Validation and error handling

**2. Fan Control Enhancements** (later patches):
- Closed-loop fan speed control (tachometer feedback)
- Temperature-based automatic fan speed adjustment
- Fan failure detection (stopped fan)

**3. RTC Features** (patch 0017 and beyond):
- Web interface RTC set/display
- Alarm functionality
- Timestamp logging
- NTP synchronization (if network available)

**4. TIM1 PWM Applications**:
- Multi-channel LED dimming
- Complementary motor control
- Precise servo control

**5. System Monitoring**:
- HAL tick jitter measurement
- Timer interrupt latency profiling
- Performance benchmarking

## Conclusion

Patch 0004 addresses critical bugs and system architecture issues discovered during production testing:

**Dynamic Network Configuration**: Eliminates HTTP task restart requirement, enabling zero-downtime network reconfiguration. This improves production test efficiency and prepares for web-based network configuration features.

**RTC Format Standardization**: Migration to BCD format improves accuracy and aligns with hardware native format. Eliminates conversion errors and simplifies RTC access across firmware.

**HAL Timebase Migration TIM1 → TIM2**: Frees advanced timer (TIM1) for application use while maintaining HAL 1ms tick on general-purpose timer (TIM2). Resolves resource contention and enables future PWM features.

**Fan Duty Cycle Tracking**: Fixes production test verification by storing and returning actual duty cycle values instead of hardcoded placeholders.

**Impact**: Though modest in line count (+128 lines), this patch significantly improves system reliability, test accuracy, and resource utilization. It demonstrates the importance of configuration management and hardware resource allocation in embedded systems.

**Quality**: Fixes are surgical and targeted, improving specific subsystems without destabilizing others. This careful approach maintains code quality while addressing real-world issues discovered in testing.

**Foundation**: Enables multiple future enhancements including web-based network configuration, fan control improvements, and advanced PWM applications.
