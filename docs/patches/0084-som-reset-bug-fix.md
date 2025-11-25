# Patch 0084: SoM Reset Bug Fix

## Metadata
- **Patch Number**: 0084
- **Ticket**: WIN2030-15279
- **Type**: fix
- **Component**: hf_gpio_process - SoM reset control
- **Author**: linmin <linmin@eswincomputing.com>
- **Date**: Tue, 11 Jun 2024 15:16:20 +0800
- **Severity**: Important - Prevents invalid reset operations
- **Files Modified**: 1
  - `Core/Src/hf_gpio_process.c`

## Summary

This patch fixes a bug where the BMC would attempt to reset the SoM (System-on-Module) even when the SoM was powered off. Asserting the reset line to an unpowered SoM can cause undefined hardware behavior, potential power sequencing violations, and GPIO damage. The fix adds a power state check before performing the reset operation.

## Problem Description

### Symptom

**Invalid Reset Operation**:
- SoM reset signal (`MCU_RESET_SOM_N`) asserted when SoM is powered off
- Reset line toggled on unpowered hardware

**Trigger Scenario**:
- SoM daemon sends reset feedback signal (FLAGS_SOM_RST_OUT)
- BMC receives signal in `som_rst_feedback_process()`
- Function **unconditionally** asserts reset line
- **No check** if SoM is actually powered on

**Example Flow** (Buggy):
```
1. SoM powered off (SOM_POWER_OFF state)
2. Stale/spurious reset feedback signal arrives
3. som_rst_feedback_process() executes
4. som_reset_control(pdTRUE) → Asserts RESET line
5. osDelay(2) → Wait 2ms
6. som_reset_control(pdFALSE) → Deasserts RESET line
7. RESET toggled on UNPOWERED SoM!
```

### Why This Is a Problem

**1. Electrical Issues**

**Unpowered GPIO Input**:
- When SoM is powered off, its GPIOs have no power supply
- Asserting reset line drives current into unpowered GPIO
- Input protection diodes conduct: `Reset Pin → VDD_SOM (floating)`
- **Parasitic power**: Current flows through protection diodes, partially powers SoM
- SoM VDD rises to ~0.3-0.7V (below operating voltage but not zero)
- Undefined logic levels, potential latch-up

**GPIO Damage Risk**:
- Input protection diodes not designed for continuous current
- Exceeding diode current rating: Long-term degradation
- Worst case: Diode failure → GPIO pin damage

**2. Power Sequencing Violations**

**RESET Before POWER**:
- Proper power-up: VDD stable → wait → deassert RESET
- This bug: RESET toggled → then later VDD applied
- Violates EIC7700X power sequencing requirements
- May cause:
  - Internal state corruption in SoC
  - Boot failures when SoM later powered on
  - Need for additional reset after power-up

**3. Undefined Behavior**

**Unpredictable SoC State**:
- SoC receiving reset signal without power
- Internal circuitry in unknown state
- Next power-on: SoC might not boot cleanly
- Intermittent boot failures difficult to diagnose

**4. Signal Integrity**

**Reset Line Contention**:
- If SoM has reset line pull-up to its own VDD (powered off)
- BMC driving reset low
- Current flows: BMC GPIO → pull-up → unpowered VDD
- GPIO may not reach valid logic low level
- Wastes power, stresses BMC GPIO driver

### Root Cause

**Function**: `som_rst_feedback_process()` in `Core/Src/hf_gpio_process.c`

**Original Code** (Buggy):
```c
static void som_rst_feedback_process(void)
{
    printf("%s %d som reset\n", __func__, __LINE__);
    som_reset_control(pdTRUE);   // Assert reset - NO POWER CHECK!
    osDelay(2);                  // Wait 2ms
    som_reset_control(pdFALSE);  // Deassert reset
}
```

**Problem**: No guard against resetting unpowered SoM

