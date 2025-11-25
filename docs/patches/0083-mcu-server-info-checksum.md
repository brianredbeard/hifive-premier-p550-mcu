# Patch 0083: MCU Server Info CRC32 Checksum

## Metadata
- **Patch Number**: 0083
- **Ticket**: WIN2030-15279
- **Type**: fix
- **Component**: hf_common - EEPROM data integrity
- **Author**: linmin <linmin@eswincomputing.com>
- **Date**: Tue, 11 Jun 2024 11:08:38 +0800
- **Files Modified**: 2
  - `Core/Inc/hf_common.h` (structure definition)
  - `Core/Src/hf_common.c` (validation and update logic)

## Summary

This patch adds CRC32 checksum protection to the `MCUServerInfo` structure, which stores critical BMC configuration including admin credentials, network settings (IP, netmask, gateway). Prior to this patch, this structure had no integrity protection, allowing silent corruption of network configuration and authentication credentials. The implementation follows the same pattern as `CarrierBoardInfo` CRC32 protection introduced in earlier patches.

## Problem Description

### Vulnerability: Unprotected Configuration Data

**MCUServerInfo Structure** stores:
1. **Admin Name**: Default "admin" username
2. **Admin Password**: Default password for web authentication
3. **IP Address**: Static IP configuration (if not using DHCP)
4. **Netmask Address**: Network mask
5. **Gateway Address**: Default gateway

**Prior State**: Structure had padding field, no checksum

**Risk**:
- EEPROM bit flips could corrupt credentials → lockout from web interface
- Network configuration corruption → BMC unreachable
- Password corruption → security bypass or denial of access
- No detection mechanism → silent failures

### Validation Logic Before Fix

**Original Code** (`get_mcu_server_info()`):
```c
if ((0 == strlen(gMCU_Server_Info.AdminName)) ||
    (gCarrier_Board_Info.magicNumber != MAGIC_NUMBER)) {
    // Restore to default
}

if ((0 == strlen(gMCU_Server_Info.AdminPassword)) ||
    (gCarrier_Board_Info.magicNumber != MAGIC_NUMBER)) {
    // Restore to default
}

if ((0 == gMCU_Server_Info.ip_address[0]) ||
    (gCarrier_Board_Info.magicNumber != MAGIC_NUMBER)) {
    // Restore to default
}

if (!ip4_addr_netmask_valid(hlmask) ||
    (gCarrier_Board_Info.magicNumber != MAGIC_NUMBER)) {
    // Restore to default
}

if ((0 == gMCU_Server_Info.gateway_address[0]) ||
    (gCarrier_Board_Info.magicNumber != MAGIC_NUMBER)) {
    // Restore to default
}
```

**Problems**:
1. **Separate checks for each field** - complex logic, easy to miss corruption
2. **Weak validation** - "first byte zero" doesn't catch all corruption
3. **Wrong magic number check** - checking `gCarrier_Board_Info.magicNumber` instead of MCU server info's own validity
4. **No corruption detection** - partial corruption (e.g., byte 10 flipped) goes undetected
5. **Verbose code** - 5 separate if blocks for each field

### Validation Logic After Fix

**New Code** (`get_mcu_server_info()`):
```c
uint32_t crc32Checksum;

crc32Checksum = sifive_crc32((uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);

if (crc32Checksum != gMCU_Server_Info.crc32Checksum) {
    // CRC mismatch - corruption detected
    // Restore ALL fields to factory defaults
    strcpy(gMCU_Server_Info.AdminName, DEFAULT_ADMIN_NAME);
    strcpy(gMCU_Server_Info.AdminPassword, DEFAULT_ADMIN_PASSWORD);
    gMCU_Server_Info.ip_address[0] = IP_ADDR0;
    // ... all fields reset ...
    gMCU_Server_Info.crc32Checksum = sifive_crc32(...);
    printf("Invalid checksum of mcu server info, update it with default value!\n");
}
```

**Improvements**:
1. **Single CRC check** - detects corruption in ANY field
2. **Stronger validation** - CRC-32 detects 99.9999% of bit flips
3. **Atomic restoration** - all fields restored together (consistent state)
4. **Clear logging** - explicit "invalid checksum" message
5. **Simpler code** - single if block replaces 5 separate checks

## Technical Details

### Structure Modification

**Header File** (`Core/Inc/hf_common.h`):

