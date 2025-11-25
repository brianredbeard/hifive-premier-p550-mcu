# Patch 0080: EEPROM Write Protection Command

## Metadata
- **Patch File**: `0080-WIN2030-15279-feat-console-Add-eeprom-write-protect-.patch`
- **Author**: linmin <linmin@eswincomputing.com>
- **Date**: Sat, 8 Jun 2024 15:39:50 +0800
- **Ticket**: WIN2030-15279
- **Type**: feat (Feature addition - production/test commands)
- **Change-Id**: I9ed59dc65fc5e4988de918c3ae4a397039a146e7
- **Criticality**: HIGH - Production line tool, prevents accidental EEPROM corruption

## Summary

This patch adds two new console commands for **production line testing and diagnostics**:

1. **`eepromwp-s <1|0>`**: Enable/disable hardware EEPROM write protection
2. **`pwrlast-g`**: Get the last SoM power state from EEPROM

These commands serve critical functions in manufacturing and field diagnostics, but are **intentionally hidden** from normal users (not shown in help output) to prevent accidental misuse that could brick the BMC.

**Key Design Decision**: The `eepromwp-s` command description is set to an **empty string** (`""`), which prevents it from appearing in the help menu while still making it accessible to trained production personnel who know the command syntax.

## Changelog

From the commit message:
1. **Add eepromwp-s <1 or 0>**: Enable or disable EEPROM write protect
   - "This is only for burning eeprom on the production line and not available for users. So this command will not be shown with help command."
2. **Add pwrlast-g command**: Get the SoM power last state from EEPROM
   - "It is used for test to determine whether the power off of the som is expected."

## Statistics

- **Files Changed**: 1 file (`Core/Src/console.c`)
- **Insertions**: 68 lines
- **Deletions**: 0 lines
- **Net Change**: +68 lines

### File Breakdown
- `Core/Src/console.c`: +68 lines (two new command implementations)

## EEPROM Write Protection Hardware

### WP Pin Architecture

**Hardware Implementation**:

EEPROM ICs (AT24Cxx series) include a **hardware write-protect (WP) pin** that, when asserted, prevents any write operations to the memory array.

**AT24C256 EEPROM Pinout**:
```
     +---+---+
 A0  | 1   8 | VCC
 A1  | 2   7 | WP   ← Write Protect (active HIGH)
 A2  | 3   6 | SCL
 GND | 4   5 | SDA
     +-------+
```

**WP Pin Behavior**:
- **WP = LOW (0V)**: Write operations **enabled** (normal operation)
- **WP = HIGH (3.3V)**: Write operations **disabled** (protected)
  - Write commands acknowledged but data not written
  - Read operations still function normally
  - Protection persists until WP brought low

**GPIO Connection** (BMC → EEPROM):
```
STM32F407 GPIO → AT24C256 WP Pin
EEPROM_WP_Pin (PB12 or similar) → Pin 7 of EEPROM

Default state: LOW (writes enabled)
Protection enabled: HIGH (writes disabled)
```

### Why Hardware Write Protection?

**Production Line Benefits**:

1. **Prevents Accidental Corruption**:
   - During manufacturing, EEPROM is programmed with critical data (serial numbers, MAC addresses)
   - After programming, WP enabled to prevent accidental overwrites
   - Ensures data integrity during subsequent manufacturing steps

2. **Field Service Protection**:
   - Prevents end-users from modifying factory-programmed data
   - Protects against accidental `eeprom write` commands
   - Reduces RMA rate from user-induced EEPROM corruption

3. **Secure Configuration**:
   - Ensures carrier board info remains immutable in field deployment
   - Prevents configuration tampering
   - Complements CRC32 validation (patch 0079)

**Limitations**:
- Only protects against **software writes** via I2C
- Physical access to I2C bus can bypass (external I2C master)
- Can be disabled by authorized personnel with console access

### Production Workflow

**Manufacturing Process**:

```
1. BMC Powered On (WP = LOW, writes enabled by default)
   ↓
2. Production test software runs
   ↓
3. EEPROM programmed with:
   - Carrier board serial number
   - MAC addresses (SoM MAC1, MAC2, MCU MAC3)
   - Board revision, BOM variant
   - Manufacturing test status
   - CRC32 checksum (using SiFive algorithm from patch 0079)
   ↓
4. Verify EEPROM contents (read back and CRC check)
   ↓
5. Enable write protection:
   $ eepromwp-s 1
   EEPROM Write protect enabled!
   ↓
6. Test write protection:
   - Attempt to write EEPROM → should fail
   - Verify original data still intact
   ↓
7. BMC ships with EEPROM locked
```