**Why This Function Exists**:
- SoM daemon process can request BMC to reset the SoM
- Communication via FLAGS_SOM_RST_OUT flag
- Intended for graceful reset during normal operation
- **Assumption**: SoM daemon only sends request when SoM is powered on
- **Reality**: Race conditions, stale signals, or bugs can trigger when powered off

## Technical Details

### Code Changes

**File**: `Core/Src/hf_gpio_process.c`

**Function**: `som_rst_feedback_process()`

**Before** (Lines 21-25):
```c
static void som_rst_feedback_process(void)
{
    printf("%s %d som reset\n", __func__, __LINE__);
    som_reset_control(pdTRUE);
    osDelay(2);
    som_reset_control(pdFALSE);
}
```

**After** (Lines 21-29):
```c
static void som_rst_feedback_process(void)
{
    printf("%s %d som reset\n", __func__, __LINE__);
    if (get_som_power_state() == SOM_DAEMON_ON) {  // NEW: Power state check
        som_reset_control(pdTRUE);
        osDelay(2);
        som_reset_control(pdFALSE);
    }
}
```

**Key Change**: Added `if (get_som_power_state() == SOM_DAEMON_ON)` guard

### Power State Check

**Function**: `get_som_power_state()`

**Returns**: Current SoM power state enum value

**Possible States** (from `Core/Inc/hf_common.h`):
```c
typedef enum {
    SOM_POWER_OFF = 0,       // SoM fully powered down
    SOM_POWER_ON,            // SoM power sequence in progress
    SOM_DAEMON_OFF,          // SoM powered but daemon not running
    SOM_DAEMON_ON,           // SoM powered and daemon running
} power_switch_t;
```

**Why Check for SOM_DAEMON_ON?**

**Not just SOM_POWER_ON**:
- `SOM_POWER_ON`: Power sequence initiated, but SoM may not be fully operational
- `SOM_DAEMON_ON`: SoM fully booted, daemon process running, communication established

**Rationale**:
- Reset feedback signal comes FROM SoM daemon
- If daemon is running, SoM is definitely powered and operational
- Safe to perform reset: SoM can handle reset signal properly
- If daemon not running, reset signal likely spurious/stale

**Alternative Check** (more permissive):
```c
if (get_som_power_state() != SOM_POWER_OFF) {
    // Reset if ANY power state except fully off
}
```

**Rejected**: Too permissive - might reset during power sequencing

**Chosen Check** (conservative):
```c
if (get_som_power_state() == SOM_DAEMON_ON) {
    // Reset ONLY if fully operational
}
```

**Advantage**: Most conservative, safest approach

### Reset Control Function

**Function**: `som_reset_control(uint8_t reset)` (in `hf_power_process.c`)

**Parameters**:
- `reset == pdTRUE` (1): Assert reset (active low → drive low)
- `reset == pdFALSE` (0): Deassert reset (active low → release/high)

**Implementation** (simplified):
```c
void som_reset_control(uint8_t reset)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (reset == pdTRUE) {
        // Assert reset: Configure as output, drive low
        GPIO_InitStruct.Pin = MCU_RESET_SOM_N_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(MCU_RESET_SOM_N_GPIO_Port, &GPIO_InitStruct);

        HAL_GPIO_WritePin(MCU_RESET_SOM_N_GPIO_Port,
                          MCU_RESET_SOM_N_Pin, GPIO_PIN_RESET);

        // Additional cleanup: UART, SPI, I2C
        uart_deinit(UART4);
        uart_deinit(USART6);
        HAL_I2C_DeInit(&hi2c3);
        SPI2_MASTER_CS_LOW();
        set_bootsel(0, 0);
    } else {
        // Deassert reset: Release (configure back to normal)
        GPIO_InitStruct.Pin = MCU_RESET_SOM_N_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(MCU_RESET_SOM_N_GPIO_Port, &GPIO_InitStruct);
    }
}
```

**Key Points**:
- Reset is **active low**: Low = Reset asserted, High = Normal operation
- When asserting: Configure as output push-pull, drive low
- When deasserting: Configure as input with pull-up (allows SoM control)
- Also deinitializes communication peripherals during reset