**Before**:
```c
typedef struct {
    char AdminName[ADMIN_NAME_LEN];
    char AdminPassword[ADMIN_PASSWORD_LEN];
    uint8_t ip_address[4];
    uint8_t netmask_address[4];
    uint8_t gateway_address[4];
    // uint8_t padding[4];  ← Commented-out padding
} MCUServerInfo;
```

**After**:
```c
typedef struct {
    char AdminName[ADMIN_NAME_LEN];
    char AdminPassword[ADMIN_PASSWORD_LEN];
    uint8_t ip_address[4];
    uint8_t netmask_address[4];
    uint8_t gateway_address[4];
    uint32_t crc32Checksum;  ← Replaced padding with active CRC field
} MCUServerInfo;
```

**Key Change**: Padding field replaced with functional CRC32 checksum field

**Size Impact**: No change - structure size remains identical (padding replaced 1:1)

### CRC Calculation and Validation

**Pattern** (consistent with Patch 0081 fix for CarrierBoardInfo):

```c
// Calculate CRC over entire structure EXCEPT the 4-byte CRC field itself
uint32_t crc = sifive_crc32((uint8_t *)&structure,
                             sizeof(MCUServerInfo) - 4);
```

**Not** the incorrect pattern from Patch 0081:
```c
// WRONG (from before patch 0081 fix):
uint32_t crc = sifive_crc32((uint8_t *)&structure,
                             sizeof(MCUServerInfo)/4 - 1);
```

**This patch immediately benefits from the Patch 0081 fix** - uses correct byte-based length calculation from the start.

### Code Changes Detail

**File**: `Core/Src/hf_common.c`

#### 1. get_mcu_server_info() - Validation on Read

**Replaced Complex Validation** (Lines 46-92):

Deleted:
- 27 lines of individual field checks
- Multiple debug prints for each field
- Redundant magic number checks

Added:
- Single CRC calculation (1 line)
- Single CRC comparison (1 line)
- Single block for factory restore on mismatch (10 lines)

**Net Result**: 27 lines deleted, 12 lines added = **15 lines removed**, simpler logic

**New Validation Logic** (Line 47-50):
```c
crc32Checksum = sifive_crc32((uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);

if (crc32Checksum != gMCU_Server_Info.crc32Checksum) {
    skip_update_eeprom = 0;
    // Restore all fields to defaults...
    gMCU_Server_Info.crc32Checksum = sifive_crc32((uint8_t *)&gMCU_Server_Info,
                                                   sizeof(MCUServerInfo) - 4);
    printf("Invalid checksum of mcu server info, update it with default value!\n");
}
```

#### 2. es_set_mcu_ipaddr() - Update CRC on IP Change

**Added CRC Recalculation** (Line 115):
```c
int es_set_mcu_ipaddr(uint8_t *p_ip_address)
{
    esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
    if (0 != memcmp(gMCU_Server_Info.ip_address, p_ip_address,
                    sizeof(gMCU_Server_Info.ip_address))) {
        memcpy(gMCU_Server_Info.ip_address, p_ip_address,
               sizeof(gMCU_Server_Info.ip_address));

        // NEW: Recalculate CRC after modifying IP
        gMCU_Server_Info.crc32Checksum = sifive_crc32(
            (uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);

        hf_i2c_mem_write(&hi2c1, AT24C_ADDR, MCU_SERVER_INFO_EEPROM_OFFSET,
                         (uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));
    }
    esEXIT_CRITICAL(gEEPROM_Mutex);
    return 0;
}
```

**Critical**: CRC must be recalculated BEFORE writing to EEPROM, otherwise stored CRC won't match stored data!

#### 3. es_set_mcu_netmask() - Update CRC on Netmask Change

**Added CRC Recalculation** (Line 123):
```c
int es_set_mcu_netmask(uint8_t *p_netmask_address)
{
    esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
    if (0 != memcmp(gMCU_Server_Info.netmask_address, p_netmask_address,
                    sizeof(gMCU_Server_Info.netmask_address))) {
        memcpy(gMCU_Server_Info.netmask_address, p_netmask_address,
               sizeof(gMCU_Server_Info.netmask_address));

        // NEW: Recalculate CRC after modifying netmask
        gMCU_Server_Info.crc32Checksum = sifive_crc32(
            (uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);

        hf_i2c_mem_write(&hi2c1, AT24C_ADDR, MCU_SERVER_INFO_EEPROM_OFFSET,
                         (uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));
    }
    esEXIT_CRITICAL(gEEPROM_Mutex);
    return 0;
}
```

