# Patch 0020: Web Command Support for Power-Off - Thread-Safe State Management

## Metadata
- **Patch File**: `0020-WIN2030-15099-refactor-bmc-bmc-web-cmd-support-power.patch`
- **Author**: huangyifeng <huangyifeng@eswincomputing.com>
- **Date**: Mon, 13 May 2024 14:27:50 +0800
- **Ticket**: WIN2030-15099
- **Type**: refactor (Code restructuring and feature addition)
- **Change-Id**: I0677ba956827830f4ecc93e45d37ac2ba1b1cd41

## Summary

This patch completes the web command support infrastructure (started in patch 0018) by adding critical thread-safe state management, implementing graceful power-off functionality from the web interface, and establishing a robust SoM daemon status tracking system. The patch introduces critical section protection for shared state variables, implements a watchdog timer for power-off timeout enforcement, and adds notification message handling for coordinated shutdown between BMC and SoC. This represents a major advancement in BMC-SoC communication reliability and web interface control capabilities.

## Changelog

The official changelog from the commit message:
1. **Support poweroff cmd from web** - Web interface can now safely trigger graceful SoM shutdown
2. **Support to get SOM daemon status** - Track whether SoM daemon process is running and responsive
3. **Add critical section protection for som_power_state and som_daemon_state** - Thread-safe access to shared state variables preventing race conditions

## Statistics

- **Files Changed**: 5 files
- **Insertions**: 130 lines
- **Deletions**: 37 lines
- **Net Change**: +93 lines

## Detailed Changes by File

### 1. Common Header - API Additions (`Core/Inc/hf_common.h`)

#### Changes: Exported State Variables and New Accessor Functions

**Location**: Lines 29-50, 190-197

**Removed**:
```c
extern power_switch_t som_power_state;  // Global direct access removed
```

**Added**:
```c
// New thread-safe accessor functions
power_switch_t get_som_power_state(void);
void change_som_power_state(power_switch_t newState);
deamon_stats_t get_som_daemon_state(void);
void change_som_daemon_state(deamon_stats_t newState);

void TriggerSomTimer(void);
```

**Analysis**:

**Major Architectural Change**: Transition from direct global variable access to accessor functions

**Before Patch** (Unsafe):
```c
// Any file could directly access
extern power_switch_t som_power_state;

// In multiple threads:
if (som_power_state == SOM_POWER_ON) {  // Thread 1 reading
    som_power_state = SOM_POWER_OFF;     // Thread 2 writing - RACE CONDITION!
}
```

**After Patch** (Safe):
```c
// All access through functions
power_switch_t state = get_som_power_state();      // Thread-safe read
change_som_power_state(SOM_POWER_OFF);            // Thread-safe write
```

**Why This Matters - Race Condition Example**:

**Scenario Without Protection**:
```
Time | Web Handler Thread        | Power Management Thread
-----|---------------------------|---------------------------
T0   | Read som_power_state (ON) |
T1   |                           | Read som_power_state (ON)
T2   | Decide to power off       |
T3   |                           | Check if ON, decide to keep on
T4   | Write som_power_state=OFF |
T5   |                           | Write som_power_state=ON (overwrites!)
T6   | POWER STILL ON!           | Expects power on
     | Web says "powered off"    | Power task says "powered on"
     | STATE INCONSISTENCY!      |
```

**With Critical Section Protection**:
```
Time | Web Handler Thread                    | Power Management Thread
-----|---------------------------------------|---------------------------
T0   | change_som_power_state(OFF)           |
T1   |   taskENTER_CRITICAL()               |
T2   |   som_power_state = OFF               |
T3   |                                       | get_som_power_state()
T4   |                                       |   taskENTER_CRITICAL() <- BLOCKS
T5   |   taskEXIT_CRITICAL()                 |
T6   |                                       |   <- Unblocks, reads OFF
T7   |                                       |   taskEXIT_CRITICAL()
     | CONSISTENT STATE!                     | Sees correct state
```

**New Functions Explained**:

1. **get_som_power_state()**: Thread-safe read of power state
   - Returns current power state atomically
   - Prevents reading partial state during writes

2. **change_som_power_state()**: Thread-safe write of power state
   - Atomically updates power state
   - Prevents concurrent modifications

3. **get_som_daemon_state()**: Thread-safe read of SoM daemon status
   - Tracks if SoM daemon responsive
   - Used to detect SoM crashes or hangs

4. **change_som_daemon_state()**: Thread-safe write of daemon status
   - Updates daemon state atomically
   - Called when daemon heartbeat succeeds/fails

5. **TriggerSomTimer()**: Start power-off timeout watchdog
   - Ensures power-off completes within time limit
   - Prevents hung shutdown sequences

**Thread-Safe Design Pattern**: Getter/Setter with Critical Sections

This is a **critical section mutual exclusion pattern**:
```c
volatile power_switch_t som_power_state;  // Shared resource

power_switch_t get_som_power_state(void) {
    power_switch_t state;
    taskENTER_CRITICAL();  // Disable interrupts
    state = som_power_state;
    taskEXIT_CRITICAL();   // Re-enable interrupts
    return state;
}

void change_som_power_state(power_switch_t newState) {
    taskENTER_CRITICAL();
    som_power_state = newState;
    taskEXIT_CRITICAL();
}
```

**Why `volatile` Keyword**:
```c
volatile power_switch_t som_power_state;
```

`volatile` tells compiler:
- Variable may change outside normal program flow (interrupts, other threads)
- Don't optimize reads/writes (always access memory)
- Don't cache value in registers

Without `volatile`:
```c
// Compiler might optimize to:
if (som_power_state == ON) {     // Read once
    // Use cached value
    doSomething();
    if (som_power_state == ON) { // Compiler uses cached value, doesn't re-read!
        // ...
    }
}
```

With `volatile`:
```c
// Compiler forced to re-read each time:
if (som_power_state == ON) {     // Read from memory
    doSomething();
    if (som_power_state == ON) { // Read from memory again (sees latest value)
        // ...
    }
}
```

### 2. GPIO Processing - Use Thread-Safe Accessors (`Core/Src/hf_gpio_process.c`)

#### Changes: Replace Direct Access with Accessor Functions

**Location**: Lines 56, 64, 65, 73, 74 (multiple occurrences)

**Before**:
```c
} else if ((som_power_state == SOM_POWER_OFF) && (currentTime - pressStartTime >= PRESS_Time)) {
    button_state = KEY_SHORT_PRESS_STATE;
}

// ...

if (som_power_state != SOM_POWER_ON) {
    som_power_state = SOM_POWER_ON;
}

// ...

if (som_power_state == SOM_POWER_ON) {
    som_power_state = SOM_POWER_OFF;
}
```

**After**:
```c
} else if ((get_som_power_state() == SOM_POWER_OFF) && (currentTime - pressStartTime >= PRESS_Time)) {
    button_state = KEY_SHORT_PRESS_STATE;
}

// ...

if (get_som_power_state() != SOM_POWER_ON) {
    change_som_power_state(SOM_POWER_ON);
}

// ...

if (get_som_power_state() == SOM_POWER_ON) {
    change_som_power_state(SOM_POWER_OFF);
}
```

**Analysis**:

**GPIO Button Handler Context**:

This code runs in the **GPIO processing task** which handles physical power button presses. The button task must coordinate with the power management task through shared state.

