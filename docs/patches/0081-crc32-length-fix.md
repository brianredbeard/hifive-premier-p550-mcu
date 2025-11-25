# Patch 0081: CRC32 Length Calculation Fix

## Metadata
- **Patch Number**: 0081
- **Ticket**: WIN2030-15279
- **Type**: fix
- **Component**: hf_common
- **Author**: linmin <linmin@eswincomputing.com>
- **Date**: Sat, 8 Jun 2024 16:48:07 +0800
- **Files Modified**: 1
  - `Core/Src/hf_common.c`

## Summary

This patch corrects a critical bug in the CRC32 checksum calculation for the `CarrierBoardInfo` structure. The length parameter passed to the `sifive_crc32()` function was incorrect, using `sizeof(CarrierBoardInfo)/4 - 1` instead of the correct `sizeof(CarrierBoardInfo) - 4`.

## Problem Description

### Root Cause

The CRC32 calculation was using an incorrect length formula:
- **Incorrect**: `sizeof(CarrierBoardInfo)/4 - 1`
- **Correct**: `sizeof(CarrierBoardInfo) - 4`

The CRC32 checksum field itself is 4 bytes (uint32_t) and is stored at the end of the structure. The CRC should be calculated over all bytes of the structure EXCEPT the 4-byte checksum field itself.

### Why This Matters

**Mathematical Analysis**:
- If `sizeof(CarrierBoardInfo)` is, for example, 128 bytes:
  - Incorrect formula: `128/4 - 1 = 32 - 1 = 31 bytes`
  - Correct formula: `128 - 4 = 124 bytes`

The incorrect calculation was computing the CRC over only **31 bytes** when it should have been computing over **124 bytes** - missing most of the structure!

**Consequences**:
1. **False Positives**: CRC validation would pass even with corrupted data in bytes beyond position 31
2. **Data Integrity Failure**: The primary purpose of CRC32 (detecting EEPROM corruption) was completely compromised
3. **Silent Data Corruption**: Invalid carrier board information could be loaded without detection
4. **Backup Partition Issues**: Backup/main partition validation was unreliable

### Affected Operations

This bug affected all carrier board info operations:
1. `restore_cbinfo_to_factory()` - Factory restore CRC calculation
2. `get_carrier_board_info()` - Main partition CRC validation
3. `get_carrier_board_info()` - Backup partition CRC validation
4. `get_carrier_board_info()` - Backup partition CRC check
5. `es_set_carrier_borad_info()` - Update CRC when modifying info
6. `es_set_mcu_mac()` - Update CRC when changing MAC address

## Technical Details

### Code Changes

**Location**: `Core/Src/hf_common.c`

Six instances of the incorrect CRC length calculation were fixed:

#### 1. Factory Restore Function (Line 255)
```c
// BEFORE:
pCarrier_Board_Info->crc32Checksum = sifive_crc32((uint8_t *)pCarrier_Board_Info, sizeof(CarrierBoardInfo)/4 - 1);

// AFTER:
pCarrier_Board_Info->crc32Checksum = sifive_crc32((uint8_t *)pCarrier_Board_Info, sizeof(CarrierBoardInfo) - 4);
```

#### 2. Main Partition Validation (Line 313)
```c
// BEFORE:
crc32Checksum = sifive_crc32((uint8_t *)&gCarrier_Board_Info, sizeof(CarrierBoardInfo)/4 - 1);

// AFTER:
crc32Checksum = sifive_crc32((uint8_t *)&gCarrier_Board_Info, sizeof(CarrierBoardInfo) - 4);
```

#### 3. Backup Partition Validation (Line 324)
```c
// BEFORE:
crc32Checksum = sifive_crc32((uint8_t *)&gCarrier_Board_Info, sizeof(CarrierBoardInfo)/4 - 1);

// AFTER:
crc32Checksum = sifive_crc32((uint8_t *)&gCarrier_Board_Info, sizeof(CarrierBoardInfo) - 4);
```

#### 4. Backup Partition CRC Check (Line 344)
```c
// BEFORE:
crc32Checksum = sifive_crc32((uint8_t *)&CbinfoBackup, sizeof(CarrierBoardInfo)/4 - 1);

// AFTER:
crc32Checksum = sifive_crc32((uint8_t *)&CbinfoBackup, sizeof(CarrierBoardInfo) - 4);
```

#### 5. Set Carrier Board Info (Line 555)
```c
// BEFORE:
pCarrier_Board_Info->crc32Checksum = sifive_crc32((uint8_t *)pCarrier_Board_Info, sizeof(CarrierBoardInfo)/4 - 1);

// AFTER:
pCarrier_Board_Info->crc32Checksum = sifive_crc32((uint8_t *)pCarrier_Board_Info, sizeof(CarrierBoardInfo) - 4);
```

#### 6. Set MCU MAC Address (Line 648)
```c
// BEFORE:
gCarrier_Board_Info.crc32Checksum = sifive_crc32((uint8_t *)&gCarrier_Board_Info, sizeof(CarrierBoardInfo)/4 - 1);

// AFTER:
gCarrier_Board_Info.crc32Checksum = sifive_crc32((uint8_t *)&gCarrier_Board_Info, sizeof(CarrierBoardInfo) - 4);
```