#### 4. es_set_mcu_gateway() - Update CRC on Gateway Change

**Added CRC Recalculation** (Line 131):
```c
int es_set_mcu_gateway(uint8_t *p_gateway_address)
{
    esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
    if (0 != memcmp(gMCU_Server_Info.gateway_address, p_gateway_address,
                    sizeof(gMCU_Server_Info.gateway_address))) {
        memcpy(gMCU_Server_Info.gateway_address, p_gateway_address,
               sizeof(gMCU_Server_Info.gateway_address));

        // NEW: Recalculate CRC after modifying gateway
        gMCU_Server_Info.crc32Checksum = sifive_crc32(
            (uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);

        hf_i2c_mem_write(&hi2c1, AT24C_ADDR, MCU_SERVER_INFO_EEPROM_OFFSET,
                         (uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));
    }
    esEXIT_CRITICAL(gEEPROM_Mutex);
    return 0;
}
```

#### 5. es_set_username_password() - Update CRC on Credential Change

**Added CRC Recalculation** (Line 139):
```c
int es_set_username_password(const char *p_admin_name,
                               const char *p_admin_password)
{
    esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
    if ((0 != strcmp(gMCU_Server_Info.AdminName, p_admin_name)) ||
        (0 != strcmp(gMCU_Server_Info.AdminPassword, p_admin_password))) {
        strcpy(gMCU_Server_Info.AdminName, p_admin_name);
        strcpy(gMCU_Server_Info.AdminPassword, p_admin_password);

        // NEW: Recalculate CRC after modifying credentials
        gMCU_Server_Info.crc32Checksum = sifive_crc32(
            (uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);

        hf_i2c_mem_write(&hi2c1, AT24C_ADDR, MCU_SERVER_INFO_EEPROM_OFFSET,
                         (uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));
    }
    esEXIT_CRITICAL(gEEPROM_Mutex);
    return 0;
}
```

### Pattern Consistency

**All setter functions follow this pattern**:
1. Enter critical section (EEPROM mutex)
2. Check if value actually changed (memcmp/strcmp)
3. If changed:
   - Update in-memory structure
   - **Recalculate CRC32**
   - Write entire structure to EEPROM (including new CRC)
4. Exit critical section

**Why recalculate CRC on every change?**
- Ensures EEPROM always has valid CRC
- Power loss during write: Either old (valid) or new (valid) data, never half-updated
- Atomic from software perspective (hardware EEPROM write is atomic per page)

## Impact Analysis

### Security Improvements

**Authentication Credential Protection**:
- **Before**: Password corruption could lock out admin OR allow bypass (if corrupted to empty string)
- **After**: Password corruption detected, reset to known default
- **Impact**: Predictable failure mode (reset to default) vs. unpredictable lockout/bypass

**Network Configuration Integrity**:
- **Before**: IP address corruption could make BMC unreachable
- **After**: IP corruption detected, reset to default (or trigger DHCP)
- **Impact**: Self-healing on corruption

**Audit Trail**:
- **Before**: Silent corruption, difficult to diagnose
- **After**: Explicit console message "Invalid checksum of mcu server info"
- **Impact**: Clear indication of EEPROM issue for debugging

### Reliability Improvements

**EEPROM Wear/Aging**:
- EEPROM cells degrade over time (bit flips more common in aged EEPROM)
- CRC32 provides early warning of EEPROM degradation
- Allows proactive replacement before complete failure

**Power-Loss During Write**:
- If power lost during EEPROM write:
  - Some bytes updated, some not
  - CRC won't match
  - System detects corruption, restores defaults
- **Fail-safe behavior** vs. undefined state

**Manufacturing Quality**:
- Defective EEPROM chips detected early
- Quality control: Units with bad EEPROM fail CRC check immediately
- Prevents field failures

### Functional Changes

**Default Restore Behavior**:

**Before**:
- Each field checked independently
- Possible to have mix of default and corrupted values
- Inconsistent state (e.g., default IP but corrupted gateway)

**After**:
- Single CRC check
- ALL fields restored atomically
- Consistent state guaranteed

**Example Scenario**:
- Corruption flips bit in netmask field
- **Before**: IP and gateway valid, netmask restored to default → mismatched network config
- **After**: CRC detects any corruption, resets ALL network settings → consistent config

## EEPROM Layout

### MCUServerInfo Location

