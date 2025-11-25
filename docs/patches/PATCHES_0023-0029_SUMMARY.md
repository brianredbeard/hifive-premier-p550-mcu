# Patches 0023-0029: Web Server Refinements and Power Management Enhancements

## Overview

This document summarizes patches 0023 through 0029, which collectively refine the BMC firmware's web interface, fix critical bugs, and enhance power management capabilities. These patches represent the maturation phase of the web server and power subsystem before the major INA226 power monitoring feature (patch 0030).

---

## Patch 0023: Auto-Refresh AJAX and Bug Fixes

### Metadata
- **File**: `0023-WIN2030-15279-fix-bmc-auto-refresh-ajax-fixbugs.patch`
- **Author**: yuan junhui
- **Date**: Mon, 13 May 2024 16:08:59 +0800
- **Stats**: 257 insertions, 94 deletions

### Key Changes

1. **Auto-Refresh Mechanism** (30-second and 3-minute intervals):
   - Power consumption, RTC, SoC status: Every 30 seconds
   - PVT info, DIP switch, network: Every 3 minutes
   - Prevents conflict with auto-logout timeout

2. **Session Cookie Refresh on Manual Requests**:
   - Added `json_header_withcookie` template
   - Extends session lifetime when user manually clicks refresh
   - Prevents premature timeout during active use

3. **Power Retention → Power Lost Resume Rename**:
   - Changed `/power_retention_status` to `/power_lostresume_status`
   - Clarifies feature purpose (resume power state after power loss)
   - Updated all frontend references

4. **RTC Input Validation**:
   - Added range checks: year (0-3000), month (0-12), date (0-31), weekday (0-6), hours (0-23), minutes/seconds (0-59)
   - Prevents invalid date/time submission
   - Assertions ensure valid data before setting RTC

5. **Hidden Refresh Buttons Pattern**:
   - Real buttons: Visible, clickable by user
   - Hidden buttons: `value="0"` → auto-refresh, `value="1"` → manual refresh
   - Enables different cookie behavior based on refresh source

### Impact
- Improved UX with real-time data updates
- Better session management
- Input validation prevents RTC corruption
- Foundation for responsive dashboard

---

## Patch 0024: Printf Redirection Fix

### Metadata
- **File**: `0024-WIN2030-15099-fix-hf_common.c-_write-was-disabled.patch`
- **Author**: linmin
- **Date**: Mon, 13 May 2024 18:00:33 +0800
- **Stats**: 1 insertion, 2 deletions (tiny but critical)

### Single Change

**Re-enabled `_write()` function**:
```c
// Removed: #if 0
int _write(int fd, char *ch, int len) {
    // UART printf redirection
}
// Removed: #endif
```

### Impact
- **Critical**: Printf debugging now works on serial console
- All debug messages route to UART3
- Essential for remote diagnostics
- Was accidentally disabled in patch 0022

---

## Patch 0025: Power Units and DIP Switch Software Control

### Metadata
- **File**: `0025-WIN2030-15279-fix-bmc-mW-mV-mA.dipswitch-swctrl.patch`
- **Author**: yuan junhui
- **Date**: Mon, 13 May 2024 18:49:51 +0800
- **Stats**: 19 insertions, 6 deletions

### Changes

1. **Power Measurement Units** in HTML:
   ```html
   <label>Power Consumption:</label> <input .../><span>mW</span>
   <label>Voltage:</label> <input .../><span>mV</span>
   <label>Current:</label> <input .../><span>mA</span>
   ```
   - Clarifies measurement units for users
   - Aligns with INA226 sensor output (patch 0030)

2. **DIP Switch Software Control Field**:
   ```c
   typedef struct {
       int dip01, dip02, dip03, dip04;
       int swctrl;  // 0=hardware, 1=software
   } DIPSwitchInfo;
   ```
   - Enables web override of physical DIP switch
   - Essential for remote boot mode control
   - `swctrl=0`: Physical switch position used
   - `swctrl=1`: Software setting overrides hardware

