# Patch 0057: Power Button Sends Graceful Power-Off to SoM

**Patch File:** `0057-WIN2030-15279-perf-push-key-send-poweroff-to-mcpu.patch`
**Author:** xuxiang <xuxiang@eswincomputing.com>
**Date:** Thu, 23 May 2024 13:28:55 +0800
**Type:** Performance - Graceful Shutdown Enhancement

---

## Overview

Improves power button handling to perform **graceful shutdown** of SoM operating system instead of immediate power cut. Also fixes parameter passing bug in I2C reinitialization calls.

## Key Changes

### 1. Graceful Power-Off via Daemon Protocol

**GPIO Key Processing Enhancement:**

**Before (Immediate Power Cut):**
```c
case KEY_LONG_PRESS_STATE:
    printf("KEY_LONG_PRESS_STATE time %ld\n", currentTime - pressStartTime);
    button_state = KEY_PRESS_STATE_END;
    if (get_som_power_state() == SOM_POWER_ON) {
        change_som_power_state(SOM_POWER_OFF);  // IMMEDIATE CUT!
    }
    break;
```

**After (Graceful Shutdown):**
```c
case KEY_LONG_PRESS_STATE:
    printf("KEY_LONG_PRESS_STATE time %ld\n", currentTime - pressStartTime);
    button_state = KEY_PRESS_STATE_END;
    if (get_som_power_state() == SOM_POWER_ON) {
        ret = web_cmd_handle(CMD_POWER_OFF, NULL, 0, 2000);  // Ask SoM to shutdown
        if (HAL_OK != ret) {
            change_som_power_state(SOM_POWER_OFF);  // Force if no response
        }
        TriggerSomPowerOffTimer();  // 5-second safety timer
    }
    break;
```

**Shutdown sequence:**
1. User long-presses power button (>5 seconds)
2. BMC sends `CMD_POWER_OFF` to SoM daemon process
3. SoM daemon initiates OS shutdown: `systemctl poweroff` or equivalent
4. SoM syncs filesystems, unmounts partitions, stops services
5. SoM signals shutdown complete to BMC
6. BMC cuts power to SoM voltage rails

**Fallback protection:**
- If SoM doesn't respond within 2 seconds → forced power cut
- If SoM doesn't power off within 5 seconds → timer expires, forced cut
- Prevents indefinite hang if OS unresponsive

---

## Benefits

### 1. Filesystem Integrity

**Without graceful shutdown:**
- Filesystem caches not flushed → data loss
- Partition tables left in inconsistent state
- Journal not committed → corruption on next boot
- Running processes killed mid-write → partial file writes

**Example scenario (before patch):**
```
User compiling kernel on SoM:
1. gcc writes object files to /tmp
2. User presses power button
3. Power immediately cut
4. Object files half-written, corrupted
5. Next boot: filesystem errors, forced fsck
```

**With graceful shutdown:**
```
1. gcc writes object files
2. User presses power button
3. BMC sends CMD_POWER_OFF
4. SoM: sync; systemctl poweroff
5. All caches flushed, filesystems unmounted cleanly
6. Power cut after clean shutdown
7. Next boot: clean filesystem, no fsck needed
```

### 2. Application State Preservation

**Services can cleanup:**
- Database commits pending transactions
- Web servers close connections gracefully
- Logging daemons flush buffers
- Configuration saved to disk

**Example: Logging daemon**
```
Before: Log entries lost (in buffer, not written)
After:  SIGTERM → daemon flushes → all logs persisted
```

### 3. Reduced Wear on eMMC/SD Card

**Flash wear considerations:**
- Unclean shutdowns trigger fsck on boot
- fsck performs extensive writes (scanning, journaling)
- More writes → faster flash wear
- Graceful shutdown → clean state → no fsck → reduced writes

**Lifespan impact:**
- Unclean shutdown: ~100-200 MB written during fsck
- Clean shutdown: <1 MB written
- With 100K write cycles typical for eMMC: significant lifespan extension

---

## I2C Reinitialization Parameter Fix

### Bug: Incorrect Parameter Type

**All I2C wrapper functions had same bug:**

```c
// WRONG: Passing pointer to pointer
hf_i2c_reinit(&hi2c);

// CORRECT: Passing pointer
hf_i2c_reinit(hi2c);
```

**Function signature:**
```c
void hf_i2c_reinit(I2C_HandleTypeDef *hi2c)  // Expects pointer
```

**Why this worked despite bug:**
In C:
- `&hi2c` where `hi2c` is already pointer → double pointer
- Dereferencing in function: `hi2c->Instance` treats double pointer as single pointer
- **Accidental correctness** due to pointer representation

**Why fix anyway:**
- Semantically wrong (type confusion)
- Compiler warnings (`-Wincompatible-pointer-types`)
- Fragile - breaks if function implementation changes
- Code clarity - intent unclear

**Fixed in 7 function calls:**
1. `hf_i2c_reg_write()`
2. `hf_i2c_reg_read()`
3. `hf_i2c_mem_read()`
4. `hf_i2c_mem_write()` (3 locations)
5. `hf_i2c_reg_write_block()`
6. `hf_i2c_reg_read_block()`

### Debug Logging Added