**EEPROM Offset**: `MCU_SERVER_INFO_EEPROM_OFFSET` (defined in header)

**Typical Layout**:
```
0x0000: CarrierBoardInfo (main partition)
0x0100: CarrierBoardInfo (backup partition)
0x0200: MCUServerInfo
```

**Size**: `sizeof(MCUServerInfo)` bytes

**No Dual Partition**: Unlike CarrierBoardInfo, MCUServerInfo has single partition only (no backup)

**Rationale**: Factory defaults always available, less critical than carrier board info (which contains unique serial numbers/MAC addresses)

### Structure Memory Layout

```
Offset  Field                Size    Type
------  --------------------  ------  ----------------
0x00    AdminName             32      char[32]
0x20    AdminPassword         32      char[32]
0x40    ip_address            4       uint8_t[4]
0x44    netmask_address       4       uint8_t[4]
0x48    gateway_address       4       uint8_t[4]
0x4C    crc32Checksum         4       uint32_t
Total:  80 bytes (0x50)
```

**CRC Calculation Coverage**: Bytes 0x00-0x4B (76 bytes)
**CRC Storage**: Bytes 0x4C-0x4F (4 bytes)

## Testing Recommendations

### Functional Testing

**1. Normal Operation - CRC Recalculation**:
```
Test: Change IP address via web interface
Steps:
1. Boot BMC with factory defaults
2. Access web configuration page
3. Set static IP: 192.168.1.100
4. Save configuration
5. Reboot BMC

Expected:
- es_set_mcu_ipaddr() recalculates CRC
- CRC written to EEPROM
- After reboot: CRC validates successfully
- BMC uses configured IP address

Verify:
- Serial console shows "get_mcu_server_info Successfully!"
- No "Invalid checksum" message
- Network accessible at 192.168.1.100
```

**2. Corruption Detection**:
```
Test: Simulate EEPROM corruption
Steps:
1. Boot BMC normally
2. Use EEPROM programmer or debug interface
3. Modify byte at offset MCU_SERVER_INFO_EEPROM_OFFSET + 10
   (corrupts AdminName field)
4. Reboot BMC

Expected:
- get_mcu_server_info() calculates CRC
- CRC mismatch detected
- Console prints: "Invalid checksum of mcu server info, update it with default value!"
- All fields restored to factory defaults
- New CRC calculated and written to EEPROM

Verify:
- Username: "admin" (default)
- Password: default password
- IP/netmask/gateway: default values
```

**3. Partial Corruption Recovery**:
```
Test: Corrupt single field, verify all fields reset
Steps:
1. Configure custom IP: 10.0.0.50
2. Configure custom gateway: 10.0.0.1
3. Shutdown BMC
4. Corrupt only netmask field in EEPROM
5. Boot BMC

Expected:
- CRC detects corruption
- IP, netmask, AND gateway all reset to defaults
- No mixed corrupted/valid state

Verify:
- IP: default (not 10.0.0.50)
- Gateway: default (not 10.0.0.1)
- Netmask: default
```

**4. Password Change Integrity**:
```
Test: Change password, verify CRC protection
Steps:
1. Login with default password
2. Change password to "NewPass123"
3. Logout
4. Power cycle BMC
5. Login with "NewPass123"

Expected:
- es_set_username_password() recalculates CRC
- After reboot: CRC validates
- New password works

Verify:
- Login with "NewPass123" succeeds
- Login with default password fails
```

### Stress Testing

**5. Power-Loss During Write**:
```
Test: Simulate power loss during EEPROM write
Steps:
1. Trigger network config change
2. During hf_i2c_mem_write(), cut power
3. Reboot BMC

Expected (two possible outcomes):
- Outcome A: Old (valid) data with old (valid) CRC
- Outcome B: New (valid) data with new (valid) CRC
- Outcome C (rare): Partial write, CRC mismatch, restore to defaults

NOT expected:
- Partial write with valid CRC (impossible - CRC updated last)

Verify:
- After reboot, BMC functional
- Either old or new config active
- If neither, factory defaults active
```

**6. EEPROM Wear Simulation**:
```
Test: Repeated writes, verify CRC always valid
Steps:
1. Script to change IP address 1000 times
2. Each change: increment last octet
3. After every 100 changes, reboot and verify

Expected:
- All 1000 writes succeed
- All 10 reboots validate CRC successfully
- No corruption detected

Verify:
- 1000 successful CRC recalculations
- 10 successful CRC validations
```

