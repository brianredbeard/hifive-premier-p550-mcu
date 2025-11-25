# Patch 0092: Power Consumption Bug Fix - Get SOM Power Consumption (Part 2)

## Metadata
- **Patch File**: `0092-WIN2030-15279-fix-power-consumption-Bug-fix-for-get_.patch`
- **Title**: WIN2030-15279:fix(power consumption):Bug fix for get som power consumption
- **Author**: linmin <linmin@eswincomputing.com>
- **Date**: Mon, 28 Oct 2024 18:10:48 +0800
- **Category**: Bug Fix
- **Subsystem**: Power Management / Daemon
- **Change-Id**: I3c2e67bb87fb0a25f70e0d8e3f5fe32c66ba5a10

## Description

### Problem Statement
Immediately following patch 0091, this patch addresses a **critical bug in the zero-detection condition** for SOM power consumption data. The previous zero-check logic was incomplete and could cause incorrect fallback behavior.

### Root Cause Analysis

**Buggy Code from Patch 0091**:
```c
if(somPowerConsumption.voltage_5v == 0
    && somPowerConsumption.current_5v == 0
    && somPowerConsumption.voltage_12v == 0
    && somPowerConsumption.current_12v == 0) {
    /* Invalid - fall back to INA226 */
    voltage_val = ina226_voltage;
    current_val = ina226_current;
    power_val = ina226_power;
}
```

**The Problem**: This condition is **too strict**. It only detects when **ALL FOUR** fields are zero simultaneously.

**Real-World Scenarios Missed**:
1. **5V Rail Unpopulated**: Some board configurations don't have 5V power monitoring
   - `voltage_5v = 0`, `current_5v = 0` (expected)
   - `voltage_12v = valid`, `current_12v = valid`
   - **Result**: Condition false, uses invalid partial data

2. **Partial Sensor Failure**: If only 12V rail sensors fail
   - `voltage_12v = 0`, `current_12v = 0`
   - `voltage_5v = valid`, `current_5v = valid`
   - **Result**: Condition false, uses invalid 12V data

3. **Initialization Race**: Sensors initialize at different rates
   - Some rails ready, others still at zero
   - **Result**: Mix of valid and invalid data used

### Solution Implementation

**Fixed Code**:
```c
if(somPowerConsumption.voltage_12v == 0
    || somPowerConsumption.current_12v == 0) {
    /* 12V rail invalid - fall back to INA226 */
    voltage_val = ina226_voltage;
    current_val = ina226_current;
    power_val = ina226_power;
}
```

**Key Changes**:
1. **Removed 5V Checks**: Only validate the 12V rail that BMC actually uses for primary monitoring
2. **Changed AND to OR**: If **either** voltage_12v **or** current_12v is zero, data is invalid
3. **Focused Validation**: Only checks the fields that will actually be used

**Why This Works**:
- BMC primarily monitors the **12V rail** for system power consumption
- 5V rail data is supplementary (used for `power_consumption_5v_val` tracking)
- If 12V data is invalid (either field zero), can't calculate meaningful power
- Even if 5V data is valid, 12V is critical path → safer to fall back entirely

### Validation Logic Rationale

**Why OR instead of AND?**

For power calculation `P = V × I`, **both** voltage and current must be valid:
- If voltage is zero but current is non-zero → `P = 0 × I = 0` (meaningless)
- If current is zero but voltage is non-zero → `P = V × 0 = 0` (meaningless)
- Zero in either field indicates sensor failure or uninitialized state

**Why ignore 5V rail entirely?**

The 5V rail validation is **removed** because:
1. Not all hardware configurations populate 5V sensors
2. 5V data is **supplementary** - used for detailed breakdowns but not critical path
3. If 5V sensors absent/failed, still want to use valid 12V data
4. Separating 5V and 12V validation avoids false fallbacks

## Technical Analysis

### Data Structure Layout

**SomPowerConsumption Structure** (from protocol library):
```c
typedef struct {
    uint32_t voltage_5v;    // 5V rail voltage in mV
    uint32_t current_5v;    // 5V rail current in µA
    uint32_t voltage_12v;   // 12V rail voltage in mV (PRIMARY)
    uint32_t current_12v;   // 12V rail current in µA (PRIMARY)
} SomPowerConsumption;
```

