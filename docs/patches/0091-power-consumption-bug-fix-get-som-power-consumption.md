# Patch 0091: Power Consumption Bug Fix - Get SOM Power Consumption

## Metadata
- **Patch File**: `0091-WIN2030-15279-fix-power-consumption-Bug-fix-for-get_.patch`
- **Title**: WIN2030-15279:fix(power consumption):Bug fix for get som power consumption
- **Author**: linmin <linmin@eswincomputing.com>
- **Date**: Mon, 28 Oct 2024 17:55:21 +0800
- **Category**: Bug Fix
- **Subsystem**: Power Management / Daemon
- **Change-Id**: I50239ec0a7fdf33c74d3bb86f098e2aa4ee14bc3

## Description

### Problem Statement
The power consumption reporting logic had an incorrect implementation for retrieving SOM power consumption data through the protocol library. When the power consumption values were zero (indicating a communication error or invalid data), the daemon would incorrectly report these zero values as valid measurements.

### Root Cause Analysis
The bug was in the conditional logic within `bmc_daemon_task()`:

**Original Buggy Code**:
```c
if(protocol_get_som_power_consumption(&somPowerConsumption) == 0) {
    // Empty block - does nothing!
}

if(somPowerConsumption.voltage_5v == 0
    && somPowerConsumption.current_5v == 0
    && somPowerConsumption.voltage_12v == 0
    && somPowerConsumption.current_12v == 0) {
    /* Invalid values detected, fall back to INA226 */
    voltage_val = ina226_voltage;
    current_val = ina226_current;
    power_val = ina226_power;
} else {
    /* Use SOM values */
    // ... store somPowerConsumption values
}
```

The problem: The function retrieves `somPowerConsumption` but the **successful return check does nothing** with it. The subsequent zero-value detection always runs regardless of whether the protocol call succeeded or failed.

This means:
1. If `protocol_get_som_power_consumption()` fails (returns non-zero), the `somPowerConsumption` structure contains **uninitialized or stale data**
2. That stale data then gets evaluated in the zero-check condition
3. If the stale data happens to be non-zero, it gets used even though it's invalid
4. If the stale data is all zeros, it correctly falls back to INA226, but for the wrong reason

### Solution Implementation

**Fixed Code**:
```c
if(protocol_get_som_power_consumption(&somPowerConsumption) != 0) {
    /* Protocol call failed - fall back to INA226 immediately */
    voltage_val = ina226_voltage;
    current_val = ina226_current;
    power_val = ina226_power;
} else {
    /* Protocol call succeeded - now validate the data */
    if(somPowerConsumption.voltage_5v == 0
        && somPowerConsumption.current_5v == 0
        && somPowerConsumption.voltage_12v == 0
        && somPowerConsumption.current_12v == 0) {
        /* SOM returned all zeros - data invalid, fall back to INA226 */
        voltage_val = ina226_voltage;
        current_val = ina226_current;
        power_val = ina226_power;
    } else {
        /* SOM data valid - use it */
        voltage_val = somPowerConsumption.voltage_12v;
        current_val = somPowerConsumption.current_12v / 1000; // Convert µA to mA
        power_val = voltage_val * current_val / 1000;

        power_consumption_5v_val = somPowerConsumption.voltage_5v *
                                   somPowerConsumption.current_5v / 1000000;
        power_consumption_12v_val = power_val;

        voltage_5v_val = somPowerConsumption.voltage_5v;
        current_5v_val = somPowerConsumption.current_5v / 1000;
    }
}
```

**Key Improvements**:
1. **Explicit Failure Handling**: Check if protocol call fails (`!= 0`), immediately fall back
2. **Nested Validation**: Only validate data contents if protocol call succeeded
3. **Clear Logic Flow**: Separates "did the call succeed?" from "is the data valid?"
4. **Prevents Stale Data**: Never uses uninitialized `somPowerConsumption` values

## Technical Analysis

### Power Consumption Data Flow

**Normal Path (SOM Powered On and Responding)**:
```
1. protocol_get_som_power_consumption() → Success (returns 0)
2. Check if data is all zeros → False (valid data present)
3. Use SOM-provided values:
   - voltage_12v (in mV)
   - current_12v (in µA, converted to mA)
   - voltage_5v (in mV)
   - current_5v (in µA, converted to mA)
4. Calculate power consumption:
   - power_12v = voltage_12v * current_12v / 1000 (in mW)
   - power_5v = voltage_5v * current_5v / 1000000 (in mW)
```