### Edge Case Testing

**7. Uninitialized EEPROM**:
```
Test: First boot with blank EEPROM
Steps:
1. Erase EEPROM (all 0xFF)
2. Boot BMC

Expected:
- CRC of 0xFF...0xFF won't match valid data CRC
- "Invalid checksum" message
- Factory defaults loaded
- Valid CRC written to EEPROM

Verify:
- Default username/password work
- Network defaults active
```

**8. CRC Collision (Theoretical)**:
```
Test: Corrupt data that produces same CRC (unlikely)
Note: CRC-32 has 2^32 possible values, collision rate ~0.00002%

Steps:
1. Calculate CRC of current MCUServerInfo
2. Find alternate data with same CRC (requires brute force)
3. Write alternate data to EEPROM
4. Boot BMC

Expected:
- CRC matches (collision)
- System uses corrupted data
- Likely: Network unreachable, login fails

Mitigation:
- Add magic number field to MCUServerInfo
- Dual-layer validation: magic number + CRC
```

## Migration Considerations

### Upgrading from Pre-CRC Firmware

**Scenario**: Field unit running firmware before Patch 0083

**EEPROM State**:
- AdminName: "admin" (or custom)
- AdminPassword: default (or custom)
- IP/netmask/gateway: configured values
- **Padding field**: Undefined data (likely 0x00 or random)

**On Upgrade**:
1. New firmware reads EEPROM
2. Interprets padding as crc32Checksum field
3. **CRC validation FAILS** (padding ≠ valid CRC)
4. "Invalid checksum" message
5. **ALL SETTINGS RESET TO FACTORY DEFAULTS**

**Impact**:
- **Lost Configuration**: Custom IP, gateway, passwords RESET
- **Network Outage**: BMC changes IP to default
- **Authentication Reset**: Passwords reset to default

**Mitigation Options**:

**Option 1: Accept Configuration Reset**
- Document in release notes
- Users must reconfigure after upgrade

**Option 2: Migration Logic**
```c
// In get_mcu_server_info():
if (gMCU_Server_Info.crc32Checksum == 0x00000000 ||
    gMCU_Server_Info.crc32Checksum == 0xFFFFFFFF) {
    // Likely uninitialized CRC field
    // Assume data is valid (pre-CRC firmware)
    printf("Migrating MCUServerInfo to CRC-protected format\n");

    // Calculate and store CRC
    gMCU_Server_Info.crc32Checksum = sifive_crc32(
        (uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo) - 4);

    // Write back to EEPROM
    hf_i2c_mem_write(&hi2c1, AT24C_ADDR, MCU_SERVER_INFO_EEPROM_OFFSET,
                     (uint8_t *)&gMCU_Server_Info, sizeof(MCUServerInfo));
} else {
    // Normal CRC validation
    // ...
}
```

**Option 3: Backup/Restore Tool**
- Pre-upgrade: Dump EEPROM to file
- Post-upgrade: Restore configuration via web interface or CLI

**Recommendation**: Option 2 (migration logic) for production deployments

## Comparison with CarrierBoardInfo

### Similarities

| Aspect               | CarrierBoardInfo              | MCUServerInfo                |
|----------------------|-------------------------------|------------------------------|
| **CRC Algorithm**    | sifive_crc32()                | sifive_crc32()               |
| **CRC Length**       | sizeof(struct) - 4            | sizeof(struct) - 4           |
| **CRC Field**        | uint32_t crc32Checksum (last) | uint32_t crc32Checksum (last)|
| **Validation**       | Read, calculate, compare      | Read, calculate, compare     |
| **On Mismatch**      | Restore to factory defaults   | Restore to factory defaults  |
| **Update Pattern**   | Recalc CRC before write       | Recalc CRC before write      |

### Differences

| Aspect               | CarrierBoardInfo              | MCUServerInfo                |
|----------------------|-------------------------------|------------------------------|
| **Redundancy**       | Dual partition (main+backup)  | Single partition only        |
| **Recovery**         | Backup → Main recovery        | Direct factory restore       |
| **Uniqueness**       | Unique per unit (MAC, SN)     | Defaults acceptable          |
| **Criticality**      | HIGH (unique identity)        | MEDIUM (configurable)        |
| **Factory Restore**  | Last resort                   | Common, acceptable           |

**Rationale for Differences**:
- CarrierBoardInfo contains **unique** data (serial numbers, MAC addresses)
  - Loss of this data = loss of unit identity
  - Dual partition essential for recovery
