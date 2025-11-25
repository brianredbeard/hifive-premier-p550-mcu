# Patch 0033: CLI Console on UART3 (CRITICAL)

## Patch Metadata

**Patch File:** `0033-WIN2030-15099-feat-cli-Add-console-on-uart3.patch`
**Issue Tracker:** WIN2030-15099
**Type:** Feature Addition (feat)
**Component:** CLI (Command-Line Interface)
**Author:** yuan junhui <yuanjunhui@eswincomputing.com>
**Date:** Wed, 15 May 2024 20:14:46 +0800
**Change-ID:** I0e0c04e2e5e0a5b7c8f7f0c4d0f9e7f8f0e0c0d0

## Changelogs

1. Add console on UART3
2. Delete debug prints
3. Update UART3 tasks

## Executive Summary

**CRITICAL FEATURE:** This patch implements a parallel CLI (Command-Line Interface) console on UART3, providing an independent management and debugging interface for the BMC firmware. This is a foundational feature that enables direct hardware-level access and control through a serial terminal, essential for development, debugging, production testing, and field diagnostics.

### Why This Is CRITICAL

1. **Out-of-Band Management**: Provides console access independent of network connectivity
2. **Production Testing**: Enables factory test procedures and validation
3. **Emergency Recovery**: Allows BMC control when web interface is unavailable
4. **Security Research**: Provides direct access for firmware analysis and debugging
5. **Development Tool**: Essential for firmware development and debugging workflows

### Impact Analysis

**Scope:** High - Creates entirely new management interface
**Risk:** Low - Additive change, doesn't modify existing functionality
**Complexity:** Medium - Requires FreeRTOS task management and UART integration
**Security:** Medium - New attack surface through serial console (physical access required)

## Technical Overview

### Architecture Changes

This patch adds a complete CLI subsystem running on UART3, parallel to any existing console infrastructure. The implementation uses FreeRTOS tasks and the FreeRTOS+CLI command framework.

**Key Components:**

1. **UART3 Hardware Interface**
   - Uses STM32F4 UART3 peripheral
   - Hardware abstraction through HAL (Hardware Abstraction Layer)
   - Interrupt-driven receive, polled transmit

2. **FreeRTOS CLI Task**
   - Dedicated task: `ConsoleTask3()`
   - Processes incoming commands from UART3
   - Uses FreeRTOS+CLI command registration system
   - Handles command parsing, execution, output formatting

3. **Command Registration**
   - Integrates with existing command table
   - Commands available: RTC control, PVT info, coreboot info, etc.

### Files Modified

1. **Core/Inc/hf_common.h** - Header file for common hardware abstractions
2. **Core/Src/console.c** - Console implementation and task management
3. **Core/Src/hf_common.c** - Common utility functions
4. **Core/Src/main.c** - Main application initialization

## Detailed Code Analysis

### 1. Core/Inc/hf_common.h Changes

**Location:** Lines added to header file

**Change:**
```c
+extern UART_HandleTypeDef huart3;
```

**Analysis:**

This exposes the UART3 handle globally, making it accessible to the console subsystem. The `UART_HandleTypeDef` is an STM32 HAL structure containing:
- UART peripheral base address
- Initialization parameters (baud rate, word length, stop bits, parity)
- State information (busy, ready, error flags)
- DMA handles (if used)

**Design Pattern:** The `extern` declaration allows the console code to reference the UART3 handle initialized in the CubeMX-generated code (likely in `main.c` or `stm32f4xx_hal_msp.c`).

**Hardware Mapping:**
- UART3 TX typically on PB10 or PC10 (verify in IOC file)
- UART3 RX typically on PB11 or PC11
- Configuration: 115200 baud, 8N1 (standard for embedded consoles)

### 2. Core/Src/console.c Changes - Part A: Console Task Implementation

**Location:** New function `ConsoleTask3()` added

**Complete Implementation:**