**This function is hardware-active** - toggling GPIO, deinit peripherals
**Must not call when SoM unpowered** - this patch enforces that!

### Signal Flow

**Normal Flow** (SoM Powered On):
```
SoM Daemon Process
    ↓
Sends Reset Request (FLAGS_SOM_RST_OUT)
    ↓
BMC hf_gpio_task() receives flag
    ↓
Calls som_rst_feedback_process()
    ↓
Checks: get_som_power_state() == SOM_DAEMON_ON
    ↓
TRUE → Proceed with reset
    ↓
som_reset_control(pdTRUE) → Assert reset line
    ↓
osDelay(2ms) → Hold reset
    ↓
som_reset_control(pdFALSE) → Release reset line
    ↓
SoM reboots cleanly
```

**Protected Flow** (SoM Powered Off):
```
Spurious/Stale Reset Request (FLAGS_SOM_RST_OUT)
    ↓
BMC hf_gpio_task() receives flag
    ↓
Calls som_rst_feedback_process()
    ↓
Checks: get_som_power_state() == SOM_DAEMON_ON
    ↓
FALSE → SKIP reset operation
    ↓
Function returns without calling som_reset_control()
    ↓
SoM remains safely powered off, no reset line toggle
```

### Reset Timing

**Reset Pulse Duration**: 2ms (osDelay(2))

**Why 2ms?**

**EIC7700X Reset Requirements** (from SoC manual):
- Minimum reset pulse width: Typically 1ms for clean reset
- 2ms provides safety margin
- Sufficient to reset all SoC internal state machines

**Too Short**:
- <1ms: SoC might not recognize reset
- Internal state not fully cleared

**Too Long**:
- >100ms: Unnecessary, slows down reset/restart operations
- >1s: User-perceptible delay

**2ms is optimal**: Fast enough for responsive operation, long enough for reliable reset

## Impact Analysis

### Before Fix

**Risks**:
1. **GPIO Damage**: Repeated invalid resets could degrade BMC GPIO over time
2. **SoM Boot Failures**: Power sequencing violations lead to intermittent boot issues
3. **Unpredictable Behavior**: SoM state undefined after invalid reset
4. **Debug Complexity**: Intermittent failures hard to diagnose (works sometimes, fails others)

**Scenarios Where Bug Manifests**:

**Scenario 1: Spurious Signal During Power-Off**
```
1. SoM powered off
2. Electromagnetic interference (EMI) on communication bus
3. BMC interprets noise as reset request
4. Attempts reset on unpowered SoM
```

**Scenario 2: Race Condition During Power-Down**
```
1. SoM daemon sends reset request
2. Simultaneously: User presses power-off button
3. SoM powers down while reset request in flight
4. BMC processes reset request after SoM powered off
5. Invalid reset attempted
```

**Scenario 3: SoM Crash Before Power-Off**
```
1. SoM crashes (daemon hangs)
2. Watchdog or user initiates SoM power-off
3. During power-down, stale reset request flag still set
4. After power-off, BMC processes stale flag
5. Invalid reset attempted
```

### After Fix

**Protection**:
1. **GPIO Safety**: BMC never drives reset line to unpowered SoM
2. **Clean Power Sequencing**: Reset only when SoM fully operational
3. **Predictable Behavior**: Reset only occurs in valid states
4. **Debugging Easier**: Reset failures always indicate power state issues

**Trade-offs**:
- **Conservative**: Might skip legitimate reset if power state reporting is buggy
- **Dependency**: Relies on accurate `get_som_power_state()` implementation
- **No Retry**: If power state temporarily wrong, reset request lost (not retried)

**Overall**: Trade-offs acceptable - protecting hardware is higher priority than handling every edge case

## Related Code

### SoM Power State Management

**State Transitions** (simplified):
```
SOM_POWER_OFF
    ↓ (power-on request)
SOM_POWER_ON
    ↓ (power sequence complete)
SOM_DAEMON_OFF
    ↓ (daemon keepalive received)
SOM_DAEMON_ON
    ↓ (daemon timeout or power-off request)
SOM_DAEMON_OFF or SOM_POWER_OFF
```