**Critical vs. Supplementary**:
- **Critical (12V)**: Used for primary system power monitoring
  - `voltage_val`, `current_val`, `power_val` calculated from these
  - Displayed prominently in web interface
  - Used for alerts and thresholds

- **Supplementary (5V)**: Used for detailed power breakdown
  - `power_consumption_5v_val` calculated separately
  - Useful for debugging, less critical for monitoring

### Decision Tree After Patch

```
protocol_get_som_power_consumption()
  ├─ Returns != 0 (Failure)
  │   └─> Use INA226 fallback
  │
  └─ Returns 0 (Success)
      ├─ voltage_12v == 0?
      │   └─ YES: Use INA226 fallback
      │
      └─ current_12v == 0?
          ├─ YES: Use INA226 fallback
          │
          └─ NO: Use SOM data (both fields valid)
              ├─ Calculate primary values from 12V rail
              └─ Calculate supplementary 5V values (may be zero if not populated)
```

**Key Insight**: 5V values can be zero (unpopulated) without triggering fallback, as long as 12V data is valid.

## Impact Assessment

### What This Fixes

**Scenario 1: Board Without 5V Monitoring**
- **Before**: Would use INA226 fallback even if 12V data was valid
- **After**: Uses SOM 12V data, ignores missing 5V data
- **Benefit**: More accurate power measurements on boards with only 12V sensors

**Scenario 2: Partial Sensor Initialization**
- **Before**: If 12V initialized but 5V hadn't yet, would fall back unnecessarily
- **After**: Uses valid 12V data immediately, doesn't wait for 5V
- **Benefit**: Faster valid readings after SOM boot

**Scenario 3: 12V Sensor Failure**
- **Before**: If only one of voltage_12v/current_12v was zero, might use invalid data
- **After**: Detects either field zero, falls back properly
- **Benefit**: More robust failure detection

### User-Visible Changes

**Web Interface**:
- More consistent power readings on hardware variants
- Faster transition to valid data after SOM power-on
- Correctly handles boards with partial sensor population

**Reliability**:
- Fewer false fallbacks to INA226
- More accurate power consumption when SOM data available
- Proper handling of mixed valid/invalid sensor states

## Files Modified

### `Core/Src/bmc_daemon.c`

**Function**: `bmc_daemon_task()`

**Specific Change**:
```diff
-if(somPowerConsumption.voltage_5v == 0
-    && somPowerConsumption.current_5v == 0
-    && somPowerConsumption.voltage_12v == 0
-    && somPowerConsumption.current_12v == 0) {
+if(somPowerConsumption.voltage_12v == 0
+    || somPowerConsumption.current_12v == 0) {
```

**Impact**: 4-way AND condition → 2-way OR condition
**Lines Changed**: 4 lines removed, 2 lines added

## Code Review Notes

### Positive Aspects

1. **Focused Validation**: Only checks fields actually used for critical calculations
2. **Hardware Flexibility**: Allows partial sensor population (5V optional)
3. **Logical Correctness**: OR condition matches power calculation requirements (need both V and I)
4. **Simpler Logic**: Reduced from 4 conditions to 2 (easier to understand and test)

### Design Decisions

**Why Not Keep 5V Validation?**

If we kept separate 5V validation:
```c
if((somPowerConsumption.voltage_12v == 0 || somPowerConsumption.current_12v == 0) ||
   (somPowerConsumption.voltage_5v == 0 || somPowerConsumption.current_5v == 0)) {
    // Fallback
}
```

**Problem**: Boards without 5V sensors would **always** fall back, defeating the purpose of SOM data.

**Better Approach** (current implementation):
```c
// Validate 12V (critical path)
if(somPowerConsumption.voltage_12v == 0 || somPowerConsumption.current_12v == 0) {
    use_ina226_fallback();
} else {
    // Use 12V data (always valid here)
    voltage_val = somPowerConsumption.voltage_12v;
    current_val = somPowerConsumption.current_12v / 1000;
    power_val = voltage_val * current_val / 1000;

    // Use 5V data if available (may be zero on some boards)
    if(somPowerConsumption.voltage_5v != 0 && somPowerConsumption.current_5v != 0) {
        power_consumption_5v_val = somPowerConsumption.voltage_5v *
                                   somPowerConsumption.current_5v / 1000000;
    } else {
        power_consumption_5v_val = 0; // Not available on this hardware
    }
}
```

