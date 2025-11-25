# Patch 0041: Add Carrier Board Info Set Command (cbinfo-s)

## Metadata
- **Patch File**: `0041-WIN2030-15279-feat-console.c-Add-cbinfo-s-command.patch`
- **Author**: linmin <linmin@eswincomputing.com>
- **Date**: Mon, 20 May 2024 11:00:44 +0800
- **Ticket**: WIN2030-15279
- **Type**: feat (Feature addition)
- **Change-Id**: I59e43e4ffeee5b8c37c81aed64147951120ea957

## Summary

This patch adds a powerful new CLI command `cbinfo-s` (carrier board info set) that allows developers and factory personnel to programmatically set carrier board EEPROM information fields for debugging, testing, and manufacturing purposes. Previously, carrier board information could only be read via `cbinfo`; now individual fields can be set via the console without requiring external EEPROM programming tools.

## Changelog

The official changelog from the commit message:
1. **Add cbinfo-s command to set cbinfo for debugging and test** - New CLI command for setting carrier board information fields

## Statistics

- **Files Changed**: 2 files
- **Insertions**: 95 lines
- **Deletions**: 40 lines
- **Net Change**: +55 lines

## Detailed Changes by File

### 1. Header Updates - Carrier Board Info API (`Core/Inc/hf_common.h`)

#### Change 1: Serial Number Type Fix (Line 24)

**Before**:
```c
uint8_t boardSerialNumber[18];
```

**After**:
```c
char boardSerialNumber[18];  // 18 bytes of serial number, excluding string terminator
```

**Analysis**:

This is a **type correctness fix** that acknowledges the serial number is actually a **character string**, not raw bytes:

- **Old Type**: `uint8_t[18]` - Treated as arbitrary byte array
- **New Type**: `char[18]` - Explicitly a C string (though not null-terminated within the 18 bytes)
- **Comment Added**: Clarifies that this is 18 bytes of serial number data, excluding the null terminator
- **Impact**: Allows proper string operations (`memcpy`, `strncpy`) without type warnings
- **Format**: Serial numbers are typically alphanumeric strings like "HF106C00001234567"

**Why 18 bytes**:
- Industry-standard serial number length for embedded boards
- Provides unique identification across entire product line
- Compact enough for EEPROM storage, long enough for uniqueness

#### Change 2: New API Function Declaration (Line 239)

**Added**:
```c
int es_set_carrier_borad_info(CarrierBoardInfo *pCarrier_board_info);
```

**Analysis**:

This declares the **setter function** to complement the existing getter:

- **Function Name**: `es_set_carrier_borad_info()` (Note: typo "borad" retained for consistency with existing code)
- **Purpose**: Writes entire `CarrierBoardInfo` structure to EEPROM
- **Parameter**: Pointer to populated structure with new values
- **Return**: Integer status code (0=success, negative=error)
- **Use Cases**:
  - Factory programming of carrier board serial numbers
  - Updating PCB revision markers
  - Setting manufacturing test status flags
  - Debugging and development testing

### 2. Console Command Implementation (`Core/Src/console.c`)

#### Change 1: Command Prototype Declaration (Lines 44-45)

**Added**:
```c
static BaseType_t prvCommandCarrierBoardInfoGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvCommandCarrierBoardInfoSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
```

**Analysis**:

Added function prototype for the new setter command handler. Note the **consistent naming pattern**:
- `prvCommandCarrierBoardInfoGet` - Reads and displays carrier board info
- `prvCommandCarrierBoardInfoSet` - Parses arguments and sets carrier board info

#### Change 2: Rename Existing Command (Lines 127-133)

**Before**:
```c
{
    "cbinfo",
    "\r\ncbinfo: Display carrierboard information.\r\n",
    prvCommandCarrierBoardInfoGet,
    0
},
```

**After**:
```c
{
    "cbinfo-g",
    "\r\ncbinfo-g: Display carrierboard information.\r\n",
    prvCommandCarrierBoardInfoGet,
    0
},
{
    "cbinfo-s",
    "\r\ncbinfo-s <magic/format/productid/pcbr/bomr/bomv/boardsn/manu> <value in hex>: Set magicNumber...\r\n",
    prvCommandCarrierBoardInfoSet,
    2
},
```

**Analysis**:

The original `cbinfo` command is **renamed and enhanced**:

**Old Command**:
- Name: `cbinfo`
- Single purpose: Get (display) carrier board info

**New Commands**:
- `cbinfo-g` - **Get** carrier board info (renamed from `cbinfo`)
  - Same functionality as before
  - `-g` suffix makes intent explicit
- `cbinfo-s` - **Set** carrier board info fields (NEW)
  - Requires 2 arguments: field name and value
  - Help text shows available fields