**Race Condition Scenario** (Before Patch):

```
Scenario: User presses power button while web interface also triggering power-off

Time | Button Task                        | Web Handler Task
-----|-----------------------------------|----------------------------------
T0   | Detect button press               |
T1   | Check: som_power_state == ON      | User clicks "Power Off"
T2   | Decision: Will power off          |
T3   |                                   | Check: som_power_state == ON
T4   |                                   | Decision: Will power off
T5   | Write: som_power_state = OFF      |
T6   |                                   | Write: som_power_state = OFF
T7   | Trigger power-off sequence        | Trigger power-off sequence
T8   | BOTH TASKS TRIGGER POWER-OFF!     | (Duplicate power-off attempts)
```

**Result**:
- Power-off triggered twice
- Potential state machine confusion
- Debug messages duplicated
- Resource cleanup attempted twice

**With Critical Sections** (After Patch):

```
Time | Button Task                                | Web Handler Task
-----|-------------------------------------------|----------------------------------
T0   | Detect button press                        |
T1   | get_som_power_state() [CRITICAL]           |
T2   |   Returns ON, exits critical               |
T3   | Decision: Will power off                   | User clicks "Power Off"
T4   | change_som_power_state(OFF) [CRITICAL]     |
T5   |   Enters critical, sets OFF, exits         |
T6   |                                            | get_som_power_state() [CRITICAL]
T7   |                                            |   Sees OFF (already changed!)
T8   |                                            | Decision: Already off, skip
T9   | Trigger power-off (ONCE)                   | Print "already off"
```

**Result**:
- Only one task triggers power-off
- Clean state transitions
- No duplicate operations

**Button State Machine Context**:

The button processing includes debouncing and press duration detection:

```c
case KEY_PRESS_STATE:
    currentTime = xTaskGetTickCount();
    pin_state = get_key_state();
    if (pin_state == KEY_RELEASE) {
        button_state = KEY_RELEASE_DETECTED_STATE;
    } else if (currentTime - pressStartTime > LONG_PRESS_THRESHOLD) {
        button_state = KEY_LONG_PRESS_STATE;
    } else if ((get_som_power_state() == SOM_POWER_OFF) &&
               (currentTime - pressStartTime >= PRESS_Time)) {
        button_state = KEY_SHORT_PRESS_STATE;
    }
    break;

case KEY_SHORT_PRESS_STATE:
    printf("KEY_SHORT_PRESS_STATE time %ld\n", currentTime - pressStartTime);
    button_state = KEY_PRESS_STATE_END;
    if (get_som_power_state() != SOM_POWER_ON) {
        change_som_power_state(SOM_POWER_ON);  // Request power-on
    }
    break;

case KEY_LONG_PRESS_STATE:
    printf("KEY_LONG_PRESS_STATE time %ld\n", currentTime - pressStartTime);
    button_state = KEY_PRESS_STATE_END;
    if (get_som_power_state() == SOM_POWER_ON) {
        change_som_power_state(SOM_POWER_OFF);  // Request power-off
    }
    break;
```

**Button Behavior**:
- **Short press** (<5 seconds): Toggle power state (typically used when SoM off)
- **Long press** (>5 seconds): Force power-off (emergency shutdown)

**Thread Safety Critical Here Because**:
- Button task and web server task both can modify power state
- Power management task reads power state to know what to do
- Physical button should always work (higher priority than web)

### 3. Power Process - Critical Section Protection and State Accessors (`Core/Src/hf_power_process.c`)

#### Change 1: Make som_power_state Volatile

**Location**: Line 89

**Before**:
```c
power_switch_t som_power_state = SOM_POWER_OFF;
```

**After**:
```c
volatile power_switch_t som_power_state = SOM_POWER_OFF;
```

**Analysis**:

**`volatile` Keyword Critical for Thread Safety**

As explained earlier, `volatile` prevents compiler optimizations that assume variable doesn't change unexpectedly. Essential for variables accessed by multiple threads or interrupts.

#### Change 2: Implement Thread-Safe Accessor Functions

**Location**: Lines 98-115

**Added**:
```c
power_switch_t get_som_power_state(void)
{
    power_switch_t state;

    taskENTER_CRITICAL();
    state = som_power_state;
    taskEXIT_CRITICAL();
    return state;
}

void change_som_power_state(power_switch_t newState)
{
    // Enter critical section to ensure thread safety when traversing and deleting
    taskENTER_CRITICAL();
    som_power_state = newState;
    taskEXIT_CRITICAL();
}
```

**Analysis**:

**FreeRTOS Critical Sections**:

`taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()` macro pair:

**On ARM Cortex-M (STM32F407)**:
```c
#define taskENTER_CRITICAL()   portDISABLE_INTERRUPTS()
#define taskEXIT_CRITICAL()    portENABLE_INTERRUPTS()

// Which expands to:
#define portDISABLE_INTERRUPTS()  __asm volatile("cpsid i")  // Disable interrupts
#define portENABLE_INTERRUPTS()   __asm volatile("cpsie i")  // Enable interrupts
```

**What Happens**:
1. **taskENTER_CRITICAL()**: Executes `CPSID I` instruction (Clear PRIMASK, Disable Interrupts)
   - All interrupts (except hard faults and NMI) disabled
   - Prevents task switching (scheduler runs in interrupt context)
   - Current task has exclusive access to CPU

2. **Critical section code**: Executes atomically
   - No other task can run
   - No interrupts can occur
   - Variable access/modification atomic

3. **taskEXIT_CRITICAL()**: Executes `CPSIE I` instruction (Clear PRIMASK, Enable Interrupts)
   - Interrupts re-enabled
   - Pending interrupts serviced
   - Scheduler can switch tasks again

**Critical Section Duration**:

VERY IMPORTANT: Keep critical sections **SHORT**!

**Good** (this patch):
```c
taskENTER_CRITICAL();
state = som_power_state;  // ~1 CPU cycle
taskEXIT_CRITICAL();
// Total: ~10-20 CPU cycles, <1 microsecond
```

**Bad** (hypothetical):
```c
taskENTER_CRITICAL();
i2c_read(...);  // Takes milliseconds!
osDelay(10);    // Delays 10ms!
printf(...);    // UART transmission!
taskEXIT_CRITICAL();
// Total: >10ms with interrupts disabled - VERY BAD!
```

**Why Long Critical Sections Bad**:
- Interrupts disabled → no response to hardware events
- USB, Ethernet, UART data lost
- Watchdog may not refresh → system reset
- Real-time deadlines missed
- Other tasks starved

**Best Practices**:
1. Critical section ≤ 100 CPU cycles (prefer ≤ 10)
2. No function calls in critical sections (except inline)
3. No waiting (delays, I/O, mutexes)
4. Copy data out quickly, process outside critical section

#### Change 3: Use Accessor in AUTO_BOOT

**Location**: Lines 126-127

**Before**:
```c
#ifdef AUTO_BOOT
    power_state = ATX_PS_ON_STATE;
    som_power_state = SOM_POWER_ON;
#endif
```

**After**:
```c
#ifdef AUTO_BOOT
    power_state = ATX_PS_ON_STATE;
    change_som_power_state(SOM_POWER_ON);
#endif
```

**Analysis**:

