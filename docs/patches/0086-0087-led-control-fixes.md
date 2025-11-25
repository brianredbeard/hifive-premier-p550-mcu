# Patches 0086-0087: LED Control Fixes

## Combined Documentation

These two closely-related patches (0086 and 0087) fix LED behavior during power state transitions and resolve a power control logic exception caused by improperly managed timers.

---

## Patch 0086: Power LED Turn-Off When Powered Off

### Metadata
- **Patch Number**: 0086
- **Ticket**: WIN2030-15279
- **Type**: fix
- **Component**: hf_power_process - LED control
- **Author**: xuxiang <xuxiang@eswincomputing.com>
- **Date**: Wed, 12 Jun 2024 09:47:39 +0800
- **Files Modified**: 1
  - `Core/Src/hf_power_process.c`

### Summary

This patch fixes inverted LED logic that caused the chassis power LED and sleep LED to display incorrectly during SoM power state transitions. The power LED now correctly turns ON when SoM is powered, and OFF when powered off. Similarly, the sleep LED now correctly indicates sleep state.

### Problem Description

**Incorrect LED Behavior** (Before Fix):
- **SoM Powered ON**: Sleep LED ON, Power LED OFF (backwards!)
- **SoM Powered OFF**: Power LED ON, Sleep LED OFF (backwards!)

**Expected LED Behavior**:
- **SoM Powered ON**: Power LED ON, Sleep LED OFF
- **SoM Powered OFF**: Power LED OFF, Sleep LED ON

**User Impact**:
- Confusing LED indicators - LED state opposite of actual state
- Users cannot trust visual indicators for power status
- Troubleshooting difficult when LEDs lie about state

### Root Cause

**Function**: `chass_led_ctl(uint8_t turnon)` in `Core/Src/hf_power_process.c`

**Parameter**: 
- `turnon == 1`: SoM is powered ON
- `turnon == 0`: SoM is powered OFF

**Buggy Code Logic**:
```c
if (board_revision == BOARD_REV_DVB2) {
    // DVB2 path (correct)
    power_led_on(turnon);
    osDelay(10);
    sleep_led_on(turnon);
} else {
    // Non-DVB2 path (BUGGY - inverted logic!)
    power_led_on(!turnon);   // INVERTED: ON when should be OFF
    osDelay(10);
    sleep_led_on(turnon);    // WRONG: Should be inverted
}
```

**Analysis**:
- DVB-2 hardware: LED logic is correct (active high)
- Non-DVB-2 hardware (DVB-1): LED logic was inverted
- Likely cause: Different LED driver circuitry between board revisions

### Technical Details

**File**: `Core/Src/hf_power_process.c`
**Function**: `chass_led_ctl(uint8_t turnon)` (Lines 219-227)

**Before**:
```c
static void chass_led_ctl(uint8_t turnon)
{
    if (board_revision == BOARD_REV_DVB2) {
        sleep_led_on(turnon);
        osDelay(10);
        power_led_on(turnon);
    } else {
        power_led_on(!turnon);    // BUG: Inverted
        osDelay(10);
        sleep_led_on(turnon);     // BUG: Not inverted (should be)
    }
}
```

**After**:
```c
static void chass_led_ctl(uint8_t turnon)
{
    if (board_revision == BOARD_REV_DVB2) {
        sleep_led_on(turnon);
        osDelay(10);
        power_led_on(turnon);
    } else {
        power_led_on(turnon);     // FIXED: Direct, not inverted
        osDelay(10);
        sleep_led_on(!turnon);    // FIXED: Inverted
    }
}
```

**Changes**:
- **Line 23**: `power_led_on(!turnon)` → `power_led_on(turnon)` (remove inversion)
- **Line 25**: `sleep_led_on(turnon)` → `sleep_led_on(!turnon)` (add inversion)

**Corrected Logic**:
```
turnon == 1 (SoM powered ON):
  - power_led_on(1) → Power LED ON
  - sleep_led_on(0) → Sleep LED OFF

turnon == 0 (SoM powered OFF):
  - power_led_on(0) → Power LED OFF
  - sleep_led_on(1) → Sleep LED ON
```

### LED Control Functions

**power_led_on(uint8_t on)**:
- `on == 1`: Turn power LED ON (GPIO high or PWM active)
- `on == 0`: Turn power LED OFF (GPIO low or PWM inactive)

**sleep_led_on(uint8_t on)**:
- `on == 1`: Turn sleep LED ON
- `on == 0`: Turn sleep LED OFF

**Implementation** (typical):
```c
void power_led_on(uint8_t on)
{
    if (on) {
        HAL_GPIO_WritePin(POWER_LED_GPIO_Port, POWER_LED_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(POWER_LED_GPIO_Port, POWER_LED_Pin, GPIO_PIN_RESET);
    }
}
```

**Note**: Actual implementation may use PWM for brightness control (see Patch 0076)

### Board Revision Differences

**DVB-2 Hardware**:
- LED driver circuitry: Active-high (GPIO high = LED ON)
- Power LED: GPIO high → LED ON
- Sleep LED: GPIO high → LED ON

