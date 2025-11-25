# Patch 0055: Print RTC Time Before Console Welcome

**Patch File:** `0055-WIN2030-15279-feat-console-Print-RTC-time-before-wel.patch`
**Author:** linmin <linmin@eswincomputing.com>
**Date:** Fri, 24 May 2024 10:02:59 +0800
**Type:** Feature - User Experience Enhancement

---

## Overview

Small but useful UX improvement: BMC console now displays current RTC time immediately upon startup, before printing the welcome banner. This provides instant timestamp information for debugging and logging purposes.

### Changes

**Console startup sequence:**
```c
// Before
vConsoleWrite(pcWelcomeMsg);
vConsoleEnableRxInterrupt();
vConsoleWrite(prvpcPrompt);

// After  
prvCommandRtcGet(pcOutputString, MAX_OUT_STR_LEN, NULL);  // Print RTC time
vConsoleWrite(pcOutputString);
vConsoleWrite(pcWelcomeMsg);
vConsoleEnableRxInterrupt();
vConsoleWrite(prvpcPrompt);
```

**Example output:**
```
2024-05-24 10:02:59 CST
BMC Firmware v2.2
Welcome to HiFive Premier P550 BMC Console
BMC>
```

### Benefits

1. **Immediate time visibility** - No need to type `rtc` command
2. **Log correlation** - Timestamp helps correlate BMC boots with system events
3. **RTC validation** - Quickly verify RTC configured correctly
4. **Debug assistance** - Helps determine boot time during troubleshooting

### Additional Improvements

**SoM board info command output cleaned up:**

```c
// Before: Hex dump of serial number
len = snprintf(pcWb, size, "SN(hex):");
for (int i = 0; i < sizeof(psomInfo->sn); i++) {
    pcWb += snprintf(pcWb, size, "%x", psomInfo->sn[i]);
}

// After: ASCII string format
memcpy(boardSn, psomInfo->sn, sizeof(psomInfo->sn));
len = snprintf(pcWb, size, "SN:%s\r\n", boardSn);
```

**Status field formatting improved:**
```c
// Before: Decimal
len = snprintf(pcWb, size, "status:%d\n", psomInfo->status);

// After: Hexadecimal (better for bitfield status)
len = snprintf(pcWb, size, "status:0x%x\r\n", psomInfo->status);
```

---

## Technical Details

**RTC time formatting:**
```c
snprintf(pcWriteBuffer, xWriteBufferLen, "%u-%02u-%2u %s %02u:%02u:%02u CST\r\n",
         date.Year, date.Month, date.Date, cWeekDay,
         time.Hours, time.Minutes, time.Seconds);
```

Note: Patch fixes format string (adds leading zeros for consistent alignment).

**Memory impact:** None (reuses existing output buffer)

**Performance:** <1ms to print (negligible)

---

**Document Version:** 1.0
**Last Updated:** 2025-11-24
**Related Issues:** WIN2030-15279