Even during initialization, use accessor functions. This ensures:
- Consistent pattern throughout codebase
- If accessor adds logging/debug in future, AUTO_BOOT sees it too
- Prevents forgetting to use accessor when adding features

#### Change 4: Add Critical Section to Power Task Main Loop

**Location**: Lines 131, 151-152

**Added**:
```c
while (1) {
    taskENTER_CRITICAL();
    switch (power_state) {
    case ATX_PS_ON_STATE:
        // ... all state handling
    }
    taskEXIT_CRITICAL();
    osDelay(100);
}
```

**Analysis**:

**ENTIRE STATE MACHINE in Critical Section!**

This ensures:
- State transitions atomic
- No task switches during state processing
- Consistent view of all variables during state handling

**Important Design Decision**:

Each state's code is SHORT (mostly just setting GPIOs, checking pins), so acceptable to be in critical section.

**State Execution Time** (estimated):
- ATX_PS_ON_STATE: ~10 CPU cycles (set GPIO, change state)
- DC_PWR_GOOD_STATE: ~100-1000 cycles (loop, but exits critical section during osDelay!)

**Wait - osDelay() in Critical Section?**

Looking closely at the code:
```c
case DC_PWR_GOOD_STATE:
    printf("DC_PWR_GOOD_STATE\r\n");
    try_count = MAXTRYCOUNT;
    do
    {
        pin_state = get_dc_power_status();
        osDelay(100);  // This delays OUTSIDE the critical section!
    } while (pin_state != pdTRUE && try_count--);
```

The `osDelay(100)` is in the `do-while` loop, which is in the state case, which is in the critical section... **This is a BUG!**

**Actual Behavior**:

`osDelay()` internally calls task yield, which tries to switch tasks. But with interrupts disabled (critical section), this likely causes:
- osDelay() spins instead of yielding
- 100ms busy-wait with interrupts disabled
- **VERY BAD for system responsiveness!**

**Should Be**:
```c
case DC_PWR_GOOD_STATE:
    taskEXIT_CRITICAL();  // Exit critical section before delay!
    printf("DC_PWR_GOOD_STATE\r\n");
    try_count = MAXTRYCOUNT;
    do {
        pin_state = get_dc_power_status();
        osDelay(100);
    } while (pin_state != pdTRUE && try_count--);
    taskENTER_CRITICAL();  // Re-enter before state transition
    if(try_count <= 0) {
        power_state = STOP_POWER;
    }
    if (pin_state == pdTRUE) {
        i2c_init(I2C3);
        i2c_init(I2C1);
        power_state = SOM_STATUS_CHECK_STATE;
    }
    break;
```

**Conclusion**: The critical section placement appears to be **overly broad**. Later patches likely refine this.

### 4. Protocol Processing - Daemon State Management and Notification Handling (`Core/Src/hf_protocol_process.c`)

This file contains the MOST significant changes in the patch, implementing SoM daemon tracking, notification message handling, and power-off timeout mechanism.

#### Change 1: Add Notification Message Type

**Location**: Line 171

**Before**:
```c
typedef enum {
    MSG_REQUEST = 0x01,
    MSG_REPLY,
} MsgType;
```

**After**:
```c
typedef enum {
    MSG_REQUEST = 0x01,
    MSG_REPLY,
    MSG_NOTIFLY,  // New: Notification messages from SoM
} MsgType;
```

**Analysis**:

**Message Type Categories**:

1. **MSG_REQUEST** (0x01): BMC→SoM request
   - BMC sends command to SoM
   - SoM must respond
   - Example: "Get board status", "Read sensor"

2. **MSG_REPLY** (0x02): SoM→BMC response
   - SoM responds to BMC request
   - Contains result of command execution
   - Matches request by command ID

3. **MSG_NOTIFLY** (0x03): SoM→BMC notification (NEW!)
   - SoM sends unsolicited message to BMC
   - No request needed
   - Example: "I'm shutting down now", "Error occurred"

**Notification Use Case** - Graceful Power-Off:

```
1. Web interface → BMC: "Power off SoM"
2. BMC → SoM (via UART4): CMD_POWER_OFF request
3. SoM: Begins graceful shutdown:
   - Close files
   - Unmount filesystems
   - Stop processes
   - Prepare hardware for power-down
4. SoM → BMC (via UART4): MSG_NOTIFLY with CMD_POWER_OFF
   - "I've shut down cleanly, safe to cut power"
5. BMC: Receives notification
6. BMC: Cuts power to SoM
```

**Without Notification**:
- BMC would cut power immediately (file system corruption!)
- Or BMC waits fixed timeout (slow, unreliable)

**With Notification**:
- SoM controls shutdown timing
- BMC cuts power only when safe
- Fast (no unnecessary waiting)
- Reliable (SoM confirms shutdown complete)

#### Change 2: Improved Message Dump Function

**Location**: Lines 175-177

**Before**:
```c
void dump_message(Message data)
{
    printf("Header: 0x%lX, Cmd Type: 0x%x, Data Len: %d, Checksum: 0x%X, Tail: 0x%lx\n",
        data.header, data.cmd_type, data.data_len, data.checksum, data.tail);
}
```

**After**:
```c
void dump_message(Message data)
{
    printf("Header: 0x%lX, Msg_type %d, Cmd Type: 0x%x, Data Len: %d, Checksum: 0x%X, Tail: 0x%lx\n",
        data.header, data.msg_type, data.cmd_type, data.data_len, data.checksum, data.tail);
}
```

**Analysis**:

Added `msg_type` to debug output. Essential for debugging notification vs. reply messages.

Example output:
```
Header: 0xA55AAA55, Msg_type 3, Cmd Type: 0x01, Data Len: 0, Checksum: 0x5A, Tail: 0x0D0A0D0A
                    ^^^^^^^^^
                    MSG_NOTIFLY (3) - SoM sending notification
```

#### Change 3: SoM Timer Creation

**Location**: Line 184

**Added**:
```c
TimerHandle_t xSomTimer;
```

**Analysis**:

**FreeRTOS Software Timer** for power-off timeout enforcement.

**Software Timer Characteristics**:
- Runs in **timer service task** (not ISR)
- One-shot or auto-reload modes
- Callback function executed when timer expires
- Can be started, stopped, reset from any task

**Purpose Here**:
Ensure power-off completes within timeout period. If SoM doesn't send shutdown notification within 5 seconds, force power-off anyway.

#### Change 4: Thread-Safe SoM Daemon State Accessors

**Location**: Lines 192-213

**Before**:
```c
deamon_stats_t som_daemon_state = SOM_DAEMON_OFF;
```

**After**:
```c
volatile deamon_stats_t som_daemon_state = SOM_DAEMON_OFF;

deamon_stats_t get_som_daemon_state(void)
{
    deamon_stats_t state;

    // Enter critical section to ensure thread safety when traversing and deleting
    taskENTER_CRITICAL();
    state = som_daemon_state;
    taskEXIT_CRITICAL();

    return state;
}

void change_som_daemon_state(deamon_stats_t newState)
{
    // Enter critical section to ensure thread safety when traversing and deleting
    taskENTER_CRITICAL();
    som_daemon_state = newState;
    taskEXIT_CRITICAL();
    return ;
}
```

**Analysis**:

**Same thread-safe pattern** as power state accessors.