**Note**: The current patch doesn't include the optional 5V validation above, but that could be a future enhancement.

### Potential Issues

1. **5V Data Unused**: Patch doesn't show what happens to 5V data when it's valid
   - Likely still calculated in the "else" block (not shown in diff)
   - May want explicit handling for 5V absent case

2. **No Logging**: Silent fallback, can't distinguish between:
   - Protocol failure
   - 12V data invalid
   - Would be helpful to log why fallback occurred

## Testing Recommendations

### Test Case 1: Standard Board (Both 12V and 5V Populated)
**Setup**: HF106C carrier board with all sensors
**Expected**: Uses SOM data for both rails
**Verification**: Check `voltage_val` matches SOM 12V, `power_consumption_5v_val` non-zero

### Test Case 2: 12V-Only Board
**Setup**: Hardware variant without 5V monitoring
**Expected**:
- Uses SOM 12V data
- `power_consumption_5v_val = 0` or not calculated
- No fallback to INA226
**Verification**: `voltage_val` from SOM, not INA226

### Test Case 3: 12V Sensor Failure Simulation
**Setup**: Mock `protocol_get_som_power_consumption()` to return:
- `voltage_12v = 0`, `current_12v = 5000` (voltage sensor failed)
**Expected**: Falls back to INA226
**Verification**: `voltage_val == ina226_voltage`

### Test Case 4: Partial Initialization
**Setup**: SOM boot sequence, sensors come online at different times
**Expected**:
- Uses INA226 while 12V data is zero
- Switches to SOM data once 12V valid (even if 5V still zero)
**Verification**: Monitor `/api/sensors` during SOM boot, observe transition

## Dependencies

### Depends On
- **Patch 0091**: Introduced the zero-check logic (which this patch corrects)
- **Patch 0030**: INA226 fallback mechanism
- **Patch 0049**: Protocol library for SOM communication

### Required By
- Future power monitoring enhancements
- Multi-rail power monitoring features

## Related Issues

- **WIN2030-15279**: Main BMC improvement tracking issue
- **Hardware Variants**: Supports different sensor configurations
- **Power Monitoring Accuracy**: Critical for production use

## Version Compatibility

- **Introduced**: BMC version 2.2.x (immediately after patch 0091)
- **Compatible**: All BMC 2.x versions
- **Hardware**: Essential for board variants with partial sensor population

## Author's Notes

**From commit message**:
> Changelogs:
> 1. Bug fix for get_som_power_consumption

This patch is a **critical follow-up** to patch 0091, fixing a logic error introduced in that patch. The author likely discovered this issue during testing of patch 0091 on hardware with missing 5V sensors.

**Lesson**: When implementing validation logic, consider:
1. Partial sensor population
2. Hardware variants
3. Which fields are critical vs. supplementary
4. AND vs. OR semantics for multi-field validation

## Comparison: Patch 0091 vs 0092

| Aspect | Patch 0091 | Patch 0092 |
|--------|-----------|-----------|
| **Focus** | Protocol success validation | Data content validation |
| **Logic Change** | Added `if(protocol != 0)` check | Changed zero-check from AND to OR |
| **Fields Validated** | N/A (protocol level) | 4 fields → 2 fields (12V only) |
| **Hardware Support** | All boards | Essential for variant boards |
| **Impact** | Prevents uninitialized data use | Prevents false fallbacks |

**Together**: These two patches form a **complete validation strategy**:
1. **Patch 0091**: Is the communication working? (protocol level)
2. **Patch 0092**: Is the critical data valid? (content level)

## See Also

- **Patch 0091**: Companion fix for protocol validation
- **Patch 0030**: INA226 sensor (fallback mechanism)
- **`bmc_daemon.c` Architecture**: Power monitoring loop
- **Protocol Library**: `protocol_get_som_power_consumption()` interface