**Field Reprogramming** (RMA/Repair):

```
1. Authorized technician connects via serial console
   ↓
2. Disable write protection:
   $ eepromwp-s 0
   EEPROM Write protect disabled!
   ↓
3. Reprogram EEPROM (cbinfo-s command or similar)
   ↓
4. Re-enable protection:
   $ eepromwp-s 1
   EEPROM Write protect enabled!
```

## Detailed Implementation Analysis

### 1. Function Prototypes

**File**: `Core/Src/console.c` (lines 29-32 added)

```c
// get the power last state from eeprom: 1 on , 0 off
static BaseType_t prvCommandSomPwrLastStateGet(char *pcWriteBuffer,
                                                 size_t xWriteBufferLen,
                                                 const char *pcCommandString);

// eeprom write protect pin set
static BaseType_t prvCommandCBWpEEPROM(char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString);
```

**Naming Convention**:
- `prvCommand` prefix: "private command" (static function, internal to console.c)
- `SomPwrLastStateGet`: Descriptive name for power state retrieval
- `CBWpEEPROM`: "Carrier Board Write Protect EEPROM"

**Return Type**: `BaseType_t`
- FreeRTOS CLI return value
- `pdFALSE`: Command completed, no more output
- `pdTRUE`: Command has more output (for multi-line responses)

### 2. Command Registration

**File**: `Core/Src/console.c` (lines 287-299 added to `xCommands[]` array)

#### pwrlast-g Command

```c
{
    "pwrlast-g",
    "\r\npwrlast-g: Get the som power last state. 1 or 0.\r\n",
    prvCommandSomPwrLastStateGet,
    0  // No parameters
},
```

**Command Structure**:
- **Name**: `"pwrlast-g"` (power last state - get)
- **Help Text**: Visible in help menu, describes function
- **Handler**: `prvCommandSomPwrLastStateGet`
- **Parameter Count**: 0 (no arguments required)

**Naming Rationale**:
- `-g` suffix: "get" (query operation, read-only)
- Consistent with `powerlost-g` from patch 0071
- Distinguishes from potential future `pwrlast-s` (set) command

#### eepromwp-s Command

```c
{
    "eepromwp-s",
    "",  // EMPTY STRING - hidden from help!
    prvCommandCBWpEEPROM,
    1  // Requires 1 parameter
},
```

**Command Structure**:
- **Name**: `"eepromwp-s"` (EEPROM write protect - set)
- **Help Text**: **EMPTY STRING** - intentionally hidden
- **Handler**: `prvCommandCBWpEEPROM`
- **Parameter Count**: 1 (enable/disable flag)

**Why Hide from Help?**

**Security Through Obscurity** (limited effectiveness):
```c
// FreeRTOS CLI help command implementation:
for (i = 0; i < num_commands; i++) {
    if (strlen(commands[i].help_text) > 0) {  // Only show non-empty help
        printf("%s", commands[i].help_text);
    }
}
```

**Rationale**:
1. **Prevents Accidental Use**: Casual users won't discover command
2. **Documented Internally**: Production personnel trained on command syntax
3. **Security Note**: This is **not strong security**, just deterrence
   - Knowledgeable attacker can still find command by:
     - Reading firmware source code
     - Reverse engineering console.c
     - Trying common command patterns

**Better Security Approach** (not implemented):
```c
// Require special unlock sequence
static bool wp_unlock_mode = false;

static BaseType_t prvCommandUnlockWP(...)
{
    // Check for authorization (password, key, etc.)
    if (authorized) {
        wp_unlock_mode = true;
        printf("EEPROM WP commands unlocked for 60 seconds\n");
        start_timeout_timer(60000);  // Auto-lock after 60s
    }
}

static BaseType_t prvCommandCBWpEEPROM(...)
{
    if (!wp_unlock_mode) {
        printf("Error: EEPROM WP locked. Use 'unlock-wp' first.\n");
        return pdFALSE;
    }
    // ... proceed with WP control
}
```

### 3. Power Last State Retrieval

**Function**: `prvCommandSomPwrLastStateGet()` (lines 1320-1341)