### Impact
- Improved user clarity on power readings
- Enables remote boot mode configuration
- Prepares for patch 0026's bootsel implementation

---

## Patch 0026: Power Process Refactoring

### Metadata
- **File**: `0026-WIN2030-15279-refactor-power-refactor-web-callback.patch`
- **Author**: xuxiang
- **Date**: Mon, 13 May 2024 19:42:10 +0800
- **Stats**: 392 insertions, 239 deletions (major refactoring)
- **Breaking Changes**: YES

### Major Changes

1. **Power Test Mode** (`#define POWER_TESET_MODE`):
   - Disables auto-boot by default
   - Enables full functional testing via web interface
   - Allows power on/off control without automatic SoM power-up

2. **RTC Functions Moved** (`hf_protocol_process.c` → `hf_common.c`):
   - Centralized RTC access in common utilities
   - Available to both SPI protocol and web server
   - Consistent date/time handling

3. **Bootsel Control Function**:
   ```c
   void set_bootsel(uint8_t is_soft_ctrl, uint8_t sel);
   ```
   - Hardware mode: GPIO pins configured as inputs (read physical switch)
   - Software mode: GPIO pins as outputs (set programmatically)
   - 4-bit boot select: `sel` bits control BOOT_SEL0-3 pins

4. **Auto-Boot Function**:
   ```c
   uint32_t es_autoboot(void) {
       int32_t som_pwr_last_state = 0;
       if (is_som_pwr_lost_resume() && !es_get_som_pwr_last_state(&som_pwr_last_state)) {
           if (som_pwr_last_state)
               return 1;  // Resume to ON state
       }
       return 0;  // Stay OFF
   }
   ```
   - Reads last power state from EEPROM
   - If power lost resume enabled + last state was ON → auto-boot
   - Enables "resume on AC restore" feature

5. **Web Callback Implementations**:
   - `get_rtcinfo()`: Now reads actual RTC hardware
   - `set_rtcinfo()`: Writes to RTC registers
   - `get_dip_switch()`: Returns pointer for efficiency
   - Removed placeholder data

### Breaking Changes

**Default Behavior Change**:
- Before: SoM auto-powers on BMC boot
- After: SoM stays off until web/button command
- **Rationale**: Facilitates testing, prevents unexpected power-on

**Migration Path**: Define `AUTO_BOOT` to restore old behavior

### Impact
- More predictable power behavior
- Better testability
- Real RTC integration
- Foundation for sophisticated boot control

---

## Patch 0027: Power Process Bug Fixes

### Metadata
- **File**: `0027-WIN2030-15279-fix-power-process-bug.patch`
- **Author**: xuxiang
- **Date**: Mon, 13 May 2024 20:40:38 +0800
- **Stats**: 19 insertions, 14 deletions

### Fixes

1. **PMIC Initialization Moved to `power_test_dome()`**:
   ```c
   void power_test_dome(void) {
       atx_power_on(pdTRUE);
       // Wait for DC power good
       i2c_init(I2C3);
       pmic_power_on(pdTRUE);
       // Poll until pmic_b6out_105v() succeeds
       i2c_init(I2C1);
   }
   ```
   - Ensures PMIC powered before I2C communication attempts
   - Solves I2C bus errors from tri-state SOM pins during power-off

2. **I2C Error Return Paths Fixed**:
   ```c
   if (status != HAL_OK) {
       printf("I2Cx_read_Error(%x) reg %x; status %x\r\n", slave_addr, reg_addr, status);
       return status;  // Now actually returns error!
   }
   ```
   - Previous code printed error but continued execution
   - Now properly propagates errors to caller

3. **Reset Before Power-Off**:
   ```c
   void set_power_off(void) {
       i2c_deinit(I2C3);
       som_reset_control(pdTRUE);  // Assert reset before cutting power
       // Disable power rails...
   }
   ```
   - Prevents SOM from driving I2C bus during power-down
   - Eliminates bus contention and corruption