**SoM Daemon State** (`deamon_stats_t` enum):
```c
typedef enum {
    SOM_DAEMON_OFF,   // SoM daemon not running or not responding
    SOM_DAEMON_ON,    // SoM daemon responsive
} deamon_stats_t;
```

**Daemon Tracking Purpose**:
- Detects if SoM has crashed or hung
- Web interface can show "SoM responsive" vs "SoM not responding"
- Enables recovery actions (forced restart if daemon dead)

**Thread Safety Needed Because**:
- Daemon keep-alive task updates state periodically
- Web server reads state to display status
- Power management may check daemon state before power-off

#### Change 5: Update Daemon Keep-Alive Task with Accessors

**Location**: Lines 222-253

**Before**:
```c
for (;;) {
    if (SOM_POWER_ON != som_power_state) {
        osDelay(50);
        continue;
    }
    old_status = som_daemon_state;
    ret = web_cmd_handle(CMD_BOARD_STATUS, NULL, 0, 1000);
    if (HAL_OK != ret) {
        // ... error handling
        count++;
        if (5 >= count) {
            som_daemon_state = SOM_DAEMON_OFF;
        }
    } else {
        som_daemon_state = SOM_DAEMON_ON;
        count = 0;
    }
    if (old_status != som_daemon_state) {
        printf("SOM Daemon status change to %s!\n",
            som_daemon_state == SOM_DAEMON_ON ? "on" : "off");
    }
    /*check every 1 second*/
    osDelay(pdMS_TO_TICKS(4000));
}
```

**After**:
```c
for (;;) {
    if (SOM_POWER_ON != get_som_power_state()) {
        osDelay(50);
        continue;
    }
    old_status = get_som_daemon_state();
    ret = web_cmd_handle(CMD_BOARD_STATUS, NULL, 0, 1000);
    if (HAL_OK != ret) {
        // ... error handling
        count++;
        if (5 >= count) {
            change_som_daemon_state(SOM_DAEMON_OFF);
        }
    } else {
        change_som_daemon_state(SOM_DAEMON_ON);
        count = 0;
    }
    if (old_status != get_som_daemon_state()) {
        printf("SOM Daemon status change to %s!\n",
            get_som_daemon_state() == SOM_DAEMON_ON ? "on" : "off");
    }
    /*check every 4 second*/
    osDelay(pdMS_TO_TICKS(4000));
}
```

**Analysis**:

**Daemon Keep-Alive Algorithm**:

1. **Skip if SoM powered off**: No point checking daemon if SoM not running
2. **Send status request**: `web_cmd_handle(CMD_BOARD_STATUS, ...)`
   - Sends message to SoM daemon
   - Waits 1000ms for response
3. **Track failures**:
   - If timeout: increment `count`
   - If 5 consecutive failures: mark daemon OFF
4. **Track successes**:
   - If response received: mark daemon ON, reset count
5. **Report state changes**: Log when daemon goes on/off
6. **Repeat every 4 seconds**: Regular heartbeat checks

**Failure Tolerance**:

Allows up to 5 consecutive failures before declaring daemon dead:
```
Check 1: Timeout → count = 1 (daemon still ON)
Check 2: Timeout → count = 2 (daemon still ON)
Check 3: Timeout → count = 3 (daemon still ON)
Check 4: Timeout → count = 4 (daemon still ON)
Check 5: Timeout → count = 5 (daemon NOW OFF)
Check 6: Success → count = 0 (daemon ON again)
```

**Why Tolerate Failures**:
- Network/UART glitches transient
- SoM may be busy (loading spike)
- Prevents false alarms from single timeout

**Comment Note**: Changed from "check every 1 second" to "check every 4 second"
- Code always had 4000ms delay
- Comment corrected to match reality

#### Change 6: Implement Notification Message Handler

**Location**: Lines 256-265

**Added**:
```c
void handle_notify_mesage(Message *msg)
{
    if (CMD_POWER_OFF == msg->cmd_type) {
        // Here the SOM should at shutdown state in opensbi, we can turn off its power safely
        change_som_power_state(SOM_POWER_OFF);
        printf("Poweroff SOM normaly, shutdown it now!\n");
    }
}
```

**Analysis**:

**Graceful Shutdown Completion Handler**

This function executes when SoM sends `MSG_NOTIFLY` with `CMD_POWER_OFF`:

**Flow**:
```
1. SoM completes graceful shutdown (OpenSBI finished, filesystems unmounted)
2. SoM sends: MSG_NOTIFLY, CMD_POWER_OFF
3. BMC receives message
4. Protocol task calls handle_notify_mesage()
5. Function changes power state to OFF
6. Power management task sees state change, cuts power
```

**OpenSBI Reference**:

Comment mentions "shutdown state in opensbi" - **OpenSBI** (Open Source Supervisor Binary Interface):
- RISC-V firmware layer between hardware and operating system
- Provides power management, timer, IPI services
- When OS shuts down, returns control to OpenSBI
- OpenSBI performs final hardware shutdown steps
- Then notifies BMC (via this message)

**Thread Safety**:

Uses `change_som_power_state()` (thread-safe accessor) to update power state. Power management task polling `get_som_power_state()` will see change and execute power-off.

**Why "normaly" (typo in code)**:

"Poweroff SOM normaly" = "Poweroff SOM normally"

Indicates **graceful shutdown** (as opposed to forced/emergency power-off).

#### Change 7: Rename and Enhance Message Handler

**Location**: Lines 267-283

**Before**:
```c
void handle_deamon_mesage(Message *msg)
{
    if (MSG_REPLY == msg->msg_type) {
        // ... process replies
    } else {
        printf("Unsupport msg type: 0x%x\n", msg->cmd_type);
        buf_dump((uint8_t *)&msg, sizeof(msg));
        dump_message(*msg);
    }
}
```

**After**:
```c
void handle_som_mesage(Message *msg)  // Renamed from handle_deamon_mesage
{
    if (MSG_REPLY == msg->msg_type) {
        // ... process replies
    } else if (MSG_NOTIFLY == msg->msg_type) {
        handle_notify_mesage(msg);  // NEW: Handle notifications
    } else {
        printf("Unsupport msg type: 0x%x\n", msg->msg_type);  // Fixed: msg_type not cmd_type
        buf_dump((uint8_t *)msg, sizeof(*msg));               // Fixed: *msg not msg
        dump_message(*msg);
    }
}
```

**Analysis**:

**Three Improvements**:

1. **Function Rename**: `handle_deamon_mesage` → `handle_som_mesage`
   - More accurate name (handles all SoM messages, not just daemon)
   - Fixes typo ("deamon" → "daemon" implied)

2. **Add Notification Handling**:
   ```c
   } else if (MSG_NOTIFLY == msg->msg_type) {
       handle_notify_mesage(msg);
   ```
   - Routes notification messages to dedicated handler
   - Enables power-off notification processing

3. **Bug Fixes in Error Path**:
   - `msg->cmd_type` → `msg->msg_type` (print correct field)
   - `sizeof(msg)` → `sizeof(*msg)` (dump actual message, not pointer size)

**Message Routing**:
```
Message arrives from SoM
    ↓
handle_som_mesage()
    ↓
    ├─ MSG_REPLY → Find matching request in WebCmdList
    ├─ MSG_NOTIFLY → handle_notify_mesage() → process notification
    └─ Unknown → Dump and log error
```