**Fallback Path #1 (Protocol Failure - SOM Off or Unresponsive)**:
```
1. protocol_get_som_power_consumption() → Failure (returns non-zero)
2. Immediately fall back to INA226 sensor readings
3. Use BMC-side measurements instead of SOM-side
```

**Fallback Path #2 (Protocol Succeeds but Data Invalid)**:
```
1. protocol_get_som_power_consumption() → Success (returns 0)
2. Check if data is all zeros → True (SOM returned zeros)
3. Fall back to INA226 sensor readings
4. This handles cases where SOM responds but hasn't initialized sensors yet
```

### Unit Conversions

**Current Conversion**:
```c
current_val = somPowerConsumption.current_12v / 1000;
```
- SOM reports current in **microamperes (µA)**
- BMC stores current in **milliamperes (mA)**
- Division by 1000: µA → mA

**Power Calculation**:
```c
power_val = voltage_val * current_val / 1000;
```
- voltage_val in **millivolts (mV)**
- current_val in **milliamperes (mA)**
- Result: (mV * mA) / 1000 = **milliwatts (mW)**

**5V Power Calculation**:
```c
power_consumption_5v_val = somPowerConsumption.voltage_5v *
                           somPowerConsumption.current_5v / 1000000;
```
- voltage_5v in **mV**
- current_5v in **µA**
- Division by 1,000,000: (mV * µA) / 1000000 = mW

### Why Zero Detection?

The zero-check condition serves two purposes:

1. **Uninitialized SOM State**: When SOM first powers on, its power monitoring subsystem may not have valid readings yet. It responds to the protocol but returns all zeros.

2. **Communication Corruption**: If the protocol response is corrupted or partially received, fields may be zero-filled by the parser.

3. **SOM Boot Race**: During power-on sequence, there's a window where SOM daemon responds but sensors aren't ready.

The fix ensures we distinguish between:
- **Protocol-level failure**: Can't communicate with SOM at all → use INA226
- **Data-level failure**: SOM responds but data invalid → use INA226
- **Success**: SOM responds with valid data → use SOM values

## Impact Assessment

### What This Fixes

**Before Patch**:
- **Scenario A**: SOM powered off, protocol fails, `somPowerConsumption` contains random stack data
  - If random data happened to be non-zero → **incorrect power readings displayed**
  - Only fell back correctly if random data was all zeros (unlikely)

- **Scenario B**: SOM powered on but sensors not ready, returns all zeros
  - Correctly fell back to INA226 (**but for wrong reason**)

- **Scenario C**: SOM powered on and responding normally
  - Correctly used SOM data (**worked by accident**)

**After Patch**:
- **Scenario A**: SOM powered off → **always** uses INA226 (correct)
- **Scenario B**: SOM returns zeros → **always** uses INA226 (correct)
- **Scenario C**: SOM returns valid data → uses SOM data (correct)

### User-Visible Changes

**Web Interface (`/api/sensors`)**:
- Power consumption readings now always valid (no random/stale data)
- Consistent behavior when SOM is powered off
- Smooth transition during SOM power-on (uses INA226 until SOM ready)

**CLI Console (`sensors` command)**:
- Accurate voltage/current/power display
- Proper fallback indication when SOM unavailable

**Monitoring & Logging**:
- Power consumption graphs show correct data
- No spurious spikes from stale data
- Reliable for production monitoring

### Reliability Improvement

This patch significantly improves the **robustness** of power monitoring:

1. **Fail-Safe Default**: Always falls back to INA226 when in doubt
2. **No Undefined Behavior**: Never uses uninitialized data
3. **Predictable**: Logic flow is clear and testable
4. **Defensive**: Multiple validation layers (protocol success, data sanity)

## Files Modified

### `Core/Src/bmc_daemon.c`

**Function**: `bmc_daemon_task()`

**Changes**:
- Restructured power consumption retrieval logic
- Added explicit protocol failure check (`!= 0`)
- Nested data validation inside success path
- Clearer separation of failure modes

**Lines Changed**: ~35 lines restructured (complex diff due to indentation changes)

## Code Review Notes

### Positive Aspects