```c
+void ConsoleTask3(void *argument)
+{
+  /* USER CODE BEGIN ConsoleTask3 */
+  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(500);
+  char cRxedChar;
+  uint8_t cInputIndex = 0;
+  BaseType_t xMoreDataToFollow;
+  static char cInputString[cmdMAX_INPUT_SIZE], cOutputString[cmdMAX_OUTPUT_SIZE];
+
+  /* Infinite loop */
+  for (;;)
+  {
+    HAL_UART_Receive(&huart3, (uint8_t *)&cRxedChar, sizeof(cRxedChar), 100);
+
+    if (cRxedChar == '\r' || cRxedChar == '\n')
+    {
+      HAL_UART_Transmit(&huart3, (uint8_t *)"\r\n", 2, 100);
+
+      do
+      {
+        xMoreDataToFollow = FreeRTOS_CLIProcessCommand(cInputString, cOutputString, cmdMAX_OUTPUT_SIZE);
+
+        HAL_UART_Transmit(&huart3, (uint8_t *)cOutputString, strlen(cOutputString), 500);
+
+      } while (xMoreDataToFollow != pdFALSE);
+
+      cInputIndex = 0;
+      memset(cInputString, 0x00, cmdMAX_INPUT_SIZE);
+      memset(cOutputString, 0x00, cmdMAX_OUTPUT_SIZE);
+
+      HAL_UART_Transmit(&huart3, (uint8_t *)pcEndOfOutputMessage, strlen(pcEndOfOutputMessage), 100);
+    }
+    else
+    {
+      if (cRxedChar == '\b')
+      {
+        if (cInputIndex > 0)
+        {
+          cInputIndex--;
+          cInputString[cInputIndex] = '\0';
+          HAL_UART_Transmit(&huart3, (uint8_t *)"\b \b", 3, 100);
+        }
+      }
+      else
+      {
+        if (cInputIndex < cmdMAX_INPUT_SIZE)
+        {
+          cInputString[cInputIndex] = cRxedChar;
+          cInputIndex++;
+          HAL_UART_Transmit(&huart3, (uint8_t *)&cRxedChar, sizeof(cRxedChar), 100);
+        }
+      }
+    }
+  }
+  /* USER CODE END ConsoleTask3 */
+}
```

#### Detailed Line-by-Line Analysis

**Variable Declarations:**

```c
const TickType_t xMaxBlockTime = pdMS_TO_TICKS(500);
```
- **Purpose:** Maximum time to block waiting for UART data (500ms)
- **FreeRTOS Macro:** `pdMS_TO_TICKS()` converts milliseconds to RTOS ticks
- **Note:** Currently unused in this implementation (timeout passed directly to HAL functions)

```c
char cRxedChar;
```
- **Purpose:** Holds single character received from UART
- **Type:** `char` (8-bit)
- **Scope:** Receives one byte at a time from serial input

```c
uint8_t cInputIndex = 0;
```
- **Purpose:** Tracks current position in input buffer
- **Range:** 0 to `cmdMAX_INPUT_SIZE - 1`
- **Reset:** Cleared to 0 after command execution

```c
BaseType_t xMoreDataToFollow;
```
- **Purpose:** Indicates if FreeRTOS+CLI has more output to send
- **Values:** `pdTRUE` (more data) or `pdFALSE` (complete)
- **Usage:** Drives the `do-while` loop for multi-line command output

```c
static char cInputString[cmdMAX_INPUT_SIZE];
static char cOutputString[cmdMAX_OUTPUT_SIZE];
```
- **Purpose:** Buffers for command input and output
- **Static:** Persists across function calls, but local to this task
- **Size:** `cmdMAX_INPUT_SIZE` typically 50-100 bytes, `cmdMAX_OUTPUT_SIZE` typically 512-1024 bytes
- **Memory:** Allocated on task stack (ensure stack size sufficient in task creation)

#### Main Loop - Character Reception

```c
HAL_UART_Receive(&huart3, (uint8_t *)&cRxedChar, sizeof(cRxedChar), 100);
```

**Analysis:**
- **Function:** STM32 HAL blocking UART receive
- **Parameters:**
  - `&huart3`: UART3 handle (global, initialized by CubeMX)
  - `(uint8_t *)&cRxedChar`: Destination buffer (single byte)
  - `sizeof(cRxedChar)`: Number of bytes to receive (1)
  - `100`: Timeout in milliseconds