**DVB-1 Hardware**:
- LED driver circuitry: Different configuration
- Possibly active-low or inverted driver
- This patch corrects for the difference

**Key Point**: Same firmware must support both hardware revisions by detecting board revision and adjusting LED logic accordingly.

### Impact

**Before Fix** (DVB-1 boards):
- Power LED always shows OPPOSITE of actual state
- Sleep LED doesn't indicate sleep state correctly
- User confusion and support calls

**After Fix**:
- Power LED accurately reflects power state
- Sleep LED accurately reflects sleep/active state
- Consistent behavior across all board revisions

---

## Patch 0087: Power On/Off Logic Exception Repair

### Metadata
- **Patch Number**: 0087
- **Ticket**: WIN2030-15279
- **Type**: fix
- **Component**: hf_gpio_process, web-server - Timer management
- **Author**: xuxiang <xuxiang@eswincomputing.com>
- **Date**: Wed, 12 Jun 2024 11:15:54 +0800
- **Files Modified**: 2
  - `Core/Src/hf_gpio_process.c`
  - `Core/Src/web-server.c`

### Summary

This patch fixes a critical power control bug where the power-off timer was not stopped when transitioning to power-on state. This caused the system to unexpectedly power off shortly after being powered on, creating an unusable power state oscillation.

### Problem Description

**Symptom**:
```
1. User powers ON SoM (via button or web interface)
2. SoM starts power-on sequence
3. Unexpectedly: SoM powers OFF after few seconds
4. User confused - "I just turned it on!"
```

**Root Cause**: Power-off timer not stopped

**Detailed Failure Scenario**:

```
T=0s:   User presses power button (SoM currently OFF)
        - Power-off timer STARTS (incorrectly - starting timer during power-off!)
        
T=1s:   User releases button (short press)
        - Triggers power-ON request
        - change_som_power_state(SOM_POWER_ON) called
        - Power sequence begins
        - Power-off timer STILL RUNNING (BUG!)
        
T=2s:   SoM rails powering up...
        
T=3s:   SoM boots, daemon starting...
        
T=5s:   POWER-OFF TIMER EXPIRES!
        - Timer callback: change_som_power_state(SOM_POWER_OFF)
        - SoM immediately powers down
        - User: "What just happened?!"
```

**Why Timer Started During Power-Off**:
- Complex power button state machine
- Timer started as part of power-off sequence initiation
- But if user releases button quickly → triggers power-on instead
- Timer left running, fires later

### Technical Details

#### Change 1: hf_gpio_process.c - Button Handler

**File**: `Core/Src/hf_gpio_process.c`
**Function**: `key_process()` - Power button state machine

**Location**: Line 111

**Added Line**:
```c
if (get_som_power_state() != SOM_POWER_ON) {
    vStopSomPowerOffTimer();  // NEW: Stop power-off timer before power-on
    change_som_power_state(SOM_POWER_ON);
}
```

**Context** (simplified):
```c
static void key_process(void)
{
    switch (button_state) {
        case KEY_SHORT_PRESS_STATE:
            button_state = KEY_PRESS_STATE_END;
            if (get_som_power_state() != SOM_POWER_ON) {
                vStopSomPowerOffTimer();  // ADDED
                change_som_power_state(SOM_POWER_ON);
            }
            break;
        // ... other states
    }
}
```

**Why This Fix**:
- Short button press detected → power-on requested
- **Before initiating power-on**: Stop any running power-off timer
- Prevents timer from firing after power-on completes
- Clean state transition: power-off → power-on (timer cleared)

#### Change 2: web-server.c - Web Power Control

**File**: `Core/Src/web-server.c`
**Function**: `change_power_status(int status)` - Web interface power control

**Location**: Line 10474

**Added Line**:
```c
if (SOM_POWER_OFF == get_som_power_state()) {
    vStopSomPowerOffTimer();  // NEW: Stop power-off timer before power-on
    change_som_power_state(SOM_POWER_ON);
} else {
    web_debug("SOM already power on!\n");
}
```

**Context**:
```c
int change_power_status(int status)
{
    if (status == 0) {  // Power OFF requested
        // ... power-off logic
    } else {  // Power ON requested
        if (SOM_POWER_OFF == get_som_power_state()) {
            vStopSomPowerOffTimer();  // ADDED
            change_som_power_state(SOM_POWER_ON);
        } else {
            web_debug("SOM already power on!\n");
        }
    }
}
```

**Why This Fix**:
- User clicks "Power On" button on web interface
- Before initiating power-on: Stop any running power-off timer
- Same issue as button press: timer could be left running
- Ensures clean power-on from web interface

### Timer Management Functions

**vStopSomPowerOffTimer()**: Stop power-off timer (prevent callback)