**Function**: `get_som_power_state()`
- Returns: Current state from state machine
- Thread-safe: Reads volatile global variable
- Updated by: Power management task, daemon keepalive task

**Function**: `change_som_power_state(power_switch_t newState)`
- Sets: New power state
- Triggers: Power sequencing actions
- Protected: Mutex or critical section

### Daemon Keepalive

**Purpose**: SoM daemon sends periodic keepalive messages to BMC

**Timeout Detection**:
```c
// Pseudo-code from daemon keepalive task
if (keepalive_received) {
    change_som_daemon_state(SOM_DAEMON_ON);
    reset_timeout_counter();
} else if (timeout_exceeded) {
    change_som_daemon_state(SOM_DAEMON_OFF);
}
```

**Relevance to This Patch**:
- Reset feedback only valid when daemon is ON
- Daemon ON means keepalive messages actively received
- If SoM powered off: Daemon definitely not sending keepalive → state = DAEMON_OFF or POWER_OFF
- State check ensures daemon is actually running before honoring reset request

### GPIO Task Flag Handling

**hf_gpio_task()** processes multiple flag types:
```c
void hf_gpio_task(void *parameter)
{
    while (1) {
        uint32_t flags = wait_for_gpio_flags();

        if (flags & FLAGS_SOM_RST_OUT) {
            som_rst_feedback_process();  // This function patched
        }

        if (flags & FLAGS_MCU_RESET_SOM) {
            mcu_reset_som_process();
        }

        if (flags & FLAGS_KEY_USER_RST) {
            key_user_rst_process();
        }

        // ... other flag handlers
    }
}
```

**FLAGS_SOM_RST_OUT**:
- Set by: Communication from SoM daemon
- Cleared by: After processing
- **This patch**: Adds safety check before processing

### Other Reset Functions

**mcu_reset_som_process()**: BMC-initiated reset (different from som_rst_feedback_process)
```c
static void mcu_reset_som_process(void)
{
    printf("%s %d mcu reset som\n", __func__, __LINE__);
    // BMC decides to reset SoM
    // Should also check power state (verify if patched)
    som_reset_control(pdTRUE);
    osDelay(2);
    som_reset_control(pdFALSE);
}
```

**Recommendation**: Apply similar power state check to `mcu_reset_som_process()` for consistency

## Testing Recommendations

### Functional Verification

**Test 1: Normal Reset When Powered On**
```
Preconditions:
- SoM fully booted (power state = SOM_DAEMON_ON)

Steps:
1. Trigger SoM reset via daemon request
2. Observe serial console
3. Verify SoM reboots

Expected:
- Console shows: "som_rst_feedback_process som reset"
- Reset line toggles (measure with oscilloscope)
- SoM successfully reboots
- Power state transitions: DAEMON_ON → DAEMON_OFF → POWER_ON → DAEMON_ON

Pass Criteria:
- SoM reboots cleanly
- No errors logged
```

**Test 2: Reset Blocked When Powered Off**
```
Preconditions:
- SoM powered off (power state = SOM_POWER_OFF)

Steps:
1. Manually trigger FLAGS_SOM_RST_OUT flag (via debugger or test code)
2. Observe serial console
3. Monitor reset line with oscilloscope

Expected:
- Console shows: "som_rst_feedback_process som reset"
- Console does NOT show reset execution
- Reset line remains INACTIVE (no toggle)
- SoM remains powered off

Pass Criteria:
- Reset line not toggled
- No electrical activity on reset pin
- SoM power state unchanged
```

**Test 3: Reset Blocked During Power Sequencing**
```
Preconditions:
- SoM power-on initiated (power state = SOM_POWER_ON)
- Power sequence in progress (not yet complete)

Steps:
1. During power-on sequence, trigger reset request
2. Observe behavior

Expected:
- Power state is SOM_POWER_ON (not SOM_DAEMON_ON)
- Reset blocked by if condition
- Power sequence continues uninterrupted
- After power sequence complete: state → SOM_DAEMON_ON

Pass Criteria:
- Reset not executed during power-on
- Power sequence completes successfully
```