4. **Auto-Boot Debug Message**:
   ```c
   if (som_pwr_last_state) {
       printf("pwr enable and last state is power on\n");
       return 1;
   }
   ```
   - Aids debugging of power resume behavior

### Impact
- **Critical**: Fixes I2C communication failures during power transitions
- More reliable power sequencing
- Better error reporting
- Essential for production stability

---

## Patch 0028: SOM Power Last State Bug Fix

### Metadata
- **File**: `0028-WIN2030-15279-fix-set-som-pwr-last-state-bug.patch`
- **Author**: xuxiang
- **Date**: Tue, 14 May 2024 09:58:13 +0800
- **Stats**: 2 insertions, 4 deletions

### Fix

**Moved State Persistence to State Change Function**:

Before (incorrect - state saved in power-off only):
```c
void set_power_off(void) {
    // Disable power...
    es_set_som_pwr_last_state(SOM_PWR_LAST_STATE_OFF);
}
```

After (correct - state saved on any change):
```c
void change_som_power_state(power_switch_t newState) {
    taskENTER_CRITICAL();
    som_power_state = newState;
    taskEXIT_CRITICAL();
    es_set_som_pwr_last_state(newState);  // Save to EEPROM
}
```

**Also Added DIP Switch Software Control Persist**:
```c
int set_dip_switch(DIPSwitchInfo dipSwitchInfo) {
    // Pack bits...
    es_set_som_dip_switch_soft_ctl_attr(dipSwitchInfo.swctrl);  // Save control mode
    return es_set_som_dip_switch_soft_state(som_dip_switch_state);
}
```

### Impact
- **Critical**: Power state now correctly persists across all transitions
- Fixes power lost resume feature
- DIP switch software mode persists across reboots
- More consistent state management

---

## Patch 0029: SoC Status and Reboot Timer

### Metadata
- **File**: `0029-WIN2030-15099-refactor-bmc-bmc-web-cmd-support-soc-s.patch`
- **Author**: huangyifeng
- **Date**: Tue, 14 May 2024 13:49:06 +0800
- **Stats**: 60 insertions, 18 deletions

### Key Features

1. **Real SoC Status Implementation**:
   ```c
   int get_soc_status() {
       return SOM_DAEMON_ON == get_som_daemon_state() ? 0 : 1;
   }
   ```
   - Replaces placeholder from patch 0021
   - Reads actual daemon heartbeat state
   - 0 = working, 1 = stopped/crashed

2. **Reboot Timeout Timer**:
   ```c
   TimerHandle_t xSomRebootTimer;

   void vSomRebootTimerCallback(TimerHandle_t xSomRebootTimer) {
       som_reset_control(pdTRUE);
       osDelay(10);
       som_reset_control(pdFALSE);
       printf("reboot SOM timeout, force reset SOM!\n");
   }

   void TriggerSomRebootTimer(void) {
       xTimerStart(xSomRebootTimer, 0);  // 5-second timeout
   }
   ```
   - If SoC doesn't acknowledge reboot request within 5 seconds
   - BMC forces hardware reset
   - Prevents hung reboot attempts

3. **Reset Feedback Processing**:
   ```c
   static void mcu_reset_som_process(void) {
       printf("%s %d mcu reset som\n", __func__, __LINE__);
       StopSomRebootTimer();  // Cancel timeout if reset succeeded
   }
   ```
   - GPIO feedback from SoC when reset completes
   - Stops timer to prevent redundant reset

4. **Power-Off Timer Renamed**:
   ```c
   TimerHandle_t xSomPowerOffTimer;  // Was xSomTimer
   void TriggerSomPowerOffTimer(void);  // Was TriggerSomTimer
   ```
   - Clarifies timer purpose
   - Separates power-off timeout from reboot timeout

5. **SPI Command Validation**:
   ```c
   int web_cmd_handle(CommandType cmd, void *data, int data_len, uint32_t timeout) {
       if (SOM_POWER_ON != get_som_power_state()) {
           return HAL_ERROR;  // Reject commands when SoM is off
       }
       // Process command...
   }
   ```
   - Prevents sending SPI commands to powered-off SoM
   - Returns error immediately instead of timeout