1. **Bug Fix Quality**: Addresses the actual root cause (using uninitialized data)
2. **Defensive Programming**: Multiple validation layers prevent bad data propagation
3. **Clear Intent**: Nested conditionals make logic flow obvious
4. **No Side Effects**: Only changes conditional logic, not data structures or interfaces

### Potential Concerns

1. **Increased Nesting**: Three levels of `if/else` - could be refactored with early returns
2. **Code Duplication**: Fallback assignment (`voltage_val = ina226_voltage; ...`) appears twice
3. **No Logging**: Silent fallback - doesn't log when protocol fails or data is invalid
4. **Magic Number**: Division by 1000000 for 5V power - should be named constant

### Suggested Improvements (Future Patches)

```c
#define UA_TO_MA_DIVISOR 1000
#define MV_UA_TO_MW_DIVISOR 1000000

static void use_ina226_fallback(void) {
    voltage_val = ina226_voltage;
    current_val = ina226_current;
    power_val = ina226_power;
    printf("[DAEMON] Using INA226 fallback\n");
}

// In bmc_daemon_task():
if(protocol_get_som_power_consumption(&somPowerConsumption) != 0) {
    bmc_debug("SOM power consumption protocol failed\n");
    use_ina226_fallback();
} else if(is_som_power_data_invalid(&somPowerConsumption)) {
    bmc_debug("SOM power data invalid (all zeros)\n");
    use_ina226_fallback();
} else {
    // Use valid SOM data
    voltage_val = somPowerConsumption.voltage_12v;
    current_val = somPowerConsumption.current_12v / UA_TO_MA_DIVISOR;
    // ... etc
}
```

## Testing Recommendations

### Test Case 1: SOM Powered Off
**Setup**: Power off SOM via web interface
**Expected**: Power readings show INA226 values (BMC-side measurements)
**Verification**: Check `/api/sensors` returns non-zero, stable values

### Test Case 2: SOM Power-On Sequence
**Setup**: Power on SOM, monitor readings continuously
**Expected**:
1. During power-on: INA226 values (SOM not ready yet)
2. After SOM boot: Transition to SOM values (may be higher due to more accurate measurement)
**Verification**: No zero values, no random spikes

### Test Case 3: SOM Communication Failure
**Setup**: Disconnect SPI cable between BMC and SOM (if accessible in test)
**Expected**: Immediate fallback to INA226, stable readings
**Verification**: Power consumption values remain valid

### Test Case 4: Data Validation
**Setup**: Inject simulated protocol response with all zeros
**Expected**: BMC detects invalid data, falls back to INA226
**Verification**: Check debug logs (if added)

## Dependencies

### Depends On
- **Patch 0030**: INA226 sensor integration (provides fallback readings)
- **Patch 0049**: Protocol library `protocol_get_som_power_consumption()` function
- **Patch 0068**: BMC daemon structure and SOM communication

### Required By
- Patch 0092 (follows immediately after with related fix)
- Any future power monitoring enhancements

## Related Issues

- **WIN2030-15279**: Main tracking issue for BMC improvements
- **Power Consumption Accuracy**: Ensures web interface shows correct values
- **SOM Integration**: Proper handling of SOM communication lifecycle

## Version Compatibility

- **Introduced**: BMC version 2.2.x
- **Compatible**: All BMC 2.x versions with SOM communication protocol
- **Hardware**: All HF106 carrier board revisions (DVT, DVB2, production)

## Security Considerations

**Information Disclosure**:
- Previous behavior could leak uninitialized stack memory contents through power readings
- If `somPowerConsumption` structure contained sensitive data from previous function calls, zeros-but-protocol-succeeded scenario could expose it
- Patch fixes this by always validating protocol success before using data

**Denial of Service**:
- Invalid power readings could trigger false alerts in monitoring systems
- Corrected data prevents spurious alerts

## Author's Notes

**From commit message**:
> Changelogs:
> 1. Bug fix for get_som_power_consumption

The commit message is terse, but the code change is comprehensive. The author correctly identified the logic flaw and restructured the conditional flow to be defensive and explicit.

## See Also

- **Patch 0092**: Follow-up fix for related power consumption issue
- **Patch 0030**: INA226 sensor integration (provides fallback mechanism)
- **Patch 0068**: BMC daemon communication with SOM
- **`bmc_daemon.c` Architecture**: Main monitoring loop and sensor aggregation