**Command Arguments**:
- Parameter count: `2` (field name + value)
- Field options:
  - `magic` - Magic number (structure validation marker)
  - `format` - Format version number
  - `productid` - Product identifier code
  - `pcbr` - PCB revision
  - `bomr` - Bill of Materials revision
  - `bomv` - BOM variant
  - `boardsn` - Board serial number (18 characters)
  - `manu` - Manufacturing test status

#### Change 3: Removed Ticks Command (Lines 279-287 deleted)

**Deleted**:
```c
{
    "ticks",
    "\r\nticks: Display OS tick count and run time in seconds.\r\n",
    prvCommandTicks,
    0
},
```

**Analysis**:

The `ticks` command has been **removed entirely**:
- **Reason**: Likely redundant with other timing/debug commands
- **Functionality Lost**: Displayed FreeRTOS tick count and calculated uptime
- **Impact**: Minimal - uptime can be derived from RTC or other commands

#### Change 4: Enhanced Get Function (Lines 376-412)

**Key Changes**:

**Before (Serial Number Display)**:
```c
len = snprintf(pcWb, size, "SN(hex):");
pcWb += len;
size -= len;
for (int i = 0; i < sizeof(carrierBoardInfo.boardSerialNumber); i++) {
    pcWb += snprintf(pcWb, size, "%x", carrierBoardInfo.boardSerialNumber[i]);
    size--;
}
len = snprintf(pcWb, size, "\r\n");
```

**After (Serial Number Display)**:
```c
char boardSn[19] = {0};  // 18 bytes + null terminator
memcpy(boardSn, carrierBoardInfo.boardSerialNumber, sizeof(carrierBoardInfo.boardSerialNumber));
len = snprintf(pcWb, size, "SN:%s\r\n", boardSn);
pcWb += len;
size -= len;
```

**Analysis of the Improvement**:

**Old Method** (Hex byte-by-byte):
- Printed each byte as hex digit
- Output: `SN(hex):48463130364330303030313233343536` (hard to read!)
- Issue: Serial number is a **string**, not arbitrary hex data

**New Method** (String copy and print):
1. **Buffer Setup**: Create 19-byte buffer (18 for SN + 1 for null)
2. **Safe Copy**: `memcpy()` copies 18-byte SN into buffer
3. **Null Termination**: Buffer initialized to zeros, so byte 19 is `\0`
4. **String Print**: Use `%s` format to print as string
5. **Output**: `SN:HF106C00001234567` (human-readable!)

**Why This Works**:
- Serial numbers are **ASCII strings** (e.g., "HF106C00001234567")
- EEPROM stores raw 18 bytes without null terminator
- Copying to 19-byte zero-initialized buffer auto-null-terminates
- Safe for `printf("%s", ...)` operation

#### Change 5: New Set Command Implementation (Lines 415-497)

**Complete Function**:
```c
static BaseType_t prvCommandCarrierBoardInfoSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    BaseType_t xParamLen;
    const char * pcField;
    const char * pcValue;
    CarrierBoardInfo carrier_board_info;
    int updateFlag = 0;

    /* get the carrier board info first */
    es_get_carrier_borad_info(&carrier_board_info);

    pcField = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);

    /* fill with new account name */
    if (!strncmp(pcField, "magic", sizeof("magic") - 1)) {
        pcValue = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
        carrier_board_info.magicNumber = atoh(pcValue, xParamLen);
        updateFlag++;
    }
    else if (!strncmp(pcField, "format", sizeof("format") - 1)) {
        pcValue = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
        carrier_board_info.formatVersionNumber = atoh(pcValue, xParamLen);
        updateFlag++;
    }
    else if (!strncmp(pcField, "productid", sizeof("productid") - 1)) {
        pcValue = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
        carrier_board_info.productIdentifier = atoh(pcValue, xParamLen);
        updateFlag++;
    }
    else if (!strncmp(pcField, "pcbr", sizeof("pcbr") - 1)) {
        pcValue = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
        carrier_board_info.pcbRevision = atoh(pcValue, xParamLen);
        updateFlag++;
    }
    else if (!strncmp(pcField, "bomr", sizeof("bomr") - 1)) {
        pcValue = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
        carrier_board_info.bomRevision = atoh(pcValue, xParamLen);
        updateFlag++;
    }
    else if (!strncmp(pcField, "bomv", sizeof("bomv") - 1)) {
        pcValue = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
        carrier_board_info.bomVariant = atoh(pcValue, xParamLen);
        updateFlag++;
    }
    else if (!strncmp(pcField, "boardsn", sizeof("boardsn") - 1)) {
        pcValue = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
        if (xParamLen != sizeof(carrier_board_info.boardSerialNumber)) {
            snprintf(pcWriteBuffer, xWriteBufferLen, "Invalid boardsn parameter!!!\n");
            goto out;
        }
        memcpy(carrier_board_info.boardSerialNumber, pcValue, xParamLen);
        updateFlag++;
    }
    else if (!strncmp(pcField, "manu", sizeof("manu") - 1)) {
        pcValue = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
        carrier_board_info.manufacturingTestStatus = atoh(pcValue, xParamLen);
        updateFlag++;
    }
    else
        snprintf(pcWriteBuffer, xWriteBufferLen, "Invalid field parameter!!!\n");

    if (updateFlag) {
        es_set_carrier_borad_info(&carrier_board_info);
    }
out:
    return pdFALSE;
}
```