- **Behavior:**
  - Blocks for up to 100ms waiting for character
  - Returns `HAL_OK` on success, `HAL_TIMEOUT` if no data
  - **Design Choice:** 100ms timeout prevents task from monopolizing CPU
  - **Impact:** Console responsive but not CPU-intensive

**Performance Consideration:** This polling approach is simple but not optimal. An interrupt-driven circular buffer would be more efficient, but this works well for low-rate console input.

#### Command Termination Handling

```c
if (cRxedChar == '\r' || cRxedChar == '\n')
```

**Analysis:**
- **Trigger:** Carriage return (CR, `\r`, 0x0D) or line feed (LF, `\n`, 0x0A)
- **Purpose:** Detect end of command line
- **Compatibility:** Handles both Unix (`\n`) and Windows (`\r\n`) line endings
- **Terminal Behavior:** Most terminal programs send `\r` on Enter key

```c
HAL_UART_Transmit(&huart3, (uint8_t *)"\r\n", 2, 100);
```

**Analysis:**
- **Purpose:** Echo newline to terminal (move cursor to next line)
- **Sequence:** `\r\n` (CRLF) - standard for serial terminals
- **Timeout:** 100ms - reasonable for small data

#### Command Processing Loop

```c
do
{
    xMoreDataToFollow = FreeRTOS_CLIProcessCommand(cInputString, cOutputString, cmdMAX_OUTPUT_SIZE);
    HAL_UART_Transmit(&huart3, (uint8_t *)cOutputString, strlen(cOutputString), 500);
} while (xMoreDataToFollow != pdFALSE);
```

**Deep Analysis:**

**FreeRTOS_CLIProcessCommand():**
- **Purpose:** Parse and execute command from `cInputString`
- **Operation:**
  1. Tokenizes input string (space-delimited)
  2. Matches first token against registered command table
  3. Calls command handler function with parameters
  4. Handler writes output to `cOutputString`
  5. Returns `pdTRUE` if more output remains, `pdFALSE` if complete
- **Multi-Line Output:** Some commands (e.g., `help`, file listings) produce output larger than buffer
  - First call: Returns first chunk, sets `xMoreDataToFollow = pdTRUE`
  - Subsequent calls: Returns next chunk
  - Final call: Returns last chunk, sets `xMoreDataToFollow = pdFALSE`

**Transmit:**
- **Timeout:** 500ms (longer than receive, accommodates large output)
- **Blocking:** Task blocked during transmit (acceptable for console)
- **strlen():** Calculates actual output length (null-terminated string)

**Design Pattern:** The `do-while` loop ensures at least one execution (even if command produces no output), then repeats while more output pending.

#### Buffer Cleanup

```c
cInputIndex = 0;
memset(cInputString, 0x00, cmdMAX_INPUT_SIZE);
memset(cOutputString, 0x00, cmdMAX_OUTPUT_SIZE);
```

**Analysis:**
- **Purpose:** Reset for next command
- **cInputIndex:** Reset to beginning of buffer
- **memset():** Zero entire buffers (security best practice - clear sensitive data)
- **Performance:** `memset()` overhead acceptable given low console command rate

#### Prompt Display

```c
HAL_UART_Transmit(&huart3, (uint8_t *)pcEndOfOutputMessage, strlen(pcEndOfOutputMessage), 100);
```

**Analysis:**
- **pcEndOfOutputMessage:** Defined in FreeRTOS+CLI, typically `"\r\n> "` or similar prompt
- **Purpose:** Signal to user that BMC is ready for next command
- **User Experience:** Standard CLI pattern (prompt after command completion)

#### Backspace Handling

```c
else
{
    if (cRxedChar == '\b')
    {
        if (cInputIndex > 0)
        {
            cInputIndex--;
            cInputString[cInputIndex] = '\0';
            HAL_UART_Transmit(&huart3, (uint8_t *)"\b \b", 3, 100);
        }
    }
```

**Deep Analysis:**