**Test 4: Reset Blocked When Daemon Offline**
```
Preconditions:
- SoM powered (power state = SOM_POWER_ON or SOM_DAEMON_OFF)
- Daemon process crashed or not started

Steps:
1. Kill SoM daemon process (if running)
2. Wait for daemon timeout → state transitions to SOM_DAEMON_OFF
3. Trigger reset request
4. Observe behavior

Expected:
- Power state is SOM_DAEMON_OFF (not SOM_DAEMON_ON)
- Reset blocked
- SoM remains in current state

Pass Criteria:
- Reset not executed
- SoM power state unchanged
```

### Race Condition Testing

**Test 5: Concurrent Power-Off and Reset**
```
Setup:
- Automated test framework
- SoM fully operational (DAEMON_ON)

Steps:
1. Thread 1: Trigger power-off request
2. Thread 2 (10ms later): Trigger reset request
3. Repeat 100 times

Expected:
- Some iterations: Reset executes before power-off (valid)
- Some iterations: Power-off executes before reset reaches processing (reset blocked)
- Never: Reset on unpowered SoM

Pass Criteria:
- 100/100 iterations: No reset attempted when SoM powered off
- No GPIO electrical violations (measure with scope)
```

**Test 6: Spurious Flag During Power Transition**
```
Setup:
- Inject FLAGS_SOM_RST_OUT at various points in power sequence

Steps:
1. Start SoM power-on
2. At T=0ms, 50ms, 100ms, 200ms: Trigger reset flag
3. Monitor which resets execute

Expected:
- T=0-150ms: Power state not yet DAEMON_ON → resets blocked
- T=200ms+: Power state DAEMON_ON → reset executes

Pass Criteria:
- Resets only execute after DAEMON_ON state reached
```

### Hardware Verification

**Test 7: GPIO Electrical Characteristics**
```
Equipment:
- Oscilloscope on MCU_RESET_SOM_N pin
- Power supply monitoring SoM VDD

Steps:
1. SoM powered off (VDD = 0V)
2. Trigger reset request
3. Measure reset pin voltage and current

Expected:
- Reset pin remains HIGH or HiZ (not driven)
- No current flow into SoM reset pin
- SoM VDD remains 0V (no parasitic power)

Pass Criteria:
- Reset pin voltage: 3.3V or floating (not driven low)
- Current into reset pin: <1µA (leakage only)
```

**Test 8: Long-Term GPIO Stress Test**
```
Setup:
- Automated test running 24 hours
- SoM powered off entire time

Steps:
1. Every 10 seconds: Trigger reset request
2. Monitor for GPIO damage indicators:
   - Pin voltage drift
   - Current increase
   - Signal integrity degradation
3. After 24 hours: Verify GPIO still functional

Expected:
- 8,640 reset requests, all blocked
- No GPIO electrical parameter changes
- GPIO remains functional after test

Pass Criteria:
- GPIO electrical specs within normal ranges
- No degradation observed
```

## Best Practices

### General Pattern: Check Power Before Hardware Control

**This patch establishes a pattern that should be applied to ALL hardware control functions**:

```c
// CORRECT Pattern:
void hardware_control_function(void)
{
    if (is_hardware_powered_and_ready()) {
        // Perform hardware operation
        control_hardware();
    } else {
        // Log or silently skip
        printf("Hardware not ready, skipping operation\n");
    }
}

// INCORRECT Pattern:
void hardware_control_function(void)
{
    // Directly control hardware - DANGEROUS!
    control_hardware();
}
```

**Apply to**:
- `som_reset_control()` - Already has power check (this patch)
- `set_bootsel()` - Should check power state before changing boot mode pins
- SPI/UART/I2C to SoM - Should verify SoM powered before transactions
- LED control on SoM - Should check power before GPIO writes
- Any BMC→SoM GPIO control