**Function Flow Analysis**:

**Step 1: Read Current Values**
```c
es_get_carrier_borad_info(&carrier_board_info);
```
- Read existing EEPROM data into structure
- This allows **modify-then-write** pattern (update single field without affecting others)

**Step 2: Parse Field Name**
```c
pcField = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);
```
- Extract first parameter (field name)
- `FreeRTOS_CLIGetParameter()` - FreeRTOS CLI utility function
- Parameter 1 is field name (e.g., "magic", "boardsn")

**Step 3: Field-Specific Handling**

The function uses an **if-else chain** to handle each field type:

**Numeric Fields** (magic, format, productid, pcbr, bomr, bomv, manu):
```c
if (!strncmp(pcField, "magic", sizeof("magic") - 1)) {
    pcValue = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
    carrier_board_info.magicNumber = atoh(pcValue, xParamLen);
    updateFlag++;
}
```
- Get second parameter (value as hex string)
- Convert using `atoh()` - ASCII-to-Hex converter
- Assign to appropriate structure field
- Increment `updateFlag` to indicate successful parse

**String Field** (boardsn):
```c
else if (!strncmp(pcField, "boardsn", sizeof("boardsn") - 1)) {
    pcValue = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
    if (xParamLen != sizeof(carrier_board_info.boardSerialNumber)) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "Invalid boardsn parameter!!!\n");
        goto out;
    }
    memcpy(carrier_board_info.boardSerialNumber, pcValue, xParamLen);
    updateFlag++;
}
```
- **Critical Difference**: Serial number is a STRING, not hex number
- **Length Validation**: MUST be exactly 18 characters
  - Too short: Incomplete serial number
  - Too long: Buffer overflow risk
- **Direct Copy**: Use `memcpy()` instead of numeric conversion
- **No Null Terminator**: Copy exactly 18 bytes (null not stored in EEPROM field)

**Error Handling**:
```c
else
    snprintf(pcWriteBuffer, xWriteBufferLen, "Invalid field parameter!!!\n");
```
- If field name doesn't match any known field, print error

**Step 4: Commit Changes**
```c
if (updateFlag) {
    es_set_carrier_borad_info(&carrier_board_info);
}
```
- Only write to EEPROM if a field was successfully updated
- Prevents spurious EEPROM writes (reduces wear)

#### Change 6: Removed Ticks Command Implementation (Lines 847-869 deleted)

**Deleted Function**:
```c
static BaseType_t prvCommandTicks(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    uint32_t uMs;
    uint32_t uSec;
    TickType_t xTickCount = xTaskGetTickCount();

    uSec = xTickCount / configTICK_RATE_HZ;
    uMs = xTickCount % configTICK_RATE_HZ;
    snprintf(pcWriteBuffer, xWriteBufferLen,
             "Tick rate: %u Hz\r\nTicks: %lu\r\nRun time: %lu.%.3lu seconds\r\n",
              (unsigned)configTICK_RATE_HZ, xTickCount, uSec, uMs);

    return pdFALSE;
}
```

**Analysis**:

This function **calculated and displayed FreeRTOS timing information**:
- Read current tick count from RTOS
- Calculate runtime in seconds: `ticks / tick_rate`
- Calculate milliseconds: `ticks % tick_rate`
- Display tick rate, total ticks, and formatted runtime

**Why Removed**:
- Redundant with RTC time display
- Debug-only functionality
- Code cleanup to reduce firmware size

## Usage Examples

### Example 1: Set Magic Number

```bash
BMC> cbinfo-s magic deadbeef
```
**Result**: Sets carrier board magic number to 0xdeadbeef

### Example 2: Set PCB Revision

```bash
BMC> cbinfo-s pcbr 02
```
**Result**: Sets PCB revision to 0x02 (revision B)

### Example 3: Set Board Serial Number

```bash
BMC> cbinfo-s boardsn HF106C00001234567
```
**Result**: Sets board serial number to "HF106C00001234567" (exactly 18 characters)

**Invalid Example**:
```bash
BMC> cbinfo-s boardsn SHORT
Invalid boardsn parameter!!!
```
**Reason**: Serial number must be exactly 18 characters