#### Change 8: Implement SoM Timer Callback

**Location**: Lines 285-293

**Added**:
```c
void vSomTimerCallback(TimerHandle_t xSomTimer)
{
    if (SOM_POWER_OFF != get_som_power_state()) {
        change_som_power_state(SOM_POWER_OFF);
        printf("Poweroff SOM timeout, shutdown it now!\n");
    }
}
```

**Analysis**:

**Power-Off Timeout Enforcement**

This callback executes if SoM timer expires (5 seconds after power-off requested).

**Timeout Scenario**:
```
T=0s:  Web interface requests power-off
T=0s:  BMC sends CMD_POWER_OFF to SoM
T=0s:  BMC starts 5-second timer
T=0-5s: Waiting for SoM to send shutdown notification...
T=5s:  Timer expires, callback executes
T=5s:  Callback checks: if SoM still ON, force power-off
```

**Why Needed**:

**Problem Without Timeout**:
```
1. BMC sends power-off command to SoM
2. SoM daemon receives command
3. SoM daemon crashes during shutdown (bug!)
4. SoM never sends notification
5. BMC waits forever
6. User thinks power-off failed
7. Power still on (SoM hung, consuming power)
```

**Solution With Timeout**:
```
1. BMC sends power-off command to SoM
2. BMC starts 5-second timer
3. SoM daemon crashes (doesn't send notification)
4. Timer expires after 5 seconds
5. Callback forces power-off anyway
6. Power cut (even if SoM hung)
7. User sees power-off completed (maybe not graceful, but completed)
```

**Safety Check**:
```c
if (SOM_POWER_OFF != get_som_power_state()) {
```

Only force power-off if SoM still showing as ON:
- If SoM already sent notification, power state already OFF, no action needed
- If SoM hung, power state still ON, force OFF

**Timeout Duration**:

5 seconds chosen because:
- Normal shutdown: 0.5-2 seconds (plenty of margin)
- Slow shutdown (many processes): 3-4 seconds (still fits)
- Hung/crashed SoM: Never completes (timeout triggers)

**Message Comparison**:
- Normal: "Poweroff SOM normaly, shutdown it now!" (from notification)
- Timeout: "Poweroff SOM timeout, shutdown it now!" (from timer)

Allows distinguishing graceful vs. forced shutdown in logs.

#### Change 9: Implement Timer Trigger Function

**Location**: Lines 295-301

**Added**:
```c
void TriggerSomTimer(void)
{
    if (xTimerStart(xSomTimer, 0) != pdPASS) {
        printf("Failed to trigger Som timer!\n");
    }
}
```

**Analysis**:

**Timer Start Wrapper Function**

Starts the SoM power-off timeout timer.

**xTimerStart() Parameters**:
```c
xTimerStart(TimerHandle_t xTimer, TickType_t xTicksToWait)
```

- `xSomTimer`: Timer handle (created in protocol task init)
- `0`: Don't wait if timer command queue full (fail immediately)