**Backspace Character:**
- **Value:** `\b` (0x08, ASCII backspace)
- **Source:** Terminal programs send this when user presses backspace/delete key

**Logic:**
1. **Bounds Check:** `if (cInputIndex > 0)` - prevents underflow
2. **Decrement Index:** Move back one position
3. **Null Termination:** `cInputString[cInputIndex] = '\0'` - properly terminate string
4. **Visual Feedback:** `"\b \b"` sequence:
   - `\b`: Move cursor left
   - ` ` (space): Overwrite character with space
   - `\b`: Move cursor left again (position for next character)
   - **Result:** Character visually erased from terminal

**Edge Case Handling:**
- If `cInputIndex == 0`, backspace ignored (nothing to delete)
- No error message (standard shell behavior)

#### Normal Character Input

```c
    else
    {
        if (cInputIndex < cmdMAX_INPUT_SIZE)
        {
            cInputString[cInputIndex] = cRxedChar;
            cInputIndex++;
            HAL_UART_Transmit(&huart3, (uint8_t *)&cRxedChar, sizeof(cRxedChar), 100);
        }
    }
}
```

**Analysis:**

**Buffer Overflow Protection:**
- **Check:** `if (cInputIndex < cmdMAX_INPUT_SIZE)`
- **Purpose:** Prevent buffer overflow attack or accidental overrun
- **Behavior:** If buffer full, additional characters silently ignored
- **Security:** Critical protection against potential exploit via serial input

**Character Storage and Echo:**
1. **Store:** `cInputString[cInputIndex] = cRxedChar`
2. **Increment:** `cInputIndex++` (post-increment pattern)
3. **Echo:** Transmit character back to terminal
   - **Purpose:** User sees what they type (local echo)
   - **Standard:** Most serial consoles expect echo from device

**User Experience:**
- Characters appear as typed
- Backspace removes characters
- Enter executes command
- Buffer limit prevents runaway input

### 2. Core/Src/console.c Changes - Part B: Command Registration

**Location:** Addition to CLI command registration table

**Change Added:**
```c
+	{
+		"cbinfo",
+		"\r\ncbinfo <s>:\r\n Get coreboot-table info \r\n Usage: cbinfo s\r\n",
+		prvCommandCBInfo,
+		1
+	},
```

**Analysis:**

This registers the `cbinfo` command with FreeRTOS+CLI.

**Structure Fields:**
1. **Command Name:** `"cbinfo"` - string user types to invoke command
2. **Help Text:** Multi-line description and usage
   - `\r\n` sequences for proper terminal formatting
   - Shows command syntax: `cbinfo <s>`
   - Documents parameter: `s` (likely "summary" or specific section)
3. **Handler Function:** `prvCommandCBInfo` - function called when command executed
4. **Parameter Count:** `1` - expects exactly one parameter

**Coreboot Integration:**
- **Purpose:** Retrieve coreboot firmware table information from EEPROM
- **Use Case:** Display bootloader version, build info, system configuration
- **Location:** Coreboot data stored in dedicated EEPROM region (defined in `hf_common.c`)

**Function Signature (typical):**
```c
static BaseType_t prvCommandCBInfo(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
```

**Implementation Location:** Defined elsewhere in `console.c` or `hf_common.c` (not shown in this patch, pre-existing)

### 3. Core/Src/hf_common.c Changes

**Location:** Line 26

**Changes:**
```c
+extern UART_HandleTypeDef huart3;
```

**Context:**
This file contains common utility functions used across the BMC firmware. The `extern` declaration here allows these utilities to use UART3 for debug output or console communication.

**Design Pattern:**
- Shared UART handle accessible to multiple modules
- Centralized hardware abstraction
- Enables consistent console output across firmware components

### 4. Core/Src/main.c Changes - Removed Debug Prints

**Multiple Locations Throughout File**

**Typical Removed Lines:**
```c
-  printf("test create task\n");
-  printf("test before task rtos\n");
```

**Analysis:**