### Defensive Programming Principles

1. **Assume Input Invalid**: Don't trust flags/signals without validation
2. **Verify Preconditions**: Check system state before operations
3. **Fail Safe**: If uncertain, skip operation rather than risk damage
4. **Log Blocked Operations**: Help debugging ("reset blocked, SoM powered off")
5. **Document Assumptions**: Comment why checks are necessary

## Potential Enhancements

### Enhancement 1: Logging for Blocked Resets

**Current Code**:
```c
if (get_som_power_state() == SOM_DAEMON_ON) {
    som_reset_control(pdTRUE);
    osDelay(2);
    som_reset_control(pdFALSE);
}
// If blocked, function silently returns
```

**Enhanced Code**:
```c
if (get_som_power_state() == SOM_DAEMON_ON) {
    som_reset_control(pdTRUE);
    osDelay(2);
    som_reset_control(pdFALSE);
} else {
    printf("Reset request ignored: SoM power state = %d (not DAEMON_ON)\n",
           get_som_power_state());
}
```

**Benefit**: Debugging - shows why reset didn't execute

### Enhancement 2: Reset Request Queuing

**Current**: Reset request lost if SoM not in DAEMON_ON state

**Enhanced**:
```c
static bool pending_reset = false;

void som_rst_feedback_process(void)
{
    printf("%s %d som reset\n", __func__, __LINE__);
    if (get_som_power_state() == SOM_DAEMON_ON) {
        som_reset_control(pdTRUE);
        osDelay(2);
        som_reset_control(pdFALSE);
        pending_reset = false;
    } else {
        // Queue reset for when SoM becomes ready
        pending_reset = true;
    }
}

// In power state change handler:
void on_power_state_change(power_switch_t new_state)
{
    if (new_state == SOM_DAEMON_ON && pending_reset) {
        som_rst_feedback_process();
    }
}
```

**Trade-off**: More complexity, may execute stale reset later (is this desired?)

### Enhancement 3: Timeout for Power State

**Current**: Checks power state once, proceeds or blocks

**Enhanced**:
```c
void som_rst_feedback_process(void)
{
    printf("%s %d som reset\n", __func__, __LINE__);

    // Wait up to 5 seconds for SoM to become ready
    for (int i = 0; i < 50; i++) {
        if (get_som_power_state() == SOM_DAEMON_ON) {
            som_reset_control(pdTRUE);
            osDelay(2);
            som_reset_control(pdFALSE);
            return;
        }
        osDelay(100);  // Check every 100ms
    }

    printf("Reset timeout: SoM never reached DAEMON_ON state\n");
}
```

**Trade-off**: Blocks task for up to 5s, may not be desirable

**Recommendation**: Current implementation (immediate check, no wait) is appropriate for most use cases

## Files Modified

```
Core/Src/hf_gpio_process.c | 8 +++++---
1 file changed, 5 insertions(+), 3 deletions(-)
```

**Diff Summary**: 3 lines deleted, 5 lines added (net +2 lines for the if statement)

## Changelog Entry

```
Changelogs:
1. Judge the return value of get_som_power_state() before doing the reset
```

## Related Patches

- **Patch 0057**: Power button sends power-off to MCPU (related power control)
- **Patch 0069**: Restart command implementation (may also need power check)
- **Patch 0087**: Power on/off logic exception repair (timer management during power transitions)

## Conclusion

This patch fixes an important hardware safety issue by preventing the BMC from attempting to reset an unpowered SoM. The fix is minimal (adding a single power state check) but critical for protecting GPIO hardware and ensuring clean power sequencing. The conservative approach (checking for SOM_DAEMON_ON specifically) ensures reset operations only occur when the SoM is fully operational and able to handle the reset signal properly.

**Criticality**: Important - Prevents hardware damage and undefined behavior
**Complexity**: Low - Simple conditional check
**Impact**: Positive - Improves hardware safety and system reliability
**Risk**: Very low - Only blocks invalid operations, doesn't change valid behavior
