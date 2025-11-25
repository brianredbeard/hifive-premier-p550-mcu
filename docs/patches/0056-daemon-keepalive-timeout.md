# Patch 0056: Daemon Keep-Alive Timeout and Logging Improvements

**Patch File:** `0056-WIN2030-15099-fix-daemon-keep-live-timeout-when-lost.patch`
**Author:** huangyifeng <huangyifeng@eswincomputing.com>
**Date:** Fri, 24 May 2024 09:58:10 +0800
**Type:** Fix - Daemon Reliability

---

## Overview

Critical fix for BMC-to-SoM communication daemon, addressing timeout logic bug and improving observability through enhanced logging.

## Key Changes

### 1. Fixed Timeout Logic (Critical Bug)

**Before (BROKEN):**
```c
count++;
if (5 >= count) {  // WRONG: Always true when count>5!
    change_som_daemon_state(SOM_DAEMON_OFF);
}
```

**After (CORRECT):**
```c
count++;
if (count >= 5) {  // FIXED: Timeout after 5 failed packets
    change_som_daemon_state(SOM_DAEMON_OFF);
}
```

**Impact of bug:**
- Daemon never declared SoM offline (timeout never triggered)
- BMC reported SoM as "alive" even when unresponsive
- Power management decisions based on incorrect state

### 2. Increased Poll Rate

```c
// Before: Check every 4 seconds
osDelay(pdMS_TO_TICKS(4000));

// After: Check every 1 second
osDelay(pdMS_TO_TICKS(1000));
```

**Timeout behavior:**
- Old: 5 × 4s = 20 seconds to detect SoM offline
- New: 5 × 1s = **5 seconds** to detect SoM offline

**Rationale:**
- Faster detection of SoM failures
- More responsive power management
- Better UX (quicker web interface updates)

### 3. Enhanced Status Change Logging

```c
// Before: Simple message
printf("SOM Daemon status change to %s!\n",
       get_som_daemon_state() == SOM_DAEMON_ON ? "on" : "off");

// After: Timestamp included
es_get_rtc_date(&date);
es_get_rtc_time(&time);
printf("SOM Daemon status change to %s at %d-%02d-%02d %02d:%02d:%02d!\n",
       get_som_daemon_state() == SOM_DAEMON_ON ? "on" : "off",
       date.Year, date.Month, date.Date,
       time.Hours, time.Minutes, time.Seconds);
```

**Benefits:**
- Correlate daemon events with other system logs
- Debug intermittent connection issues
- Track SoM uptime/downtime patterns

### 4. Increased Timeout for Power Operations

**Console and web server power commands:**
```c
// Before: 1 second timeout
ret = web_cmd_handle(CMD_POWER_OFF, NULL, 0, 1000);
ret = web_cmd_handle(CMD_RESET, NULL, 0, 1000);

// After: 2 second timeout
ret = web_cmd_handle(CMD_POWER_OFF, NULL, 0, 2000);
ret = web_cmd_handle(CMD_RESET, NULL, 0, 2000);
```

**Why increase:**
- SoM needs time to gracefully shutdown (close files, sync disk)
- 1 second insufficient for Linux shutdown procedures
- 2 seconds allows clean poweroff, reducing corruption risk

---

## Daemon Keep-Alive Protocol

**Purpose:** Verify SoM operating system still running

**Mechanism:**
1. BMC sends keep-alive request via SPI protocol every 1 second
2. SoM daemon process responds with ACK
3. If 5 consecutive requests fail → SoM declared offline

**States:**
- `SOM_DAEMON_ON`: SoM responding to keep-alives
- `SOM_DAEMON_OFF`: SoM not responding (crashed, powered off, or busy)

**Distinction from power state:**
- `SOM_POWER_ON`: Hardware powered (voltage rails enabled)
- `SOM_DAEMON_ON`: Software running (OS responsive)
- Can have power ON but daemon OFF (crash, kernel panic)

---

## Impact on User-Facing Features

### Web Interface SoC Status

**API endpoint `/soc-status`:**
```c
int get_soc_status() {
    return SOM_DAEMON_ON == get_som_daemon_state() ? 0 : 1;
}
```

Returns:
- `0` = "working" (daemon responding)
- `1` = "stopped" (daemon timeout or power off)

**With faster timeout:**
- Status updates within 5 seconds of SoM crash
- Previously: Could take 20+ seconds to reflect failure

### Graceful Shutdown

**Power button press handling:**
```c
ret = web_cmd_handle(CMD_POWER_OFF, NULL, 0, 2000);  // 2s timeout
if (HAL_OK != ret) {
    change_som_power_state(SOM_POWER_OFF);  // Force shutdown
}
TriggerSomPowerOffTimer();  // 5-second hard cutoff
```

**Shutdown sequence:**
1. BMC sends CMD_POWER_OFF to SoM daemon
2. SoM initiates graceful shutdown (`systemctl poweroff`)
3. BMC waits up to 2 seconds for ACK
4. If no ACK or timeout, force power cut

**Improved reliability:**
- 2-second timeout accommodates slow shutdowns
- Still protects against hung OS (5-second hard timer)

---

## Testing Recommendations

### 1. Daemon Timeout Test
```
Scenario: SoM daemon process killed
1. SSH to SoM, kill daemon: `killall som-daemon`
2. Observe BMC console logs
3. Expected: "SOM Daemon status change to off at [timestamp]" within 5 seconds
```

### 2. Graceful Shutdown Test
```
Scenario: Normal power-off
1. Press power button
2. Observe SoM console for shutdown messages
3. Expected: "systemd[1]: Reached target Shutdown" before power cut
```

### 3. Hung OS Test
```
Scenario: SoM kernel panic or freeze
1. Simulate hang: `echo c > /proc/sysrq-trigger` (requires debug kernel)
2. Observe BMC behavior
3. Expected: Daemon timeout → force power off after 5s total
```

---

## Code Quality Improvements

**Minor whitespace cleanup:**
```c
// Trailing whitespace removed from comments
while (current->next != NULL) {//handle node:current->next
```

Not functionally significant, but improves code hygiene.

---

## Conclusion

Patch 0056 fixes critical daemon timeout logic while improving observability:

✅ **Bug fix:** Corrected inverted timeout condition
✅ **Faster detection:** 5-second timeout (was 20 seconds)
✅ **Better logging:** Timestamps on daemon state changes
✅ **Safer shutdowns:** 2-second graceful shutdown window

**Essential for reliable SoM monitoring.**

---

**Document Version:** 1.0
**Last Updated:** 2025-11-24
**Related Issues:** WIN2030-15099