**Purpose of Removal:**
1. **Code Cleanup:** Remove development/debug clutter
2. **Performance:** Eliminate unnecessary UART transmissions
3. **Production Readiness:** Clean output for end users
4. **Security:** Reduce information leakage through debug messages

**Impact:**
- Cleaner boot sequence (no extraneous messages)
- Slightly faster boot (fewer UART operations)
- More professional console output

**Best Practice:** This demonstrates proper transition from development to production:
- Debug prints used during development
- Removed or conditionally compiled (`#ifdef DEBUG`) for production

## UART3 Hardware Configuration

**Expected Configuration (verify in STM32F407VET6_BMC.ioc):**

```
UART3 Settings:
- Baud Rate: 115200
- Word Length: 8 bits
- Parity: None
- Stop Bits: 1
- Hardware Flow Control: None (CTS/RTS disabled)
- Mode: Asynchronous
- Pin Configuration:
  - TX: PB10 or PC10 (alternative functions)
  - RX: PB11 or PC11
- DMA: Disabled (polling mode)
- Interrupts: Disabled (polling mode in this implementation)
```

**Physical Connection:**
```
BMC (STM32F407)          USB-Serial Adapter
------------------------------------------
UART3 TX (PB10/PC10) --> RX
UART3 RX (PB11/PC11) <-- TX
GND                  <-> GND
```

**Terminal Settings:**
- Baud: 115200
- Data: 8 bits
- Parity: None
- Stop: 1 bit
- Flow Control: None
- Local Echo: Off (BMC echoes characters)

## FreeRTOS Task Integration

**Task Creation (expected in main.c or initialization code):**

```c
osThreadDef(ConsoleTask3, ConsoleTask3, osPriorityNormal, 0, 256);
ConsoleTask3Handle = osThreadCreate(osThread(ConsoleTask3), NULL);
```

**Task Parameters:**
- **Name:** ConsoleTask3
- **Function:** ConsoleTask3()
- **Priority:** osPriorityNormal (medium priority)
  - Lower than critical hardware tasks
  - Higher than background/idle tasks
  - Appropriate for interactive console
- **Instances:** 0 (no multi-instance support needed)
- **Stack Size:** 256 words (1024 bytes on ARM Cortex-M4)
  - **Adequacy:** Sufficient for local buffers and HAL stack usage
  - **Consideration:** If `cmdMAX_OUTPUT_SIZE` large, may need increase

**Task Lifecycle:**
1. **Creation:** During FreeRTOS scheduler start (after hardware init)
2. **Execution:** Enters infinite loop immediately
3. **Blocking:** Blocks on UART receive (100ms timeout)
4. **Preemption:** Can be preempted by higher priority tasks
5. **Termination:** Never (infinite loop)

**Concurrency Considerations:**
- **UART Access:** Only this task uses UART3 (no mutex needed)
- **Shared Data:** Commands may access shared structures (must use mutexes/semaphores)
- **Stack Safety:** Local buffers on stack (task-private)

## Available Commands (Expected After This Patch)

Based on FreeRTOS+CLI integration and coreboot reference, likely available commands:

### System Information Commands

**cbinfo [s]** - Coreboot information
```
Usage: cbinfo s
Output: Displays coreboot table data from EEPROM
        - Magic number verification
        - Coreboot version
        - Build timestamp
        - System configuration
        - 's' parameter may show summary or specific section
```

**help** - Command list (FreeRTOS+CLI standard)
```
Usage: help
Output: Lists all registered commands with descriptions
```

### RTC Commands (referenced in command registration)

**rtc** - Real-Time Clock operations
```
Usage: rtc [get|set] [parameters]
Examples:
  rtc get          - Display current date/time
  rtc set YYYY-MM-DD HH:MM:SS  - Set RTC
```

### PVT Information (Process-Voltage-Temperature)

**pvt** - Display PVT sensor readings
```
Usage: pvt
Output: Temperature, voltage, process corner information
```

### Additional Commands (Typical for BMC Console)

**power** - Power control
```
Usage: power [on|off|status]
Examples:
  power on      - Power on SoM
  power off     - Power off SoM
  power status  - Display current power state
```