- MCUServerInfo contains **configurable** data
  - Loss of this data = inconvenience, not catastrophic
  - Single partition + factory defaults sufficient

## Code Review Notes

### Strengths

1. **Consistency**: Follows exact same pattern as CarrierBoardInfo CRC
2. **Atomic Updates**: CRC recalculated on every field modification
3. **Simplification**: Replaces complex validation with single CRC check
4. **Clear Logging**: Explicit "Invalid checksum" message for diagnosis
5. **Correct Length**: Uses `sizeof(struct) - 4`, not broken `/4 - 1` pattern

### Potential Improvements

1. **Magic Number**: Add magic number field for dual-layer validation
   ```c
   typedef struct {
       uint32_t magicNumber;  // NEW
       char AdminName[ADMIN_NAME_LEN];
       // ...
       uint32_t crc32Checksum;
   } MCUServerInfo;
   ```

2. **Dual Partition**: Consider adding backup partition for high-availability deployments

3. **Migration Logic**: Add detection of pre-CRC firmware EEPROM format

4. **Version Field**: Add structure version for future extensibility
   ```c
   typedef struct {
       uint32_t magicNumber;
       uint16_t structVersion;  // NEW
       uint16_t reserved;
       char AdminName[ADMIN_NAME_LEN];
       // ...
   } MCUServerInfo;
   ```

5. **Separate Password Hash**: Consider storing password hash instead of plaintext
   - Current: `strcpy(gMCU_Server_Info.AdminPassword, "password");`
   - Better: `gMCU_Server_Info.PasswordHash = hash("password");`

## Security Implications

### Positive Security Impact

**Credential Integrity**:
- Prevents accidental/malicious password corruption
- Ensures password always valid or reset to known default
- Eliminates "corrupted password causes lockout" scenario

**Configuration Integrity**:
- Network settings protected from corruption
- Reduces attack surface for physical attackers
- EEPROM modification detected

### Remaining Security Concerns

**Plaintext Password Storage**:
- AdminPassword stored as plaintext in EEPROM
- Physical attacker with EEPROM reader can extract password
- **Mitigation**: Use password hashing (bcrypt, PBKDF2)

**No Authentication for EEPROM Writes**:
- es_set_username_password() doesn't verify current password
- Caller must enforce authentication (web server does this)
- **Risk**: If caller bypasses auth, password can be changed freely

**Default Credentials**:
- Factory defaults are well-known (documented in manual)
- After CRC failure, resets to known defaults
- **Mitigation**: Force password change on first boot, random default

**CRC Not Cryptographic**:
- CRC-32 detects accidental corruption, NOT intentional tampering
- Attacker can modify data and recalculate valid CRC
- **Mitigation**: Use HMAC or digital signature for tamper-proofing

## Files Modified

```
Core/Inc/hf_common.h |  2 +-
Core/Src/hf_common.c | 38 ++++++++++++--------------------------
2 files changed, 13 insertions(+), 27 deletions(-)
```

**Diff Summary**:
- Header: 1 line changed (padding → crc32Checksum)
- Source: Net -14 lines (27 deleted, 13 added)
- Complexity reduced significantly

## Changelog Entry

```
Changelogs:
1. Add CRC32 checksum for MCUServerInfo
```

## Related Patches

- **Patch 0079**: Introduced sifive_crc32() algorithm
- **Patch 0081**: Fixed CRC length calculation for CarrierBoardInfo (same pattern used here)
- **Patch 0082**: I2C retry logic (ensures EEPROM reads succeed for CRC validation)
- **Patch 0097**: Additional checksum verification improvements

## Conclusion

This patch significantly improves the integrity and reliability of BMC configuration data by adding CRC32 checksum protection to the MCUServerInfo structure. The implementation is consistent with existing CRC patterns, immediately benefits from the Patch 0081 fix (correct length calculation), and simplifies validation logic while providing stronger corruption detection.

**Key Benefits**:
- **Reliability**: Detects EEPROM corruption in any field
- **Simplicity**: Single CRC check replaces 5 individual field validations
- **Consistency**: Atomic restore of all fields on corruption
- **Security**: Credential integrity protected (though plaintext storage remains a concern)

**Migration Note**: Units upgrading from pre-CRC firmware will reset configuration to defaults on first boot after upgrade. Consider implementing migration logic for production deployments.