```c
/**
* @brief Get the som power last state from eeprom: 1 on or 0 off
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandSomPwrLastStateGet(char *pcWriteBuffer,
                                                 size_t xWriteBufferLen,
                                                 const char *pcCommandString)
{
    int pwr_last_state = 0;

    /* pwr last state test */
    es_get_som_pwr_last_state(&pwr_last_state);

    snprintf(pcWriteBuffer, xWriteBufferLen, "pwr_last_state: %d\r\n", pwr_last_state);

    return pdFALSE;
}
```

**Function Flow**:

1. **Initialize Variable**: `int pwr_last_state = 0;`
   - Stores retrieved power state
   - 0 = power off, 1 = power on

2. **Retrieve from EEPROM**: `es_get_som_pwr_last_state(&pwr_last_state);`
   - Function defined in `hf_common.c` (from patch 0071)
   - Reads `SomPwrMgtDIPInfo` structure from EEPROM
   - Extracts `som_pwr_last_state` field

3. **Format Output**: `snprintf(pcWriteBuffer, xWriteBufferLen, ...)`
   - Writes response to CLI buffer
   - Format: `"pwr_last_state: 0\r\n"` or `"pwr_last_state: 1\r\n"`

4. **Return**: `return pdFALSE;`
   - Indicates command complete, no more output

**EEPROM Data Structure** (from patch 0071):

```c
typedef struct {
    uint8_t som_pwr_lost_resume_attr;  // Power loss resume enable/disable
    uint8_t som_pwr_last_state;        // Last known power state (0=off, 1=on)
    uint8_t som_dip_switch_soft_ctl_attr;
    uint8_t som_dip_switch_soft_state;
} SomPwrMgtDIPInfo;

// Stored in EEPROM at offset:
#define SOM_PWRMGT_DIP_INFO_EEPROM_OFFSET  (MCU_SERVER_INFO_EEPROM_OFFSET + sizeof(MCUServerInfo))
```

**Read Implementation** (from `hf_common.c`):

```c
int es_get_som_pwr_last_state(int *pwr_last_state)
{
    if (NULL == pwr_last_state)
        return -1;

    esENTER_CRITICAL(gEEPROM_Mutex, portMAX_DELAY);
    *pwr_last_state = gSOM_PwgMgtDIP_Info.som_pwr_last_state;
    esEXIT_CRITICAL(gEEPROM_Mutex);

    return 0;
}
```

**Use Cases**:

**1. Unexpected Power Loss Detection**:
```bash
# Before power cycling SoM:
BMC> pwrlast-g
pwr_last_state: 1

# After power cycle:
BMC> pwrlast-g
pwr_last_state: 1  # Expected if graceful shutdown

# If shows 0, indicates unexpected power loss or BMC crash
```

**2. Production Testing**:
```bash
# Test power state persistence:
BMC> power on
# Wait for SoM to boot
BMC> pwrlast-g
pwr_last_state: 1  # Confirms EEPROM write on power-on

BMC> power off
# Wait for shutdown
BMC> pwrlast-g
pwr_last_state: 0  # Confirms EEPROM write on power-off
```

**3. Debugging Power Resume Issues**:
```bash
# Check if power loss resume should have triggered:
BMC> pwrlast-g
pwr_last_state: 1

BMC> powerlost-g
Som Power Lost Attr: Enable

# If both are true, BMC should auto-power-on after reboot
# If not powered on, indicates bug in resume logic
```

### 4. EEPROM Write Protection Control

**Function**: `prvCommandCBWpEEPROM()` (lines 1343-1371)

```c
/**
* @brief CarrierBoard EEPROM Write protect eeprom or disable write protect
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandCBWpEEPROM(char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString)
{
    const char *cEEPROM_WP;
    BaseType_t xParamLen;
    uint8_t WP_En;

    cEEPROM_WP = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);
    WP_En = atoi(cEEPROM_WP);

    /* change write protect pin value */
    if (!WP_En) {
        HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_RESET);
        snprintf(pcWriteBuffer, xWriteBufferLen, "EEPROM Write protect disabled!\r\n");
    }
    else {
        HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_SET);
        snprintf(pcWriteBuffer, xWriteBufferLen, "EEPROM Write protect enabled!\r\n");
    }

    return pdFALSE;
}
```

**Function Flow**:

1. **Parse Parameter**:
   ```c
   cEEPROM_WP = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);
   ```
   - Extracts first parameter from command line
   - `pcCommandString`: Full command string (`"eepromwp-s 1"`)
   - Parameter 1: The argument after command name (`"1"`)
   - `xParamLen`: Length of parameter string

2. **Convert to Integer**:
   ```c
   WP_En = atoi(cEEPROM_WP);
   ```
   - Converts string to integer
   - `"0"` → 0 (disable protection)
   - `"1"` → 1 (enable protection)
   - Invalid input (e.g., `"abc"`) → 0 (atoi returns 0 on error)

3. **Control GPIO Pin**:
   ```c
   if (!WP_En) {  // WP_En == 0 (disable protection)
       HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_RESET);
       // GPIO_PIN_RESET = LOW = 0V → writes ENABLED
   }
   else {  // WP_En == 1 (enable protection)
       HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_SET);
       // GPIO_PIN_SET = HIGH = 3.3V → writes DISABLED
   }
   ```

4. **User Feedback**:
   ```c
   snprintf(pcWriteBuffer, xWriteBufferLen, "EEPROM Write protect %s!\r\n",
            WP_En ? "enabled" : "disabled");
   ```

**GPIO Pin Configuration** (from IOC file or board definition):

```c
// Defined in main.h or board-specific header:
#define EEPROM_WP_Pin GPIO_PIN_12  // Example: PB12
#define EEPROM_WP_GPIO_Port GPIOB

// Pin initialization (in MX_GPIO_Init):
GPIO_InitTypeDef GPIO_InitStruct = {0};
GPIO_InitStruct.Pin = EEPROM_WP_Pin;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;  // Push-pull output
GPIO_InitStruct.Pull = GPIO_NOPULL;          // No pull-up/down
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; // Low speed sufficient
HAL_GPIO_Init(EEPROM_WP_GPIO_Port, &GPIO_InitStruct);

// Default state: write enabled
HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_RESET);
```

**Hardware Schematic**:

```
STM32F407
  GPIO PB12 ──────┬─────── AT24C256 WP (Pin 7)
                  │
                 [10kΩ pull-down resistor] ← Optional, ensures LOW on reset
                  │
                 GND

When PB12 = LOW:  WP = 0V → Writes enabled
When PB12 = HIGH: WP = 3.3V → Writes disabled
```

**Usage Examples**:

**Example 1: Enable Protection**
```bash
BMC> eepromwp-s 1
EEPROM Write protect enabled!

BMC> # Attempt to write EEPROM
BMC> cbinfo-s ...
Error: EEPROM write failed (hardware write-protected)
```

**Example 2: Disable Protection for Reprogramming**
```bash
BMC> eepromwp-s 0
EEPROM Write protect disabled!

BMC> # Now can reprogram EEPROM
BMC> cbinfo-s "new_serial_number" 02:00:00:00:00:01 ...
CarrierBoard info set successfully

BMC> # Re-enable protection
BMC> eepromwp-s 1
EEPROM Write protect enabled!
```

**Example 3: Verify Protection Status**
```bash
# No direct "get" command, but can verify by attempting write:
BMC> eepromwp-s 1
EEPROM Write protect enabled!

BMC> cbinfo-s "test" ...
Error: EEPROM write failed

# If write succeeds, protection not active
# If write fails, protection is active
```

### 5. Error Handling Considerations

**Missing in Current Implementation**:

1. **No Parameter Validation**:
   ```c
   // Current:
   WP_En = atoi(cEEPROM_WP);

   // Improved:
   if (cEEPROM_WP == NULL) {
       snprintf(pcWriteBuffer, xWriteBufferLen, "Error: Missing parameter\r\n");
       return pdFALSE;
   }

   WP_En = atoi(cEEPROM_WP);
   if (WP_En != 0 && WP_En != 1) {
       snprintf(pcWriteBuffer, xWriteBufferLen, "Error: Invalid value (use 0 or 1)\r\n");
       return pdFALSE;
   }
   ```

2. **No GPIO Write Verification**:
   ```c
   // Current:
   HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_SET);

   // Improved:
   HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_SET);

   // Read back to verify
   GPIO_PinState actual_state = HAL_GPIO_ReadPin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin);
   if (actual_state != GPIO_PIN_SET) {
       snprintf(pcWriteBuffer, xWriteBufferLen, "Error: GPIO write failed!\r\n");
       return pdFALSE;
   }
   ```