**Typical Implementation**:
```c
static TimerHandle_t xSomPowerOffTimer = NULL;

void TriggerSomPowerOffTimer(void)
{
    if (xSomPowerOffTimer == NULL) {
        xSomPowerOffTimer = xTimerCreate(
            "SomPowerOffTimer",
            pdMS_TO_TICKS(5000),  // 5 second timeout
            pdFALSE,              // One-shot timer
            (void *)0,
            vSomPowerOffTimerCallback
        );
    }
    xTimerStart(xSomPowerOffTimer, 0);
}

void vStopSomPowerOffTimer(void)
{
    if (xSomPowerOffTimer != NULL) {
        xTimerStop(xSomPowerOffTimer, 0);
    }
}

static void vSomPowerOffTimerCallback(TimerHandle_t xTimer)
{
    // Timer expired - initiate power-off
    change_som_power_state(SOM_POWER_OFF);
}
```

**Purpose of Timer**:
- Delayed power-off (graceful shutdown)
- User presses power button → timer starts
- If SoM acknowledges shutdown within timeout → timer stopped, graceful power-off
- If timeout expires → forced power-off

**Bug**: Timer not stopped when changing from power-off to power-on

### State Transitions

**Corrected Flow** (After Patch):

```
Power Button Short Press (SoM OFF):
    ↓
Button State Machine: SHORT_PRESS_STATE
    ↓
Check: get_som_power_state() != SOM_POWER_ON? YES
    ↓
vStopSomPowerOffTimer() ← STOP any running timer
    ↓
change_som_power_state(SOM_POWER_ON)
    ↓
Power sequence begins
    ↓
No timer interruptions
    ↓
SoM successfully boots
```

**Bug Flow** (Before Patch):

```
Power Button Sequence:
    ↓
Timer started during power-off logic (edge case)
    ↓
Button released quickly
    ↓
SHORT_PRESS detected → power-on requested
    ↓
change_som_power_state(SOM_POWER_ON)
    ↓
Timer STILL RUNNING ← BUG!
    ↓
Power sequence begins
    ↓
Timer expires → vSomPowerOffTimerCallback()
    ↓
change_som_power_state(SOM_POWER_OFF)
    ↓
SoM powers down unexpectedly
```

### Impact

**Before Fix**:
- Intermittent power-on failures
- SoM powers off shortly after power-on
- Difficult to reproduce (timing-dependent)
- Users report "power button doesn't work reliably"

**After Fix**:
- Reliable power-on from button
- Reliable power-on from web interface
- No unexpected power-offs
- Clean state transitions

## Testing Both Patches Together

### Test 1: LED Behavior Verification

```
Hardware: DVB-1 or DVB-2 board

Steps:
1. Power OFF SoM (via button or web)
2. Observe LEDs

Expected:
- Power LED: OFF
- Sleep LED: ON

3. Power ON SoM
4. Observe LEDs

Expected:
- Power LED: ON
- Sleep LED: OFF

Pass Criteria:
- LEDs correctly indicate power state
- No inverted behavior
```

### Test 2: Rapid Power Button Press

```
Steps:
1. SoM powered OFF
2. Press power button
3. Release immediately (short press, <1 second)
4. Wait 10 seconds
5. Observe SoM state

Expected:
- SoM powers ON
- SoM STAYS ON (no unexpected power-off)
- Power LED ON, Sleep LED OFF

Pass Criteria:
- SoM remains powered on
- No timer-induced power-off
```

### Test 3: Web Interface Power Control

```
Steps:
1. SoM powered OFF
2. Click "Power On" button on web interface
3. Wait 10 seconds
4. Observe SoM state

Expected:
- SoM powers ON
- SoM STAYS ON
- Power LED ON, Sleep LED OFF

Pass Criteria:
- Reliable power-on from web
- No unexpected power-off
```

### Test 4: Power State Transitions

```
Steps:
1. Power ON → verify LEDs correct
2. Power OFF → verify LEDs correct
3. Repeat 20 times rapidly

Expected:
- Every transition: LEDs update correctly
- No stuck LEDs
- No unexpected power state changes

Pass Criteria:
- 20/20 transitions successful
- LEDs always match power state
```

## Files Modified

**Patch 0086**:
```
Core/Src/hf_power_process.c | 4 ++--
1 file changed, 2 insertions(+), 2 deletions(-)
```

**Patch 0087**:
```
Core/Src/hf_gpio_process.c | 1 +
Core/Src/web-server.c      | 1 +
2 files changed, 2 insertions(+)
```

## Changelog Entries

**Patch 0086**:
```
Changelogs:
1. when som is powered on, the power led on the chassis is turn on and the sleep led is turn off
2. when som is powered off, the power led on the chassis is turn off and the sleep led is turn on
```

**Patch 0087**:
```
Changelogs:
1. The power off timer is not turned off in time, it will change the power state to poweroff when it is powered on
```

## Related Patches

- **Patch 0076**: LED PWM brightness control
- **Patch 0075**: DVB-2 board support (board_revision detection)
- **Patch 0057**: Power button sends power-off to MCPU (button state machine)

## Conclusion

These two patches work together to improve power control reliability and LED indication accuracy:

**Patch 0086**: Fixes LED inversion bug for non-DVB-2 boards, ensuring power and sleep LEDs correctly reflect system state.

**Patch 0087**: Fixes critical timer management bug that caused unexpected power-offs after power-on requests.

Together, they significantly improve the user experience by providing reliable power control and accurate visual feedback through chassis LEDs.