```c
void hf_i2c_reinit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1) {
        printf("I2C1 reinit\n");  // NEW: Log reinitialization
        i2c_deinit(I2C1);
        i2c_init(I2C1);
    }
    else if (hi2c->Instance == I2C3) {
        printf("I2C3 reinit\n");  // NEW: Log reinitialization
        i2c_deinit(I2C3);
        i2c_init(I2C3);
    }
}
```

**Diagnostic benefit:**
- Track frequency of I2C errors
- Identify problematic devices (by correlation with transaction logs)
- Debug bus reliability issues

---

## Power Button State Machine

**Key press detection:**
```c
while (key_status == KEY_PUSHDOWN) {
    currentTime = xTaskGetTickCount();
    
    switch (button_state) {
        case KEY_PRESS_STATE_INIT:
            pressStartTime = currentTime;
            button_state = KEY_SHORT_PRESS_STATE;
            break;
            
        case KEY_SHORT_PRESS_STATE:
            if (currentTime - pressStartTime >= SHORT_PRESS_TIME) {
                button_state = KEY_LONG_PRESS_STATE;
            }
            break;
            
        case KEY_LONG_PRESS_STATE:
            // Trigger graceful shutdown (THIS PATCH)
            break;
    }
}
```

**Timing thresholds:**
- Short press: <5 seconds → Power toggle (on↔off)
- Long press: ≥5 seconds → Graceful shutdown (if on)

**Safety timer `TriggerSomPowerOffTimer()`:**
- Starts 5-second countdown after CMD_POWER_OFF sent
- If timer expires before SoM shuts down → forced power cut
- Prevents infinite wait on hung OS

---

## User Experience

### Expected Behavior

**Short press (<5s):**
```
If SoM OFF:
  → Power on SoM (normal boot)
  
If SoM ON:
  → Graceful shutdown → power off
```

**Long press (≥5s):**
```
If SoM OFF:
  → No action (already off)
  
If SoM ON:
  → Same as short press (graceful shutdown)
```

**Emergency force-off:**
```
If SoM hung/unresponsive:
  → Press button, BMC sends CMD_POWER_OFF
  → No response within 2s → forced power cut
  → Or wait 5s → timer expires → forced power cut
```

### Visual/Audio Feedback (Hardware Dependent)

Typical implementations:
- LED blinks during shutdown sequence
- Beep/click on button press
- LED solid when shutdown complete

Not implemented in this patch (future enhancement).

---

## Testing Scenarios

### 1. Normal Graceful Shutdown
```
Preconditions: SoM booted to Linux prompt
Steps:
  1. Long-press power button (5+ seconds)
  2. Observe SoM console output
Expected:
  - systemd shutdown messages appear
  - Filesystems unmounted
  - "Power down" message
  - Power cuts cleanly
```

### 2. Hung OS Shutdown
```
Preconditions: SoM kernel panic or frozen
Steps:
  1. Trigger kernel panic: echo c > /proc/sysrq-trigger
  2. Long-press power button
Expected:
  - BMC sends CMD_POWER_OFF
  - No response from SoM
  - Timeout after 2 seconds
  - Forced power cut
```

### 3. Daemon Not Running
```
Preconditions: SoM booted but daemon process killed
Steps:
  1. SSH to SoM, kill daemon
  2. Long-press power button
Expected:
  - BMC sends CMD_POWER_OFF
  - No daemon to receive
  - Timeout after 2 seconds
  - Forced power cut
```

### 4. Filesystem Integrity Validation
```
Steps:
  1. Boot SoM, create test file: echo "test" > /tmp/test.txt
  2. Start background write: dd if=/dev/zero of=/tmp/bigfile &
  3. Immediately press power button
Expected:
  - Graceful shutdown initiated
  - dd process terminated cleanly
  - Next boot: no fsck required
  - /tmp/test.txt intact
```

---

## Security Implications

### Physical Security Requirement

**Attack vector:** Physical access to power button

**Scenarios:**
1. **Attacker in datacenter** - presses button, BMC initiates shutdown
2. **Malicious insider** - uses button to disrupt service

**Mitigations:**
- Physical case/enclosure required
- Button inside locked cabinet
- Disable button in BIOS/firmware (not implemented)

**Residual risk:**
- Anyone with physical access can shutdown system
- **Standard for embedded systems** - physical security assumed

### DoS via Button Hold

**Scenario:**
```
Attacker holds button continuously:
  → BMC repeatedly sends CMD_POWER_OFF
  → SoM attempts shutdown
  → Interrupted by next button press
  → Shutdown never completes
  → System stuck in loop
```

**Mitigation (already present):**
- Timer expires after 5 seconds → forced cut
- Cannot hold indefinitely

**Conclusion:** Not a significant vulnerability.

---

## Conclusion

Patch 0057 significantly improves system reliability:

✅ **Graceful shutdown** - Preserves filesystem integrity
✅ **Fallback safety** - Force cut if SoM unresponsive
✅ **Code quality** - Fixed I2C reinit parameter bug
✅ **Observability** - Added reinit logging

**Recommended for all deployments requiring filesystem reliability.**

---

**Document Version:** 1.0
**Last Updated:** 2025-11-24
**Related Issues:** WIN2030-15279