**network** - Network configuration
```
Usage: network [show|set]
Examples:
  network show  - Display current IP, netmask, gateway
  network set <ip> <netmask> <gateway>
```

**version** - Firmware version
```
Usage: version
Output: BMC firmware version, build date
```

## Command Implementation Example

**Typical Command Handler (hypothetical RTC command):**

```c
static BaseType_t prvCommandRTC(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    const char *pcParameter;
    BaseType_t xParameterStringLength;

    // Get first parameter (get/set)
    pcParameter = FreeRTOS_CLIGetParameter(
        pcCommandString,    // Full command string
        1,                  // Parameter number (1-indexed)
        &xParameterStringLength
    );

    if (strncmp(pcParameter, "get", xParameterStringLength) == 0)
    {
        RTCInfo rtcInfo;
        get_rtcinfo(&rtcInfo);

        snprintf(pcWriteBuffer, xWriteBufferLen,
                 "RTC: %04d-%02d-%02d %02d:%02d:%02d\r\n",
                 rtcInfo.year, rtcInfo.month, rtcInfo.date,
                 rtcInfo.hours, rtcInfo.minutes, rtcInfo.seconds);
    }
    else if (strncmp(pcParameter, "set", xParameterStringLength) == 0)
    {
        // Parse date/time parameters
        // Call set_rtcinfo()
        snprintf(pcWriteBuffer, xWriteBufferLen, "RTC updated\r\n");
    }
    else
    {
        snprintf(pcWriteBuffer, xWriteBufferLen, "Usage: rtc [get|set]\r\n");
    }

    return pdFALSE;  // No more output
}
```

## Security Implications

### Attack Surface Analysis

**Physical Access Required:**
- UART3 is a physical serial port on the carrier board
- Requires physical proximity to device or serial-over-LAN (if implemented)
- **Mitigation:** Physical access implies high privilege; acceptable risk

**Authentication:**
- **Current State:** No authentication on serial console (typical for embedded BMC)
- **Justification:** Physical access control is primary security boundary
- **Alternative:** Could implement login prompt (like Linux serial console)

**Command Injection:**
- **Risk:** Malformed commands could exploit parser vulnerabilities
- **Protection:** Buffer overflow protected by `cmdMAX_INPUT_SIZE` check
- **FreeRTOS+CLI:** Mature library, extensively tested
- **Recommendation:** Audit command handler functions for input validation

**Information Disclosure:**
- Commands expose system internals (memory, registers, configuration)
- **Impact:** Physical attacker gains detailed system knowledge
- **Acceptable:** Standard for debug/management console

**Privilege Escalation:**
- Console has full BMC privileges (can control power, access EEPROM, etc.)
- **Risk:** If serial port accessible remotely, serious vulnerability
- **Mitigation:** Ensure UART3 not exposed via network (e.g., serial-over-IP)

### Security Best Practices

1. **Physical Security:** Restrict access to serial port
2. **Production Disable:** Consider disabling console in production builds (compile-time option)
3. **Logging:** Log all console commands for audit trail
4. **Rate Limiting:** Prevent console command flooding (not implemented here)
5. **Timeout:** Auto-logout after inactivity (not implemented here)

## Testing and Validation

### Functional Testing

**Test 1: Basic Connectivity**
```bash
# Connect to UART3 with terminal emulator
screen /dev/ttyUSB0 115200

# Expected: Console prompt appears
>

# Test: Type characters, verify echo
> test
test

# Test: Backspace functionality
> hello[backspace][backspace]lo
helo
```

**Test 2: Command Execution**
```bash
# Test help command
> help
help:
 Lists all the registered commands

cbinfo <s>:
 Get coreboot-table info
 Usage: cbinfo s

[... more commands ...]

>

# Test cbinfo command
> cbinfo s
Coreboot Table Info:
  Magic: 0x12345678
  Version: 1.0
  Build: 2024-05-15
  ...

>
```

**Test 3: Buffer Overflow Protection**
```bash
# Attempt to overflow input buffer
> [type 200+ characters without pressing Enter]
# Expected: Characters stop echoing after cmdMAX_INPUT_SIZE
# System remains stable, no crash
```