### SiFive CRC32 Algorithm

**Reference**: The `sifive_crc32()` function implements the standard CRC-32 algorithm compatible with SiFive's implementation.

**Function Signature**:
```c
uint32_t sifive_crc32(const uint8_t *data, uint32_t length);
```

**Parameters**:
- `data`: Pointer to the data buffer to calculate CRC over
- `length`: **Number of BYTES** to include in CRC calculation (NOT word count!)

**Returns**: 32-bit CRC checksum value

**Key Point**: The `length` parameter is in **bytes**, not words. This is why the correct formula is `sizeof(struct) - 4` (subtract 4 bytes for the checksum field), NOT `sizeof(struct)/4 - 1` (which would be word-based arithmetic).

### CarrierBoardInfo Structure

**Definition** (from `Core/Inc/hf_common.h`):
```c
typedef struct {
    uint32_t magicNumber;
    uint8_t carrierSN[CARRIER_BOARD_SN_LEN];
    uint8_t ethernetMAC1[ETH_ADDR_LEN];
    uint8_t ethernetMAC2[ETH_ADDR_LEN];
    uint8_t ethernetMAC3[ETH_ADDR_LEN];
    // ... additional fields ...
    uint32_t crc32Checksum;  // LAST FIELD - 4 bytes
} CarrierBoardInfo;
```

**Structure Layout**:
- Total size: `sizeof(CarrierBoardInfo)` bytes
- CRC32 checksum field: Last 4 bytes of structure
- Data to CRC: Everything EXCEPT last 4 bytes = `sizeof(CarrierBoardInfo) - 4`

**CRC Calculation Process**:
1. Calculate CRC over bytes `[0, sizeof(struct) - 4)`
2. Store result in `structure.crc32Checksum` field
3. Write entire structure (including CRC) to EEPROM

**CRC Validation Process**:
1. Read entire structure from EEPROM
2. Extract stored CRC from `structure.crc32Checksum`
3. Calculate CRC over bytes `[0, sizeof(struct) - 4)`
4. Compare calculated CRC with stored CRC
5. If match: data is valid; if mismatch: corruption detected

## Impact Analysis

### Before Fix (Buggy Behavior)

**Scenario**: `sizeof(CarrierBoardInfo)` = 128 bytes

1. **CRC Calculation**: Only covered first 31 bytes
2. **Vulnerable Bytes**: Bytes 31-123 NOT protected by CRC
3. **False Security**: System appeared to have integrity checking but didn't
4. **Corruption Risk**: MAC addresses, serial numbers, or other data beyond byte 31 could be corrupted silently

### After Fix (Correct Behavior)

1. **CRC Calculation**: Covers all 124 bytes (full structure minus 4-byte CRC)
2. **Full Protection**: All fields protected by integrity check
3. **Reliable Detection**: EEPROM corruption properly detected
4. **Data Integrity**: System can trust carrier board info or fallback to defaults

### Security Implications

**Data Integrity Compromise**:
- MAC addresses could be corrupted without detection
- Serial numbers could be modified without detection
- Network configuration stored in carrier info could be tampered with
- Factory calibration data potentially corrupted silently

**Production Impact**:
- Units in field with corrupted EEPROM data may appear to function normally
- Intermittent network issues due to invalid MAC addresses
- Difficult-to-diagnose problems from partial data corruption

**Regulatory Compliance**:
- MAC address integrity critical for regulatory compliance (FCC, CE)
- Traceability compromised if serial numbers can be silently corrupted

## EEPROM Data Flow

### Carrier Board Info Storage

**EEPROM Layout**:
```
CARRIER_BOARD_INFO_EEPROM_MAIN_OFFSET:   Main partition
CARRIER_BOARD_INFO_EEPROM_BACKUP_OFFSET: Backup partition
```

**Dual-Partition Strategy**:
- Main and backup partitions for redundancy
- If main partition CRC fails → try backup partition
- If backup also fails → restore to factory defaults

### Factory Restore Process

**Trigger Conditions** (all must fail):
1. Main partition CRC validation fails
2. Backup partition CRC validation fails

**Factory Restore Steps**:
1. Call `restore_cbinfo_to_factory()`
2. Populate structure with factory defaults (MAC addresses, serial numbers)
3. Calculate CRC32 over structure (now correctly over 124 bytes)
4. Write to both main and backup partitions

### Read/Validate Process

**Boot-time Sequence** (`get_carrier_board_info()`):
1. Read main partition from EEPROM → `gCarrier_Board_Info`
2. Calculate CRC over read data (124 bytes)
3. Compare with stored CRC in structure
4. **If main CRC valid**: Use main partition data, verify backup CRC
   - If backup CRC invalid: Recover backup from main
5. **If main CRC invalid**: Try backup partition
   - Read backup partition
   - Calculate/validate backup CRC
   - If backup valid: Recover main from backup
   - If backup invalid: Factory restore