### Impact
- Real SoC status monitoring
- Robust reboot with fallback mechanism
- Better error handling for SPI protocol
- Foundation for watchdog functionality

---

## Cumulative Impact of Patches 0023-0029

### Reliability Improvements
1. I2C communication stability during power transitions
2. Proper error propagation throughout stack
3. Timeout mechanisms prevent infinite waits
4. State persistence ensures correct behavior after power loss

### Feature Completeness
1. Real-time web dashboard with auto-refresh
2. Complete RTC integration
3. Software-controllable boot mode
4. SoC health monitoring

### Code Quality
1. Centralized common functions (RTC, bootsel)
2. Consistent naming conventions
3. Removed placeholders and TODOs
4. Added input validation

### User Experience
1. Clear units on power measurements
2. Auto-refresh keeps data current
3. Session doesn't timeout during active use
4. Remote boot mode control

### Security Notes
- Still no authentication on `/soc-status` endpoint
- Printf debug messages expose internal state
- No rate limiting on API endpoints
- Auto-refresh could amplify DoS attacks

---

## Testing Recommendations for Patches 0023-0029

### Power Sequencing Tests
```bash
# Test power-off with timeout
curl -X POST http://<bmc-ip>/power_status -d "power_status=0"
# Verify timeout triggers if SoC doesn't respond

# Test reboot with timeout
curl -X POST http://<bmc-ip>/restart
# Verify forced reset if SoC doesn't acknowledge
```

### Auto-Boot Tests
```bash
# Enable power lost resume
curl -X POST http://<bmc-ip>/power_lostresume_status -d "power_lostresume_status=1"

# Power on SoM
curl -X POST http://<bmc-ip>/power_status -d "power_status=1"

# Physically cycle BMC power (AC power off/on)
# Verify SoM auto-powers on

# Disable power lost resume
curl -X POST http://<bmc-ip>/power_lostresume_status -d "power_lostresume_status=0"

# Cycle BMC power again
# Verify SoM stays off
```

### Software Bootsel Tests
```bash
# Set software control mode
curl -X POST http://<bmc-ip>/dip_switch \
  -d "dip01=0&dip02=1&dip03=0&dip04=0&swctrl=1"

# Reboot SoM
curl -X POST http://<bmc-ip>/restart

# Verify SoM boots from expected source (SD card if dip02=1)
```

### RTC Validation Tests
```bash
# Try invalid date
curl -X POST http://<bmc-ip>/rtc \
  -d "year=9999&month=13&date=32&hours=25&minutes=70&seconds=99"
# Should reject with error

# Set valid date
curl -X POST http://<bmc-ip>/rtc \
  -d "year=2024&month=5&date=14&hours=10&minutes=30&seconds=0"
# Should succeed

# Read back
curl http://<bmc-ip>/rtc
# Verify matches
```

### Auto-Refresh Tests
```javascript
// Open browser console
// Navigate to BMC web interface
// Observe console logs every 30 seconds:
// "refreshing power consumption..."
// "refreshing soc status..."

// Observe network tab shows requests every 30s and 3min
```

### I2C Error Handling Tests
```bash
# Disconnect I2C sensor (e.g., remove EEPROM chip)
# Attempt operations requiring I2C
curl http://<bmc-ip>/board_info_som
# Verify error message in console, graceful failure in web UI

# Reconnect sensor
# Verify operations resume
```

---

## Preparation for Patch 0030 (INA226)

These patches establish the foundation for the INA226 power monitoring feature:

1. **Power measurement display units** (mW, mV, mA) - patch 0025
2. **Auto-refresh mechanism** for real-time power data - patch 0023
3. **I2C stability fixes** for reliable sensor communication - patch 0027
4. **Test mode** enabling power control for sensor testing - patch 0026

Patch 0030 will integrate the INA226 and PAC1934 sensors to provide actual power measurements, replacing the placeholder values in `get_power_info()`.