**Test 4: Multi-Line Output**
```bash
# Command with large output (help, or verbose info)
> help
# Expected: Output appears in chunks if exceeds cmdMAX_OUTPUT_SIZE
# All output eventually displayed
```

### Performance Testing

**Response Time:**
- Measure time from Enter key to first output character
- Expected: <10ms (depends on command complexity)

**Character Echo Latency:**
- Type rapidly, verify all characters echo without loss
- Expected: No dropped characters at normal typing speed

**Task Scheduling:**
- Verify console task doesn't starve other tasks
- Monitor CPU usage during console interaction
- Expected: <5% CPU during idle, <20% during active command

### Stress Testing

**Rapid Commands:**
```bash
# Script to send commands rapidly
for i in {1..100}; do
  echo "cbinfo s" > /dev/ttyUSB0
  sleep 0.1
done

# Expected: All commands execute, no crashes or hangs
```

**Invalid Input:**
```bash
# Send binary data, control characters
> [Ctrl+C][Ctrl+D][ESC sequences]
# Expected: Graceful handling, no crash
```

## Production Deployment Considerations

### Build Configuration

**Debug vs. Production:**
```c
// In FreeRTOSConfig.h or similar

#ifdef PRODUCTION_BUILD
    #define ENABLE_UART3_CONSOLE 0  // Disable in production
#else
    #define ENABLE_UART3_CONSOLE 1  // Enable for development
#endif
```

**Conditional Compilation:**
```c
#if ENABLE_UART3_CONSOLE
    osThreadDef(ConsoleTask3, ConsoleTask3, osPriorityNormal, 0, 256);
    ConsoleTask3Handle = osThreadCreate(osThread(ConsoleTask3), NULL);
#endif
```

### Resource Allocation

**Memory Footprint:**
- Task stack: 1024 bytes
- Input buffer: ~50-100 bytes (static)
- Output buffer: ~512-1024 bytes (static)
- Code size: ~2-3 KB (task + command handlers)
- **Total:** ~4-5 KB RAM, ~3 KB Flash

**UART Peripheral:**
- UART3 reserved for console (cannot be used for other purposes)
- GPIO pins (TX/RX) dedicated

### Documentation for End Users

**User Manual Entry (recommended):**

```markdown
## Serial Console Access

The BMC provides a serial console interface on UART3 for management and debugging.

### Connection
1. Connect USB-to-serial adapter:
   - Adapter RX → BMC UART3 TX (PB10)
   - Adapter TX → BMC UART3 RX (PB11)
   - GND → GND

2. Configure terminal:
   - Baud: 115200
   - Data: 8 bits
   - Parity: None
   - Stop: 1 bit
   - Flow Control: None

3. Open terminal:
   ```bash
   screen /dev/ttyUSB0 115200
   # or
   minicom -D /dev/ttyUSB0 -b 115200
   ```

### Available Commands
- `help` - List all commands
- `cbinfo s` - Display coreboot information
- `rtc get` - Show current date/time
- `power status` - Display power state
- [... more commands ...]

### Tips
- Press Enter to execute command
- Use Backspace to correct typos
- Type `help` for command list
```

## Integration with Existing Systems

### Web Server Integration

Console commands can trigger same backend functions as web API:

```c
// Web server power control
void web_server_power_on_callback(...)
{
    power_on_sequence();  // Shared function
}

// Console power control
static BaseType_t prvCommandPower(...)
{
    power_on_sequence();  // Same shared function
}
```

**Benefits:**
- Code reuse
- Consistent behavior across interfaces
- Simplified testing (test once, works in both)

### Daemon Integration

Console can query daemon status:

```c
// Console status command
static BaseType_t prvCommandStatus(...)
{
    extern struct bmc_status g_bmc_status;  // Daemon-maintained

    snprintf(pcWriteBuffer, xWriteBufferLen,
             "Voltage: %u mV\r\nCurrent: %d mA\r\nPower: %u mW\r\n",
             g_bmc_status.bus_voltage_mv,
             g_bmc_status.current_ma,
             g_bmc_status.power_mw);

    return pdFALSE;
}
```