**This fix ensures step 2 calculates CRC over the FULL 124 bytes, not just 31 bytes!**

## Testing Recommendations

### Verification Steps

1. **New EEPROM Writes**:
   - Modify MAC address via web interface
   - Verify CRC is recalculated correctly
   - Read back from EEPROM, verify CRC validates

2. **Corruption Detection**:
   - Manually corrupt byte 50 in EEPROM (beyond old 31-byte coverage)
   - Reboot BMC
   - **Expected**: CRC validation now FAILS (previously would pass)
   - **Expected**: Fallback to backup or factory restore

3. **Partition Redundancy**:
   - Corrupt main partition
   - Verify backup partition is used
   - Verify main partition is recovered from backup

4. **Factory Restore**:
   - Corrupt both partitions
   - Verify factory restore triggered
   - Verify CRC of restored data is valid

### Regression Testing

**Existing Units in Field**:
- Units with firmware prior to this patch have incorrect CRCs stored in EEPROM
- Upon upgrade to firmware with this fix:
  - **Expected**: CRC validation will FAIL (old CRC was calculated incorrectly)
  - **Expected**: System will restore to factory defaults or backup
  - **Impact**: Production systems will lose stored configuration on first boot after upgrade!

**Mitigation Strategy**:
1. Backup carrier board info before firmware update
2. After update, system will restore to factory defaults
3. Re-enter configuration (or restore from backup)

OR:

1. Add migration logic to recalculate all CRCs on first boot after upgrade
2. Detect old firmware version, trigger CRC recalculation

## Related Code

### SiFive CRC32 Implementation

**Reference**: Patch 0079 introduced `sifive_crc32()` function

**Algorithm**: Standard CRC-32 (polynomial 0x04C11DB7)

```c
uint32_t sifive_crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;  // Initial value

    for (uint32_t i = 0; i < length; i++) {  // Iterate over LENGTH BYTES
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;  // Reflected polynomial
            } else {
                crc = crc >> 1;
            }
        }
    }

    return ~crc;  // Final XOR
}
```

**Key Point**: The `length` parameter is the byte count, NOT word count.

### Data Structures Using CRC32

**From this patch**:
- `CarrierBoardInfo` - Fixed in this patch

**Other structures** (should be verified):
- `MCUServerInfo` - Uses CRC32 (added in patch 0083)
- Any other EEPROM-stored structures with checksums

## Best Practices

### Correct CRC Length Calculation

**Pattern for structures with CRC**:
```c
typedef struct {
    // ... data fields ...
    uint32_t crc32Checksum;  // Always last field, 4 bytes
} DataStructure;

// CORRECT CRC calculation:
uint32_t crc = sifive_crc32((uint8_t *)&data, sizeof(DataStructure) - 4);

// INCORRECT (word-based arithmetic):
uint32_t crc = sifive_crc32((uint8_t *)&data, sizeof(DataStructure)/4 - 1);
```

**Rule**: Always use `sizeof(struct) - sizeof(checksum_field)` for CRC length.

### Structure Design Guidelines

1. **CRC Field Last**: Always place checksum/CRC as last field in structure
2. **Fixed Size**: Use fixed-size types (uint8_t arrays, not char*)
3. **No Padding**: Use `__attribute__((packed))` if necessary
4. **Document Layout**: Comment structure with byte offsets

### EEPROM Integrity Best Practices

1. **Dual Partitions**: Main + backup for critical data
2. **Magic Numbers**: Use magic number to detect uninitialized EEPROM
3. **Version Field**: Include version for future migration
4. **Factory Defaults**: Always have valid fallback values
5. **Write Verification**: Read back after write to verify
6. **CRC on Write**: Calculate CRC immediately before writing
7. **CRC on Read**: Validate CRC immediately after reading

## Changelog Entry

```
Changelogs:
1. The length parameter of sifive_crc32 passed in was incorrect.
```

## Related Patches

- **Patch 0079**: Introduced SiFive CRC32 algorithm
- **Patch 0083**: Added CRC32 checksum for MCUServerInfo (applied same fix pattern)
- **Patch 0080**: EEPROM write protection (ensures integrity of writes)
- **Patch 0097**: Additional checksum verification improvements

## Files Modified

```
Core/Src/hf_common.c | 12 ++++++------
1 file changed, 6 insertions(+), 6 deletions(-)
```

**Diff Summary**: 6 replacements (all instances of incorrect CRC length formula)

## Conclusion

This patch fixes a critical data integrity bug in the CRC32 calculation for carrier board information stored in EEPROM. The incorrect length calculation left most of the structure unprotected by the CRC, defeating the purpose of integrity checking. The fix ensures the full structure (minus the CRC field itself) is protected, enabling reliable detection of EEPROM corruption.

**Criticality**: HIGH - Data integrity fundamental to system operation
**Impact**: Positive - Enables reliable EEPROM corruption detection
**Risk**: Migration concern - Existing units will trigger CRC failures on upgrade