**Return Value**:
- `pdPASS`: Timer start command successfully queued
- `pdFAIL`: Timer command queue full (shouldn't happen with 0 wait)

**Why Wrapper Function**:
- Consistent error handling
- Single place to change timer start logic
- Can add logging, statistics, etc.

**Called From**: Web server power-off handler (next section)

#### Change 10: Create and Initialize SoM Timer

**Location**: Lines 306-325

**Before**:
```c
void uart4_protocol_task(void *argument)
{
    Message msg;

    xUart4MsgQueue = xQueueCreate(QUEUE_LENGTH, sizeof(Message));
    if (xUart4MsgQueue == NULL) {
        printf("Failed to create msg queue!\n");
        return;
    }

    //Init web server cmd list
    vListInitialise(&WebCmdList);

    for (;;) {
        // ... message processing loop
    }
}
```

**After**:
```c
void uart4_protocol_task(void *argument)
{
    Message msg;

    xUart4MsgQueue = xQueueCreate(QUEUE_LENGTH, sizeof(Message));
    if (xUart4MsgQueue == NULL) {
        printf("[%s %d]:Failed to create SOM msg queue!\n",__func__,__LINE__);
        return;
    }

    //Init web server cmd list
    vListInitialise(&WebCmdList);

    /* Create a timer with a timeout set to 5 seconds */
    xSomTimer = xTimerCreate( "SomTimer", (5000 / portTICK_PERIOD_MS),
            pdFALSE, (void *)0, vSomTimerCallback);

    if (xSomTimer == NULL) {
        printf("[%s %d]:Failed to create SOM Timer!\n",__func__,__LINE__);
        return;
    }

    for (;;) {
        // ... message processing loop
    }
}
```

**Analysis**:

**SoM Timer Creation**:

```c
xTimerCreate(
    "SomTimer",                    // Timer name (for debugging)
    (5000 / portTICK_PERIOD_MS),   // Period: 5000ms = 5 seconds
    pdFALSE,                       // One-shot (not auto-reload)
    (void *)0,                     // Timer ID (not used here)
    vSomTimerCallback              // Callback function
)
```

**Timer Parameters Explained**:

1. **Name**: "SomTimer"
   - Used in FreeRTOS debug tools
   - Helpful when debugging timer issues

2. **Period**: `5000 / portTICK_PERIOD_MS`
   - `portTICK_PERIOD_MS`: Milliseconds per tick (typically 1ms with 1000 Hz tick rate)
   - `5000 / 1 = 5000 ticks` = 5 seconds
   - When timer started, expires after 5 seconds

3. **Auto-reload**: `pdFALSE`
   - One-shot timer (doesn't automatically restart)
   - Expires once, then stops
   - Perfect for timeout detection (want single timeout event)

4. **Timer ID**: `(void *)0`
   - User-definable value passed to callback
   - Not used here (callback doesn't need context)
   - Could be used to identify which timer in shared callback

5. **Callback**: `vSomTimerCallback`
   - Function called when timer expires
   - Runs in timer service task context (not ISR)

**Error Handling Improvements**:

Enhanced error messages:
```c
printf("[%s %d]:Failed to create SOM msg queue!\n",__func__,__LINE__);
```

Uses `__func__` (function name) and `__LINE__` (line number) macros:
- Example output: `[uart4_protocol_task 310]:Failed to create SOM msg queue!`
- Easier debugging (exact error location without checking code)

**Initialization Order**:
```
1. Create message queue (for SoM communication)
2. Initialize web command list (for tracking pending commands)
3. Create SoM timer (for power-off timeout)
4. Enter message processing loop
```

All initialization must succeed or task aborts (returns early).

#### Change 11: Update Message Processing Loop

**Location**: Lines 327-345

**Before**:
```c
for (;;) {
    if (xQueueReceive(xUart4MsgQueue, &(msg), portMAX_DELAY)) {
        if (msg.header == FRAME_HEADER && msg.tail == FRAME_TAIL) {
            // Check checksum
            if (check_checksum(&msg)) {
                // handle command
                handle_deamon_mesage(&msg);
            } else {
                printf("[%s %d]:Checksum error!\n",__func__,__LINE__);
                buf_dump((uint8_t *)&msg, sizeof(msg));
                dump_message(msg);
            }
        } else {
            printf("[%s %d]:Invalid message format!\n",__func__,__LINE__);
            buf_dump((uint8_t *)&msg, sizeof(msg));
            dump_message(msg);
        }
    }
}
```

**After**:
```c
for (;;) {
    if (xQueueReceive(xUart4MsgQueue, &(msg), portMAX_DELAY)) {
        if (msg.header == FRAME_HEADER && msg.tail == FRAME_TAIL) {
            // Check checksum
            if (check_checksum(&msg)) {
                // handle command
                handle_som_mesage(&msg);  // Renamed function
            } else {
                printf("[%s %d]:SOM msg checksum error!\n",__func__,__LINE__);
                buf_dump((uint8_t *)&msg, sizeof(msg));
                dump_message(msg);
            }
        } else {
            printf("[%s %d]:Invalid SOM message format!\n",__func__,__LINE__);
            buf_dump((uint8_t *)&msg, sizeof(msg));
            dump_message(msg);
        }
    }
}
```

**Analysis**:

**Minor Improvements**:

1. **Function name update**: `handle_deamon_mesage()` → `handle_som_mesage()`
2. **Error message clarity**: Added "SOM" prefix to errors
   - Before: "Checksum error!"
   - After: "SOM msg checksum error!"
   - Helps distinguish SoM protocol errors from other subsystem errors

**Message Validation Sequence**:

```
1. Receive message from queue
2. Check frame structure:
   - header == FRAME_HEADER (0xA55AAA55)
   - tail == FRAME_TAIL (0x0D0A0D0A)
3. If structure valid, check checksum
4. If checksum valid, process message
5. If any check fails, dump and log error
```

**Queue Blocking**:

`portMAX_DELAY`: Block indefinitely waiting for message
- Task sleeps until message available
- No busy-waiting (efficient)
- Wakes immediately when message arrives

### 5. Web Server - Power Control Integration (`Core/Src/web-server.c`)

#### Changes: Thread-Safe Power Control with Timeout

**Location**: Lines 10354-10392

**Before**:
```c
//1:powerON,0:powerOFF
int get_power_status()
{
    return som_power_state == SOM_POWER_ON ? 1 : 0;
}

//status 0:power off,1:power on
int change_power_status(int status)
{
    int ret = 0;

    if (0 == status) {
        if (SOM_POWER_ON == som_power_state) {
            ret = web_cmd_handle(CMD_POWER_OFF, NULL, 0, 1000);
            if (HAL_OK != ret) {
                som_power_state = SOM_POWER_OFF;
                ret = HAL_OK;
                printf("Faild to power SOM, force shutdown SOM!\n");
            }
        } else {
            printf("SOM already power off!\n");
        }
    } else {
        if (SOM_POWER_OFF == som_power_state) {
            som_power_state = SOM_POWER_ON;
        } else {
            printf("SOM already power on!\n");
        }
    }
    return ret;
}
```

**After**:
```c
//1:powerON,0:powerOFF
static int get_power_status()  // Made static
{
    return get_som_power_state() == SOM_POWER_ON ? 1 : 0;
}

//status 0:power off,1:power on
static int change_power_status(int status)  // Made static
{
    int ret = 0;

    if (0 == status) {
        if (SOM_POWER_ON == get_som_power_state()) {
            ret = web_cmd_handle(CMD_POWER_OFF, NULL, 0, 1000);
            if (HAL_OK != ret) {
                change_som_power_state(SOM_POWER_OFF);
                printf("Poweroff SOM error(ret %d), force shutdown it!\n", ret);
                ret = HAL_OK;
                return ret;
            }
            // Trigger the Som timer to enusre SOM could poweroff in 5 senconds
            TriggerSomTimer();
        } else {
            printf("SOM already power off!\n");
        }
    } else {
        if (SOM_POWER_OFF == get_som_power_state()) {
            change_som_power_state(SOM_POWER_ON);
        } else {
            printf("SOM already power on!\n");
        }
    }
    return ret;
}
```

**Analysis**:

**Major Improvements**:

#### Improvement 1: Made Functions Static

```c
static int get_power_status()
static int change_power_status(int status)
```

**Why Static**:
- Functions only used within web-server.c
- Not part of public API
- Prevents name collisions with other modules
- Compiler can optimize better (knows function not called externally)

**Good Practice**: Always make internal functions `static` to limit scope.

#### Improvement 2: Use Thread-Safe Accessors

**Before**: Direct access
```c
if (SOM_POWER_ON == som_power_state) {
    som_power_state = SOM_POWER_OFF;
}
```

**After**: Accessor functions
```c
if (SOM_POWER_ON == get_som_power_state()) {
    change_som_power_state(SOM_POWER_OFF);
}
```

**Benefits**:
- Thread-safe (critical sections in accessors)
- Consistent with rest of codebase
- Prevents race conditions between web handler and power task

#### Improvement 3: Add Timer Trigger for Graceful Shutdown

**NEW CODE**:
```c
ret = web_cmd_handle(CMD_POWER_OFF, NULL, 0, 1000);
if (HAL_OK != ret) {
    // Command failed, force shutdown immediately
    change_som_power_state(SOM_POWER_OFF);
    printf("Poweroff SOM error(ret %d), force shutdown it!\n", ret);
    ret = HAL_OK;
    return ret;
}
// Command succeeded, start timeout timer
TriggerSomTimer();
```

**Graceful Shutdown Flow**:

**Case 1: SoM Responds Normally**
```
T=0s:   Web handler sends CMD_POWER_OFF to SoM
T=0s:   web_cmd_handle() returns HAL_OK (command sent successfully)
T=0s:   TriggerSomTimer() starts 5-second timeout
T=0-2s: SoM performs graceful shutdown
T=2s:   SoM sends MSG_NOTIFLY with CMD_POWER_OFF
T=2s:   handle_notify_mesage() sets power state to OFF
T=2s:   Power task cuts power
T=2s:   SoM powered down (gracefully)
T=5s:   Timer expires, callback sees power already OFF, no action
```

**Case 2: SoM Doesn't Respond (Hung)**
```
T=0s:   Web handler sends CMD_POWER_OFF to SoM
T=0s:   web_cmd_handle() returns HAL_OK
T=0s:   TriggerSomTimer() starts 5-second timeout
T=0-5s: SoM hung, no response
T=5s:   Timer expires
T=5s:   vSomTimerCallback() sees power still ON
T=5s:   Callback forces power state to OFF
T=5s:   Power task cuts power
T=5s:   SoM powered down (forced, not graceful)
```

**Case 3: Command Send Failed**
```
T=0s:  Web handler tries to send CMD_POWER_OFF
T=0s:  web_cmd_handle() returns HAL_ERROR (UART4 failed, SoM not responding)
T=0s:  Immediately force power state to OFF
T=0s:  Power task cuts power
T=0s:  SoM powered down (immediate, couldn't communicate)
T=0s:  No timer started (force shutdown, don't wait)
```

**Why Early Return After Force Shutdown**:
```c
if (HAL_OK != ret) {
    change_som_power_state(SOM_POWER_OFF);
    printf("Poweroff SOM error(ret %d), force shutdown it!\n", ret);
    ret = HAL_OK;
    return ret;  // Don't start timer, already forced off
}
// Timer trigger code only reached if command sent successfully
TriggerSomTimer();
```

Prevents starting timeout timer when we've already forced immediate shutdown.

#### Improvement 4: Better Error Messages

**Before**:
```c
printf("Faild to power SOM, force shutdown SOM!\n");
```

**After**:
```c
printf("Poweroff SOM error(ret %d), force shutdown it!\n", ret);
```

**Improvements**:
- Includes error code (`ret`) for debugging
- Fixed typo: "Faild" → "error"
- More concise message

**Comment Typo**:
```c
// Trigger the Som timer to enusre SOM could poweroff in 5 senconds
```

Should be: "ensure SOM could poweroff in 5 seconds"

## Complete Power-Off Flow Diagram

Putting it all together:

```
┌─────────────────────────────────────────────────────────────────┐
│                         User Action                              │
│      Web Interface: Click "Power Off" Button                     │
└──────────────────────────┬──────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│                   Web Server Task (BMC)                          │
│  1. Receive HTTP POST to /api/power_off                          │
│  2. Call change_power_status(0)                                  │
│  3. Check current state: get_som_power_state()                   │
│  4. Send CMD_POWER_OFF via web_cmd_handle()                      │
│  5. If send OK: TriggerSomTimer() (5 second timeout)             │
│  6. If send FAIL: change_som_power_state(OFF) immediately        │
└──────────────────────────┬──────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│              Protocol Task (BMC) - UART4                         │
│  1. Serialize CMD_POWER_OFF message                              │
│  2. Send via UART4 to SoM                                        │
│  3. Wait for response (with timeout)                             │
└──────────────────────────┬──────────────────────────────────────┘
                           ↓
                   ╔═══════════════╗
                   ║  Physical     ║
                   ║  UART4 Link   ║
                   ╚═══════╦═══════╝
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│              SoM Daemon Process (OpenSBI/Linux)                  │
│  1. Receive CMD_POWER_OFF via UART4                              │
│  2. Initiate graceful shutdown:                                  │
│     - systemctl poweroff (if Linux)                              │
│     - Close all files                                            │
│     - Unmount filesystems                                        │
│     - Stop all processes                                         │
│     - Return to OpenSBI                                          │
│  3. OpenSBI: Prepare hardware for power-down                     │
│  4. Send MSG_NOTIFLY, CMD_POWER_OFF back to BMC                  │
└──────────────────────────┬──────────────────────────────────────┘
                           ↓
                   ╔═══════════════╗
                   ║  Physical     ║
                   ║  UART4 Link   ║
                   ╚═══════╦═══════╝
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│              Protocol Task (BMC) - UART4                         │
│  1. Receive MSG_NOTIFLY message                                  │
│  2. Validate checksum                                            │
│  3. Call handle_som_mesage()                                     │
│  4. Route to handle_notify_mesage()                              │
│  5. Call change_som_power_state(SOM_POWER_OFF)                   │
│  6. (Timer cancelled implicitly - power state changed)           │
└──────────────────────────┬──────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│              Power Management Task (BMC)                         │
│  1. Poll get_som_power_state() every 100ms                       │
│  2. Detect state changed to SOM_POWER_OFF                        │
│  3. Transition to STOP_POWER state                               │
│  4. Execute power-down sequence:                                 │
│     - i2c_deinit(I2C3) - prevent leakage                         │
│     - i2c_deinit(I2C1) - prevent leakage                         │
│     - pmic_power_on(FALSE) - disable PMIC                        │
│     - osDelay(10) - allow capacitor discharge                    │
│     - atx_power_on(FALSE) - disable main power                   │
│     - osDelay(10) - ensure clean power-off                       │
│  5. SoM power rails disabled                                     │
│  6. Transition to IDLE state                                     │
└─────────────────────────────────────────────────────────────────┘

ALTERNATE PATH: Timeout (SoM Hung)
┌─────────────────────────────────────────────────────────────────┐
│              Timer Service Task (FreeRTOS)                       │
│  1. xSomTimer expires after 5 seconds                            │
│  2. Call vSomTimerCallback()                                     │
│  3. Check: get_som_power_state() still ON?                       │
│  4. Yes: Force power-off                                         │
│     - change_som_power_state(SOM_POWER_OFF)                      │
│     - Print "Poweroff SOM timeout, shutdown it now!"             │
│  5. Power task sees state change, executes power-down            │
└─────────────────────────────────────────────────────────────────┘
```

## Thread Safety Analysis

This patch introduces comprehensive thread-safe state management. Let's analyze potential race conditions:

### Shared Resource: `som_power_state`

**Accessing Threads**:
1. **Power Management Task**: Reads state to determine actions
2. **Web Server Task**: Writes state on power control requests
3. **GPIO Button Task**: Writes state on button presses
4. **Protocol Task**: Writes state on SoM notifications
5. **Timer Callback**: Writes state on timeout

**Protection Mechanism**:
- All access through `get_som_power_state()` / `change_som_power_state()`
- Critical sections disable interrupts during access
- Atomic read/write operations

**Race Condition Example** (Prevented):

**Scenario**: Button pressed while web power-off in progress

**Without Protection** (Hypothetical):
```
Thread 1 (Button):        Thread 2 (Web):           State
Read state (ON)                                     ON
                          Read state (ON)           ON
Decide: toggle to OFF                               ON
                          Decide: power off         ON
Write state = OFF                                   OFF
                          Write state = OFF         OFF (duplicate)
Trigger power-off         Trigger power-off        (both trigger!)
```

**With Protection** (Actual):
```
Thread 1 (Button):             Thread 2 (Web):           State
get_som_power_state()                                    ON
  ENTER_CRITICAL                                         ON
  Read state = ON                                        ON
  EXIT_CRITICAL                                          ON
  Return ON                                              ON
Decide: toggle to OFF                                    ON
change_som_power_state(OFF)                              ON
  ENTER_CRITICAL                                         ON
  Write state = OFF                                      OFF
  EXIT_CRITICAL                                          OFF
                               get_som_power_state()     OFF
                                 ENTER_CRITICAL          OFF
                                 Read state = OFF        OFF
                                 EXIT_CRITICAL           OFF
                                 Return OFF              OFF
                               Decide: already off       OFF
                               Print "already off"       OFF
Only one power-off triggered!                            OFF
```

### Critical Section Duration Analysis

**Accessor Functions**:
```c
taskENTER_CRITICAL();
state = som_power_state;  // ~1 clock cycle (load instruction)
taskEXIT_CRITICAL();
```

**Duration**: ~10-20 CPU cycles = ~60-120 nanoseconds at 168 MHz

**Impact**: Negligible interrupt latency (well under 1 microsecond)

**Power Task Main Loop**:
```c
taskENTER_CRITICAL();
switch (power_state) {
    // ... state handling code
}
taskEXIT_CRITICAL();
```

**Duration**: Varies by state, but includes `osDelay()` calls!

**Problem**: `osDelay()` in critical section blocks scheduler

**Should Be Fixed**: Discussed in patch 0019 analysis

## Testing Recommendations

### Functional Tests

#### Test 1: Normal Graceful Shutdown

**Procedure**:
1. Power on SoM (verify boots fully)
2. Web interface: Click "Power Off"
3. Observe serial console output
4. Time shutdown duration

**Expected**:
```
Web command sent: "CMD_POWER_OFF"
SoM begins shutdown...
[2-3 seconds of shutdown activity]
SoM notification: "MSG_NOTIFLY, CMD_POWER_OFF"
BMC: "Poweroff SOM normaly, shutdown it now!"
Power rails disabled
Total time: 2-3 seconds
```

#### Test 2: Timeout Path (Simulated Hung SoM)

**Procedure**:
1. Modify SoM daemon to NOT send notification
2. Web interface: Click "Power Off"
3. Observe console output
4. Time shutdown duration

**Expected**:
```
Web command sent: "CMD_POWER_OFF"
SoM receives but doesn't respond...
[5 seconds of waiting]
BMC: "Poweroff SOM timeout, shutdown it now!"
Power forced off
Total time: 5 seconds (timeout)
```

#### Test 3: Communication Failure Path

**Procedure**:
1. Disconnect UART4 physically
2. Web interface: Click "Power Off"
3. Observe console output

**Expected**:
```
Web command send attempt...
web_cmd_handle() returns HAL_TIMEOUT
BMC: "Poweroff SOM error(ret 3), force shutdown it!"
Immediate power-off
Total time: <1 second
```

#### Test 4: Race Condition Test

**Procedure**:
1. Script: Rapidly click "Power Off" in web interface
2. Simultaneously: Press physical power button repeatedly
3. Observe for duplicate power-off attempts

**Expected**:
- Only ONE actual power-off sequence triggered
- Console shows "SOM already power off!" for duplicate attempts
- Clean state transitions

#### Test 5: Daemon Status Tracking

**Procedure**:
1. SoM powered on, daemon running
2. Observe daemon status via web interface: "Online"
3. Stop SoM daemon process manually
4. Wait up to 20 seconds (5 attempts × 4 seconds)
5. Observe daemon status changes to "Offline"

**Expected**:
```
T=0s:   Daemon ON
T=4s:   Check fails (count=1), still ON
T=8s:   Check fails (count=2), still ON
T=12s:  Check fails (count=3), still ON
T=16s:  Check fails (count=4), still ON
T=20s:  Check fails (count=5), NOW OFF
Console: "SOM Daemon status change to off!"
```

### Performance Tests

#### Shutdown Duration Measurement

**Setup**:
- Oscilloscope on SoM power rail
- Serial console for logs
- Timestamp logging enabled

**Metrics**:
1. **Web Command Latency**: Click to CMD_POWER_OFF sent
2. **SoM Shutdown Time**: CMD_POWER_OFF received to notification sent
3. **Power-Off Execution**: Notification received to power rail disabled
4. **Total Shutdown**: Click to power rail off

**Target**:
- Web latency: <100ms
- SoM shutdown: 1-3 seconds (depends on workload)
- Power-off exec: <100ms
- Total: 1.2-3.2 seconds (typical)

### Stress Tests

#### Rapid Power Cycling

**Procedure**:
1. Script: Power on, wait 10s, power off, wait 5s
2. Repeat 100 cycles
3. Monitor for failures, hangs, memory leaks

**Expected**:
- All cycles complete successfully
- No memory leaks (heap usage stable)
- No state machine hangs
- Consistent timing

#### Concurrent Access

**Procedure**:
1. Script 1: Web power control (random on/off every 2s)
2. Script 2: Physical button simulation (random every 3s)
3. Script 3: Read status via API (every 0.5s)
4. Run for 10 minutes

**Expected**:
- No crashes
- State consistency (status reads always match actual state)
- No duplicate power operations
- All commands completed or cleanly failed

## Security Considerations

### Race Condition Elimination

**Before Patch**:
- Direct global variable access
- Potential for inconsistent state
- Could cause unexpected power states
- Denial of service potential (hang in undefined state)

**After Patch**:
- Thread-safe accessors with critical sections
- Consistent state across all tasks
- Defined behavior under concurrent access
- Improved fault tolerance

### Timeout Protection

**Security Benefit**:
- Prevents infinite hang on malicious/buggy SoM behavior
- Ensures BMC remains responsive (can force power-off)
- Denies persistent attack surface (attacker can't prevent power-off indefinitely)

### Communication Integrity

**Improvements**:
- Checksum validation enforced
- Invalid messages rejected and logged
- Frame structure validation
- Prevents protocol injection attacks

## Known Issues and Future Work

### Issue 1: Critical Section Too Broad in Power Task

**Problem**:
```c
taskENTER_CRITICAL();
switch (power_state) {
    // States contain osDelay() calls!
}
taskEXIT_CRITICAL();
```

`osDelay()` in critical section causes busy-wait instead of yielding.

**Fix**: Should use mutexes instead of critical sections for power task, or restructure to minimize critical section scope.

### Issue 2: Timer Cancellation Not Explicit

**Current**:
- Timer expires even if graceful shutdown succeeds
- Callback checks if power already off, does nothing

**Better**:
```c
void handle_notify_mesage(Message *msg) {
    if (CMD_POWER_OFF == msg->cmd_type) {
        xTimerStop(xSomTimer, 0);  // Cancel timer explicitly
        change_som_power_state(SOM_POWER_OFF);
        printf("Poweroff SOM normaly, shutdown it now!\n");
    }
}
```

**Impact**: Minor - current approach works, just not as clean

### Issue 3: Error Code Inconsistency

**Problem**: `pmic_b6out_105v()` returns boolean (0/1) but used as HAL status

**Current**:
```c
status = pmic_b6out_105v();  // Returns 0 or 1
while(status != HAL_OK && try_count--)  // HAL_OK is 0x00
```

Works by accident (HAL_OK == 0, function returns 0 on success).

**Better**: Define consistent status type or use HAL_StatusTypeDef throughout.

### Issue 4: Power-Off Return Value Ignored

**Current**:
```c
ret = web_cmd_handle(CMD_POWER_OFF, NULL, 0, 1000);
if (HAL_OK != ret) {
    // Force shutdown
} else {
    // Start timer, but what if power-off request rejected by SoM?
}
```

**Issue**: `web_cmd_handle()` success means "message sent", not "SoM accepted"

**Better**: Wait for explicit ACK from SoM before starting timer, or shorter timeout.

## Conclusion

Patch 0020 completes the web command support infrastructure with three critical improvements:

### 1. Thread-Safe State Management
- **Accessor functions** for `som_power_state` and `som_daemon_state`
- **Critical section protection** preventing race conditions
- **Volatile declarations** ensuring proper memory access
- **Consistent API** across all subsystems

### 2. Graceful Power-Off with Timeout
- **Notification messages** enable SoM-controlled shutdown timing
- **5-second timeout** prevents indefinite hangs
- **Graceful fallback** to forced shutdown on timeout
- **Web interface integration** for user-triggered shutdowns

### 3. SoM Daemon Status Tracking
- **Heartbeat monitoring** detects SoM crashes
- **Failure tolerance** (5 consecutive failures before declaring dead)
- **Status visibility** for web interface and diagnostics
- **Recovery capability** (can restart crashed SoM)

**Total Impact**: 130 lines added, 37 lines removed across 5 files, transforming the BMC from a simple power controller into a robust, thread-safe system with reliable SoM communication and coordinated shutdown capabilities.

**Critical Achievement**: Eliminates race conditions in power state management, enabling safe concurrent access from web interface, physical buttons, protocol handlers, and power management tasks.

**Integration**: This patch works synergistically with patch 0019's power sequence optimizations and patch 0018's UART4 initialization fixes to create a complete, production-ready BMC power management system.