3. **No EEPROM Write Test**:
   ```c
   // After enabling protection, verify it works:
   uint8_t test_data[4] = {0xAA, 0xBB, 0xCC, 0xDD};
   int ret = hf_i2c_mem_write(&hi2c1, AT24C_ADDR, TEST_OFFSET, test_data, 4);

   if (ret == 0) {
       // Write succeeded - WP not working!
       printf("Warning: Write protection may not be functional!\n");
   }
   ```

## Testing and Validation

### Functional Tests

**Test 1: Basic WP Enable/Disable**
```bash
# Test enable
BMC> eepromwp-s 1
EEPROM Write protect enabled!

# Test disable
BMC> eepromwp-s 0
EEPROM Write protect disabled!

# Test invalid parameter
BMC> eepromwp-s abc
EEPROM Write protect disabled!  # atoi("abc") = 0
```

**Test 2: Write Protection Verification**
```bash
# Enable protection
BMC> eepromwp-s 1
EEPROM Write protect enabled!

# Attempt to modify carrier board info
BMC> cbinfo-s "TEST_SN_123" 02:00:00:00:00:99
Error: Failed to write EEPROM  # Expected

# Disable protection
BMC> eepromwp-s 0
EEPROM Write protect disabled!

# Retry same write
BMC> cbinfo-s "TEST_SN_123" 02:00:00:00:00:99
Success  # Expected

# Verify data was written
BMC> cbinfo
SN: TEST_SN_123
MAC3: 02:00:00:00:00:99
```

**Test 3: Power Last State**
```bash
# Initial state
BMC> pwrlast-g
pwr_last_state: 0

# Power on SoM
BMC> power on
Power on sequence started...

# Check state (should update)
BMC> pwrlast-g
pwr_last_state: 1

# Power off SoM
BMC> power off
Power off sequence started...

# Check state (should update)
BMC> pwrlast-g
pwr_last_state: 0
```

**Test 4: Power Loss Resume Detection**
```bash
# Setup: Enable power loss resume
BMC> powerlost-s 1
Som Power Lost Attr: Enable

# Power on SoM
BMC> power on

# Verify last state
BMC> pwrlast-g
pwr_last_state: 1

# Simulate unexpected power loss: reboot BMC
BMC> reset

# After reboot, check if auto-powered-on
BMC> power status
Power Status: ON  # Expected (resumed from last state)

BMC> pwrlast-g
pwr_last_state: 1  # Preserved across reboot
```

### Production Test Procedure

**Complete Manufacturing Test Sequence**:

```bash
# 1. Connect to BMC console (115200 8N1)
# 2. Verify BMC booted successfully
BMC> version
BMC Firmware v1.2
Build: Jun 8 2024

# 3. Disable write protection (if previously enabled)
BMC> eepromwp-s 0
EEPROM Write protect disabled!

# 4. Program carrier board information
BMC> cbinfo-s "HF106C0001234567" 02:00:11:22:33:44 02:00:11:22:33:45 02:00:11:22:33:46
CarrierBoard info set successfully

# 5. Verify programmed data
BMC> cbinfo
magicNumber: 0x45505ef1
SN: HF106C0001234567
ethernetMAC1: 02:00:11:22:33:44
ethernetMAC2: 02:00:11:22:33:45
ethernetMAC3: 02:00:11:22:33:46
checksum: 0x12345678  # CRC32 calculated automatically

# 6. Test power state functionality
BMC> power on
BMC> pwrlast-g
pwr_last_state: 1
BMC> power off
BMC> pwrlast-g
pwr_last_state: 0

# 7. Enable write protection
BMC> eepromwp-s 1
EEPROM Write protect enabled!

# 8. Verify protection active
BMC> cbinfo-s "SHOULD_FAIL" ...
Error: Failed to write EEPROM  # Expected

# 9. Final verification
BMC> cbinfo
# Verify original data still present

# 10. Mark test complete
# Production system ships with EEPROM write-protected
```

### Error Scenarios

**Scenario 1: Forgetting to Disable WP Before Reprogramming**
```bash
# Attempt to reprogram without disabling WP
BMC> cbinfo-s "NEW_SERIAL" ...
Error: Failed to write EEPROM

# Solution:
BMC> eepromwp-s 0
EEPROM Write protect disabled!

BMC> cbinfo-s "NEW_SERIAL" ...
Success
```