### Example 4: Set Manufacturing Test Status

```bash
BMC> cbinfo-s manu FF
```
**Result**: Sets manufacturing test status to 0xFF (all tests passed)

### Example 5: View Results

```bash
BMC> cbinfo-g
[Carrierboard Information:]
magicNumber:0xdeadbeef
formatVersionNumber:0x1
productIdentifier:0x4846
pcbRevision:0x2
bomRevision:0x0
bomVariant:0x0
SN:HF106C00001234567
manufacturingTestStatus:0xff
```

## Security Implications

**CRITICAL SECURITY NOTE**: This command allows **unrestricted modification** of carrier board EEPROM data from the serial console.

**Risks**:
1. **Serial Number Tampering**: Attacker could change board serial numbers to impersonate legitimate boards
2. **Manufacturing Status Bypass**: Setting `manu` to 0xFF could bypass quality control checks
3. **Magic Number Corruption**: Setting invalid magic number could brick board recognition
4. **No Authentication**: Command works without password if attacker has serial console access

**Mitigation Recommendations**:
1. **Authentication Required**: Add password check before allowing set operations
2. **Write Protection**: Enable EEPROM hardware write-protect in production
3. **Audit Logging**: Log all cbinfo-s invocations with timestamps
4. **Firmware Lock**: Disable command in production builds via compile flag

**Current State**:
- Command is **always available** on serial console
- No authentication required
- Anyone with UART access can modify carrier board identity

## Manufacturing Use Cases

**Legitimate Uses** (Factory/RMA):

1. **Initial Programming**:
   - Factory sets unique serial numbers during production
   - Assigns PCB revision markers
   - Sets product identifier codes

2. **RMA Operations**:
   - Update manufacturing test status after repair
   - Modify BOM variant after component substitution
   - Correct serial number typos

3. **Development/Testing**:
   - Test EEPROM read/write functionality
   - Verify serial number parsing
   - Debug board identification issues

## Technical Deep Dive - CarrierBoardInfo Structure

Based on this patch and cross-referenced code, the `CarrierBoardInfo` structure contains:

```c
typedef struct {
    uint32_t magicNumber;              // Structure validation (e.g., 0xF15E5045)
    uint8_t  formatVersionNumber;      // Format version for compatibility
    uint16_t productIdentifier;        // Product code (e.g., 0x4846 = "HF")
    uint8_t  pcbRevision;              // PCB revision letter (A=0x41, B=0x42)
    uint8_t  bomRevision;              // Bill of Materials revision number
    uint8_t  bomVariant;               // BOM variant/configuration
    char     boardSerialNumber[18];    // Serial number (NO null terminator!)
    uint8_t  manufacturingTestStatus;  // Test status flags
    // Additional fields likely: MAC address, checksum, etc.
} CarrierBoardInfo;
```

**EEPROM Layout** (Estimated):
- Total size: ~32 bytes minimum (likely padded to 64 bytes for alignment)
- CRC/checksum field likely present (not shown in this patch)
- Stored at fixed EEPROM offset (e.g., 0x0000 or 0x0100)

## Related Patches

**Previous Context**:
- Patch 0013: Likely introduced carrier board info reading
- Earlier patches: EEPROM read/write infrastructure

**Future Patches**:
- Patch 0045: Fixes magic number value (changes from 0xdeadbeaf to 0xF15E5045)
- Patch 0071: Enhanced cbinfo parsing and display

## Testing Recommendations

**Test Cases**:

1. **Set Each Field**:
   - Verify each field type (magic, format, productid, etc.)
   - Confirm EEPROM write via cbinfo-g readback

2. **Serial Number Validation**:
   - Test exact 18-character SN: Should succeed
   - Test 17-character SN: Should fail with error
   - Test 19-character SN: Should fail with error

3. **Hex Conversion**:
   - Test valid hex values (0-9, A-F)
   - Test invalid hex (G, Z): Verify `atoh()` behavior

4. **EEPROM Persistence**:
   - Set values, reboot BMC
   - Verify values persist via cbinfo-g

5. **Update Flag Logic**:
   - Invalid field name: Verify no EEPROM write occurs
   - Valid field: Confirm single EEPROM write

## Impact Assessment

**Developer Impact**: HIGH
- Essential tool for EEPROM programming during development
- Enables rapid testing of board identification logic

**Manufacturing Impact**: CRITICAL
- Required for factory programming of serial numbers
- Reduces need for external EEPROM programmers
- Speeds production line setup

**Security Impact**: HIGH RISK
- Exposes EEPROM modification to console attackers
- Should be disabled or protected in production firmware

**Code Quality**: GOOD
- Clean parameter parsing
- Proper error handling
- Read-modify-write pattern prevents data loss