**Synchronization:** Use FreeRTOS mutexes if console modifies shared data.

## Comparison with Patch 0039 (devmem Commands)

**Relationship:**
- **Patch 0033 (this):** Creates console infrastructure
- **Patch 0039:** Adds `devmem-r` and `devmem-w` commands to console

**Foundation for Future Commands:**
This patch establishes the framework that enables patch 0039 and future command additions. Any new command only requires:
1. Implement handler function
2. Register in command table
3. Rebuild firmware

## Troubleshooting Guide

### Issue: No Console Output

**Symptoms:**
- Terminal shows no prompt
- Typed characters don't echo

**Diagnosis:**
1. Verify physical connections (TX/RX not swapped, GND connected)
2. Check baud rate matches (115200 on both sides)
3. Confirm UART3 initialized (check IOC file, verify huart3 handle)
4. Verify task created (add breakpoint in ConsoleTask3)
5. Check power to serial adapter

**Resolution:**
- Swap TX/RX if needed
- Reconfigure terminal settings
- Verify firmware flashed correctly

### Issue: Garbled Output

**Symptoms:**
- Random characters or symbols instead of readable text

**Diagnosis:**
- Baud rate mismatch (most common cause)
- Wrong data format (e.g., 7N1 instead of 8N1)
- Hardware issue (noise, poor connection)

**Resolution:**
- Double-check terminal baud rate (115200)
- Verify terminal settings (8N1, no flow control)
- Try different USB-serial adapter
- Shorter cable (reduce noise)

### Issue: Commands Don't Execute

**Symptoms:**
- Characters echo correctly
- Pressing Enter does nothing

**Diagnosis:**
1. Line ending mismatch (terminal sending `\n`, code expects `\r`)
2. Command not registered in table
3. FreeRTOS+CLI not initialized

**Resolution:**
- Configure terminal to send CR (`\r`) or CRLF (`\r\n`)
- Verify command in registration table
- Check FreeRTOS+CLI initialization in startup code

### Issue: System Hangs After Command

**Symptoms:**
- Command starts executing, then system freezes

**Diagnosis:**
- Command handler infinite loop or deadlock
- Stack overflow (command handler uses too much stack)
- Mutex deadlock (command waiting for locked resource)

**Resolution:**
- Debug command handler function
- Increase task stack size
- Review mutex usage, ensure proper release

## Future Enhancements

### Suggested Improvements

1. **Interrupt-Driven UART:**
   - Replace polling with interrupt + circular buffer
   - Reduces CPU usage, improves responsiveness
   - More complex implementation

2. **Command History:**
   - Up/down arrow to recall previous commands
   - Requires terminal emulation (ANSI escape sequences)
   - Significant code addition

3. **Tab Completion:**
   - Press Tab to auto-complete command names
   - Improves user experience
   - Moderate complexity

4. **Authentication:**
   - Login prompt before accepting commands
   - Username/password stored in EEPROM
   - Increases security for production use

5. **Remote Console:**
   - Telnet or SSH server for network-based console
   - Eliminates need for physical serial connection
   - Security concerns (encryption, authentication critical)

6. **Command Scripting:**
   - Execute batch of commands from EEPROM or web upload
   - Useful for automated testing, configuration
   - Requires script parser

## Conclusion

Patch 0033 is a **foundational, CRITICAL feature** that establishes a robust serial console interface on UART3. This enables:

- **Development:** Essential tool for firmware development and debugging
- **Production:** Factory testing, field diagnostics, emergency recovery
- **Security Research:** Direct hardware access for analysis
- **Integration:** Framework for future command additions (e.g., devmem in patch 0039)

The implementation follows embedded systems best practices:
- FreeRTOS task architecture for concurrency
- Buffer overflow protection
- Standard CLI patterns (echo, backspace, prompts)
- Integration with existing FreeRTOS+CLI framework

**Impact:** High value, low risk. Sets the stage for rich command-line management ecosystem.

**Recommendation:** Essential for any deployment requiring out-of-band management or development access.