**Scenario 2: WP Pin Not Connected**
```bash
# Enable WP
BMC> eepromwp-s 1
EEPROM Write protect enabled!

# Attempt write (succeeds even though WP "enabled")
BMC> cbinfo-s "TEST" ...
Success  # Unexpected!

# Indicates hardware issue:
# - WP pin not connected to EEPROM
# - GPIO not configured correctly
# - EEPROM does not support WP
```

**Scenario 3: Invalid EEPROM Address**
```bash
# Disable WP
BMC> eepromwp-s 0
EEPROM Write protect disabled!

# Attempt to write to invalid address
BMC> devmem-w 0xDEADBEEF 0x12345678
Error: I2C timeout or NACK

# WP state doesn't affect invalid operations
```

## Security Analysis

### Attack Surface

**Threat Model**:

**Attacker Profile 1: Physical Access**
- **Capability**: Serial console access, hardware probing
- **Goal**: Modify EEPROM data (serial numbers, MAC addresses)
- **Attack Path**:
  1. Connect to UART3 console
  2. Issue `eepromwp-s 0` command
  3. Reprogram EEPROM with `cbinfo-s`
  4. Re-enable WP to hide tracks

**Attacker Profile 2: Network Access**
- **Capability**: Web interface or SSH (if enabled)
- **Goal**: Execute console commands remotely
- **Attack Path**:
  1. Exploit web vulnerabilities to gain command execution
  2. Cannot directly access console commands (separate interface)
  3. **Mitigation**: Console commands not exposed via web

**Attacker Profile 3: Firmware Modification**
- **Capability**: Reflash BMC firmware
- **Goal**: Bypass WP protection entirely
- **Attack Path**:
  1. Extract firmware, modify WP control code
  2. Reflash modified firmware via SWD/JTAG
  3. WP permanently disabled in software
  4. **Mitigation**: Requires physical access, firmware signing would prevent

### Vulnerabilities

**1. Hidden Command is Not Secure**

**Issue**: Empty help string provides minimal security
```c
{"eepromwp-s", "", prvCommandCBWpEEPROM, 1}
```

**Why Weak**:
- Command still registered and functional
- Can be discovered by:
  - Reading source code
  - Reverse engineering binary
  - Brute-forcing command names
  - Social engineering production personnel

**Proof of Concept**:
```bash
# Attacker tries common command patterns:
BMC> eeprom-wp 1
Unknown command

BMC> eepromwp 1
Unknown command

BMC> eepromwp-s 1
EEPROM Write protect enabled!  # Discovered!
```

**Recommendation**:
```c
// Add authentication layer
static bool wp_authenticated = false;

static BaseType_t prvCommandWPAuth(...)
{
    const char *password = FreeRTOS_CLIGetParameter(pcCommandString, 1, &len);
    uint32_t hash = hash_password(password);

    if (hash == STORED_WP_PASSWORD_HASH) {
        wp_authenticated = true;
        start_timer(WP_AUTH_TIMEOUT_MS);
        printf("WP commands unlocked\n");
    }
}

static BaseType_t prvCommandCBWpEEPROM(...)
{
    if (!wp_authenticated) {
        printf("Error: Unauthorized\n");
        return pdFALSE;
    }
    // ... proceed
}
```

**2. No Audit Logging**

**Issue**: WP state changes not logged

**Impact**:
- Cannot detect unauthorized WP disable
- Cannot trace who modified EEPROM
- No forensic evidence

**Recommendation**:
```c
static BaseType_t prvCommandCBWpEEPROM(...)
{
    // ... WP control code

    // Log state change
    struct rtc_time_t timestamp;
    es_get_rtc_time(&timestamp);

    log_event(EVENT_EEPROM_WP_CHANGE,
              "WP %s at %04d-%02d-%02d %02d:%02d:%02d",
              WP_En ? "enabled" : "disabled",
              timestamp.year, timestamp.month, timestamp.day,
              timestamp.hour, timestamp.minute, timestamp.second);

    // Optional: send alert to remote syslog
}
```

**3. Race Condition Risk**

**Issue**: Multiple tasks could access EEPROM during WP disable

**Scenario**:
```
Task A: eepromwp-s 0 → WP disabled
Task B: Reads EEPROM (legitimate)
Task C: Malicious write attempt
Task A: eepromwp-s 1 → WP re-enabled
```

**Result**: Task C write may succeed during window

**Mitigation**:
```c
static SemaphoreHandle_t wp_mutex;

static BaseType_t prvCommandCBWpEEPROM(...)
{
    xSemaphoreTake(wp_mutex, portMAX_DELAY);

    // WP control
    HAL_GPIO_WritePin(...);

    // Keep mutex until operation complete
    // (caller must release after EEPROM write)
}

// EEPROM write functions:
int eeprom_write_protected(...)
{
    // Assume wp_mutex already taken
    // Perform write
    // Release mutex on completion
    xSemaphoreGive(wp_mutex);
}
```

**4. Physical Bypass**

**Issue**: WP pin can be overridden with hardware

**Attack**:
```
Attacker with physical access:
1. Locate EEPROM WP pin (Pin 7)
2. Cut trace to BMC GPIO
3. Connect WP pin to GND (force LOW)
4. EEPROM now writable regardless of software state
```

**Mitigation**:
- Epoxy potting of PCB
- Tamper-evident enclosure
- Regular physical inspections
- Security screws / anti-tamper seals

## Production Line Integration

### Automated Test Equipment (ATE)

**Example Python Test Script**:

```python
import serial
import time

class BMC_Tester:
    def __init__(self, port, baudrate=115200):
        self.ser = serial.Serial(port, baudrate, timeout=2)
        time.sleep(0.5)  # Wait for BMC boot

    def send_command(self, cmd):
        self.ser.write(f"{cmd}\r\n".encode())
        time.sleep(0.1)
        response = self.ser.read_all().decode()
        return response

    def disable_write_protect(self):
        resp = self.send_command("eepromwp-s 0")
        assert "disabled" in resp.lower(), "Failed to disable WP"

    def enable_write_protect(self):
        resp = self.send_command("eepromwp-s 1")
        assert "enabled" in resp.lower(), "Failed to enable WP"

    def program_eeprom(self, serial_number, mac1, mac2, mac3):
        cmd = f'cbinfo-s "{serial_number}" {mac1} {mac2} {mac3}'
        resp = self.send_command(cmd)
        assert "success" in resp.lower(), "EEPROM programming failed"

    def verify_eeprom(self, expected_sn):
        resp = self.send_command("cbinfo")
        assert expected_sn in resp, f"SN mismatch: {resp}"

    def test_write_protect(self):
        # Attempt write with protection enabled
        resp = self.send_command('cbinfo-s "SHOULD_FAIL" ...')
        assert "error" in resp.lower() or "fail" in resp.lower(), \
               "Write protection not working!"

# Production test sequence
def production_test(serial_port, unit_serial, mac_base):
    tester = BMC_Tester(serial_port)

    try:
        # 1. Disable WP
        tester.disable_write_protect()

        # 2. Program EEPROM
        mac1 = f"{mac_base}:01"
        mac2 = f"{mac_base}:02"
        mac3 = f"{mac_base}:03"
        tester.program_eeprom(unit_serial, mac1, mac2, mac3)

        # 3. Verify
        tester.verify_eeprom(unit_serial)

        # 4. Enable WP
        tester.enable_write_protect()

        # 5. Test WP
        tester.test_write_protect()

        print(f"✓ Unit {unit_serial} PASSED")
        return True

    except AssertionError as e:
        print(f"✗ Unit {unit_serial} FAILED: {e}")
        return False

# Run test
if __name__ == "__main__":
    production_test("/dev/ttyUSB0",
                     "HF106C0001234567",
                     "02:00:11:22:33")
```

### Database Integration

**Production Data Tracking**:

```sql
CREATE TABLE bmc_production (
    id SERIAL PRIMARY KEY,
    serial_number VARCHAR(32) UNIQUE NOT NULL,
    mac_address_1 VARCHAR(17) NOT NULL,
    mac_address_2 VARCHAR(17) NOT NULL,
    mac_address_3 VARCHAR(17) NOT NULL,
    test_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    test_status VARCHAR(10),  -- 'PASS' or 'FAIL'
    wp_enabled BOOLEAN DEFAULT false,
    crc32_checksum VARCHAR(10),
    operator_id VARCHAR(32),
    test_log TEXT
);

-- Insert production data after successful test
INSERT INTO bmc_production (serial_number, mac_address_1, mac_address_2,
                             mac_address_3, test_status, wp_enabled,
                             crc32_checksum, operator_id)
VALUES ('HF106C0001234567', '02:00:11:22:33:01', '02:00:11:22:33:02',
        '02:00:11:22:33:03', 'PASS', true, '0x12345678', 'operator123');
```

## Related Commands and Workflows

**Complementary Commands**:

1. **cbinfo / cbinfo-s**: Read/write carrier board info
   - Depends on WP state for write operations
   - Protected by `eepromwp-s` mechanism

2. **powerlost-g / powerlost-s**: Power loss resume control
   - Related to `pwrlast-g` functionality
   - Both stored in same EEPROM region

3. **devmem-w**: Direct memory/register write
   - Could theoretically write to EEPROM I2C registers
   - Also blocked by hardware WP

**Workflow Integration**:

```bash
# Complete field service workflow
1. Diagnose issue
   BMC> pwrlast-g  # Check if power loss occurred
   BMC> cbinfo     # Verify EEPROM integrity

2. If EEPROM corruption detected:
   BMC> eepromwp-s 0              # Disable WP
   BMC> cbinfo-s <correct_data>   # Reprogram
   BMC> cbinfo                    # Verify
   BMC> eepromwp-s 1              # Re-enable WP

3. Document repair in RMA system
```

## Future Enhancements

### 1. Read WP State

**Missing Feature**: No command to query current WP state

**Proposed Implementation**:
```c
static BaseType_t prvCommandCBWpEEPROMGet(...)
{
    GPIO_PinState wp_state = HAL_GPIO_ReadPin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin);

    snprintf(pcWriteBuffer, xWriteBufferLen,
             "EEPROM Write protect: %s\r\n",
             (wp_state == GPIO_PIN_SET) ? "enabled" : "disabled");

    return pdFALSE;
}

// Add to command table:
{"eepromwp-g", "", prvCommandCBWpEEPROMGet, 0}
```

### 2. Timed WP Disable

**Use Case**: Temporarily disable WP, auto-re-enable after operation

**Implementation**:
```c
static TimerHandle_t wp_timer;

static void wp_timer_callback(TimerHandle_t xTimer)
{
    // Auto-enable WP after timeout
    HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_SET);
    printf("EEPROM WP auto-enabled (timeout)\n");
}

static BaseType_t prvCommandCBWpEEPROMTimed(...)
{
    uint32_t timeout_sec = atoi(param);  // e.g., "60" for 60 seconds

    // Disable WP
    HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_RESET);

    // Start auto-enable timer
    xTimerChangePeriod(wp_timer, pdMS_TO_TICKS(timeout_sec * 1000), 0);
    xTimerStart(wp_timer, 0);

    snprintf(pcWriteBuffer, xWriteBufferLen,
             "EEPROM WP disabled for %u seconds\r\n", timeout_sec);
    return pdFALSE;
}
```

### 3. WP Lock Counting

**Track WP disable events**:
```c
static uint32_t wp_disable_count = 0;

static BaseType_t prvCommandCBWpEEPROM(...)
{
    if (!WP_En) {
        wp_disable_count++;
        // Store count in non-volatile storage
        save_wp_count_to_eeprom(wp_disable_count);
    }
    // ...
}

static BaseType_t prvCommandWPCount(...)
{
    snprintf(pcWriteBuffer, xWriteBufferLen,
             "EEPROM WP disabled %u times since manufacturing\r\n",
             wp_disable_count);
    return pdFALSE;
}
```

## Conclusion

Patch 0080 adds **critical production line tools** for EEPROM management:

**eepromwp-s Command**:
- Hardware-based write protection control
- Hidden from help menu (security through obscurity)
- Essential for manufacturing process
- Prevents accidental EEPROM corruption

**pwrlast-g Command**:
- Diagnostic tool for power state tracking
- Helps detect unexpected power losses
- Supports power resume debugging
- Useful for field service

**Production Readiness**: HIGH
- Well-integrated with existing EEPROM infrastructure
- Simple, reliable GPIO control
- Proven hardware WP mechanism (AT24Cxx standard)

**Security Consideration**: MODERATE
- Hidden command provides minimal security
- Requires physical/console access to exploit
- Recommend adding authentication for production
- Audit logging would improve traceability

**Manufacturing Impact**: CRITICAL
- Essential for production line workflow
- Protects factory-programmed data
- Reduces RMA rate from corruption
- Industry-standard practice

This patch complements patch 0079 (SiFive CRC32) by providing hardware protection for the data validated by software checksums, creating a defense-in-depth strategy for EEPROM integrity.
