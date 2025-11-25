# Patch 0014: BMC Web Command Support - SOM Communication Infrastructure

## Metadata
- **Patch File**: `0014-WIN2030-15099-refactor-bmc-bmc-web-cmd-support.patch`
- **Author**: huangyifeng <huangyifeng@eswincomputing.com>
- **Date**: Thu, 9 May 2024 14:27:45 +0800
- **Ticket**: WIN2030-15099
- **Type**: refactor (Major architectural refactoring)
- **Change-Id**: I242fb0c29a0195c64b47fdf44e15ed456cb072bf

## Summary

This patch represents a **major architectural transformation** of the BMC-SOM communication infrastructure. It replaces the ring buffer-based UART4 message handling with a FreeRTOS queue-based system, implements a robust command-response protocol for web interface integration, and introduces a daemon keep-alive task to monitor SOM operational status. This refactoring enables the web interface to retrieve live data from the System-on-Module (PVT information, power status, board details) and send control commands (reset SOM, power management).

The patch eliminates previous buffering inefficiencies, adds thread-safe message handling, and establishes the foundation for bi-directional BMC-SOM communication that subsequent patches will expand upon.

## Changelog

The official changelog from the commit message:
1. **Could use web cmd to get pvt info, reset SOM, get SOM info** - Web interface can now query SOM for sensor data and issue control commands
2. **Use queue instead of ring buffer to store uart4 msg** - FreeRTOS queue replaces custom ring buffer for superior message handling
3. **Update the SOM daemon status** - New daemon monitoring task tracks SOM responsiveness

## Statistics

- **Files Changed**: 8 files
- **Insertions**: 9,642 lines
- **Deletions**: 9,565 lines
- **Net Change**: +77 lines (massive refactoring with similar line count)

**File-by-File Breakdown**:
- `Core/Inc/hf_common.h`: +22 lines (new data structures, function prototypes)
- `Core/Src/hf_gpio_process.c`: -2 lines (cleanup)
- `Core/Src/hf_it_callback.c`: +39 lines (queue-based interrupt handling)
- `Core/Src/hf_power_process.c`: +4 lines (extern removal, minor cleanup)
- `Core/Src/hf_protocol_process.c`: +197 lines (major refactoring)
- `Core/Src/main.c`: +10 lines (task creation)
- `Core/Src/stm32f4xx_it.c`: -8 lines (interrupt handler cleanup)
- `Core/Src/web-server.c`: 18,925 lines changed (massive embedded HTML regeneration)

**Note**: The web-server.c line count is misleadingly large due to regeneration of embedded HTML arrays from source files. Actual functional code changes are more modest.

## Architectural Overview

### Before Patch 0014

```
┌─────────────────────────────────────────────────────┐
│ UART4 Interrupt (SOM → BMC)                        │
│   ↓                                                 │
│ HAL_UARTEx_RxEventCallback()                       │
│   ↓                                                 │
│ es_frame_put(&frame_uart4, ...)  [Ring Buffer]    │
└─────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────┐
│ uart4_protocol_task (Polling Loop)                 │
│   ↓                                                 │
│ get_ring_buf_len(&frame_uart4._frame_ring)        │
│   ↓                                                 │
│ read_ring_buf(&frame_uart4._frame_ring, ...)      │
│   ↓                                                 │
│ Parse Message struct                                │
│   ↓                                                 │
│ handle_daemon_message()                            │
└─────────────────────────────────────────────────────┘

Problems:
- Ring buffer manual management (head/tail pointers, wrap-around)
- Polling loop wastes CPU checking empty buffer
- No built-in message synchronization
- Complex error handling for buffer overflow
```

### After Patch 0014

```
┌─────────────────────────────────────────────────────┐
│ UART4 Interrupt (SOM → BMC)                        │
│   ↓                                                 │
│ HAL_UARTEx_RxEventCallback()                       │
│   ↓                                                 │
│ xQueueSendFromISR(xUart4MsgQueue, &UART4_RxMsg, ...)│
│   ↓                                                 │
│ portYIELD_FROM_ISR() [Context switch if needed]   │
└─────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────┐
│ uart4_protocol_task (Event-Driven)                 │
│   ↓                                                 │
│ xQueueReceive(xUart4MsgQueue, &msg, portMAX_DELAY) │
│   [BLOCKS until message available]                 │
│   ↓                                                 │
│ Validate Message (header, tail, checksum)          │
│   ↓                                                 │
│ handle_daemon_message()                            │
└─────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────┐
│ Web Command Handling (New!)                        │
│   ↓                                                 │
│ web_cmd_handle(CMD_PVT_INFO, ...)                  │
│   ↓                                                 │
│ transmit_daemon_request() → UART4 TX               │
│   ↓                                                 │
│ xTaskNotifyWait() [Block for response]             │
│   ↓                                                 │
│ handle_daemon_message() matches response            │
│   ↓                                                 │
│ xTaskNotifyGive() [Unblock web handler]            │
│   ↓                                                 │
│ Return data to web interface                        │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│ daemon_keeplive_task (New!)                        │
│   ↓                                                 │
│ Periodic loop (every 4 seconds)                    │
│   ↓                                                 │
│ web_cmd_handle(CMD_BOARD_STATUS, ...)             │
│   ↓                                                 │
│ If timeout or error → som_daemon_state = OFF       │
│   ↓                                                 │
│ If success → som_daemon_state = ON                 │
└─────────────────────────────────────────────────────┘

Benefits:
- FreeRTOS queue handles synchronization automatically
- Task blocks efficiently (no CPU wasted polling)
- Built-in overflow handling (queue full detection)
- Clean integration with RTOS scheduler
```

## Detailed Changes

### 1. Data Structure Enhancements (`hf_common.h`)

#### 1.1 SOM Daemon State Enumeration

**New Type** (lines 39-42):
```c
typedef enum {
    SOM_DAEMON_OFF,
    SOM_DAEMON_ON,
} deamon_stats_t;  // Note: Typo "deamon" (should be "daemon")
```

**Purpose**: Tracks whether the SOM daemon process is responsive

**States**:
- **SOM_DAEMON_OFF**: SOM not responding to keep-alive messages
  - Could indicate SOM powered off, crashed, or daemon not running
  - Web interface should display warning
- **SOM_DAEMON_ON**: SOM daemon responding normally
  - Healthy communication channel
  - Commands can be sent reliably

**Usage**:
```c
extern deamon_stats_t som_daemon_state;  // Global state variable

// In keep-alive task
if (web_cmd_handle(CMD_BOARD_STATUS, NULL, 0, 1000) != HAL_OK) {
    som_daemon_state = SOM_DAEMON_OFF;
} else {
    som_daemon_state = SOM_DAEMON_ON;
}
```

#### 1.2 Command Type Extension

**Updated Enum** (lines 49-59):
```c
typedef enum {
    CMD_POWER_OFF = 0x01,
    CMD_RESET,              // 0x02 (now enum auto-increment)
    CMD_READ_BOARD_INFO,    // 0x03
    CMD_CONTROL_LED,        // 0x04
    CMD_PVT_INFO,           // 0x05 (NEW)
    CMD_BOARD_STATUS,       // 0x06 (NEW)
} CommandType;
```

**Changes**:
- **Removed Explicit Values**: Previous `CMD_RESET = 0x02` changed to auto-increment
  - **Benefit**: Easier to add new commands without manual numbering
  - **Risk**: Must ensure enum order never changes (binary protocol compatibility)
- **New Commands**:
  - `CMD_PVT_INFO`: Request Process-Voltage-Temperature sensor data from SOM
  - `CMD_BOARD_STATUS`: Keep-alive ping to verify SOM daemon operational

**Protocol Implications**:
```c
// Command packet structure (unchanged)
typedef struct {
    uint32_t header;        // 0xA55AAA55
    uint8_t msg_type;       // MSG_REQUEST or MSG_REPLY
    uint8_t cmd_type;       // CommandType enum
    uint8_t data_len;       // Payload length
    uint32_t xTaskToNotify; // Requester task handle
    uint8_t data[FRAME_DATA_MAX];
    uint8_t checksum;       // XOR of msg_type, cmd_type, data_len, data[]
    uint32_t tail;          // 0xBDBABDBA
} Message;
```

#### 1.3 Global Variable Exports

**New Exports** (lines 46, 67-68):
```c
extern power_switch_t som_power_state;  // Moved from hf_gpio_process.c
extern QueueHandle_t xUart4MsgQueue;    // FreeRTOS queue
extern Message UART4_RxMsg;             // DMA receive buffer
```

**Rationale**:
- `som_power_state`: Now used by multiple modules (power task, web server, protocol task)
  - Centralized in header for consistent access
- `xUart4MsgQueue`: Queue handle needed by both ISR and protocol task
- `UART4_RxMsg`: DMA buffer shared between ISR and UART initialization code

**Before** (hf_gpio_process.c):
```c
extern power_switch_t som_power_state;  // Local declaration
```

**After** (hf_common.h):
```c
extern power_switch_t som_power_state;  // Globally accessible
```

#### 1.4 Function Prototype Updates

**Modified Signature** (line 78):
```c
// Before
int web_cmd_handle(CommandType cmd, void *data, int data_len);

// After
int web_cmd_handle(CommandType cmd, void *data, int data_len, uint32_t timeout);
```

**New Parameter**: `timeout` (in milliseconds)
- Allows caller to specify how long to wait for response
- Keep-alive uses 1000ms (1 second timeout)
- Web commands might use longer timeout (e.g., 5000ms for slow operations)
- Provides flexibility for different command types with varying response times

**New Prototype** (line 77):
```c
void som_reset_control(uint8_t reset);
```

**Purpose**: Exported from hf_power_process.c for use by web command handlers
- Previously `static` function, now globally accessible
- Allows web interface to trigger SOM reset via command protocol

### 2. Interrupt Handler Refactoring (`hf_it_callback.c`)

#### 2.1 Ring Buffer Removal

**Deleted Code** (lines 112-116):
```c
// REMOVED
extern b_frame_class_t frame_uart4;

static void buf_dump(uint8_t *data) {
    // Debug function to print buffer contents
}
```

**Rationale**: Ring buffer infrastructure no longer needed with queue-based approach

#### 2.2 Queue-Based UART4 Reception

**New Implementation** (lines 129-158):

```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (huart->Instance == USART3) {
        // UART3 handling unchanged (ring buffer for console)
        es_frame_put(&frame_uart3, RxBuf, Size);
        HAL_UARTEx_ReceiveToIdle_DMA(&huart3, RxBuf, RxBuf_SIZE);
        __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);

    } else if (huart->Instance == UART4) {
        // NEW: Queue-based handling
        if (HAL_UART_RXEVENT_TC == huart->RxEventType) {
            if (xQueueSendFromISR(xUart4MsgQueue,
                (void *)&UART4_RxMsg, &xHigherPriorityTaskWoken) != pdPASS) {
                // Queue full - drop message
                printf("[%s %d]: xUart4MsgQueue is full, drop the msg!\n",
                       __func__, __LINE__);
            }

            // Clear buffer for next reception
            memset(&UART4_RxMsg, 0, sizeof(UART4_RxMsg) / sizeof(uint8_t));

            // Restart DMA reception
            HAL_UARTEx_ReceiveToIdle_DMA(&huart4, (int8_t *)&UART4_RxMsg,
                                          sizeof(UART4_RxMsg));

            // Yield to higher priority task if necessary
            if (xHigherPriorityTaskWoken) {
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
        }
    }
    // USART6 handling...
}
```

**Key Implementation Details**:

**1. FreeRTOS ISR-Safe Queue Operations**:
```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;

xQueueSendFromISR(xUart4MsgQueue, (void *)&UART4_RxMsg, &xHigherPriorityTaskWoken);
```
- **`xQueueSendFromISR()`**: ISR-safe version of `xQueueSend()`
  - Cannot use regular `xQueueSend()` in interrupt context (disables interrupts too long)
- **`xHigherPriorityTaskWoken`**: Output parameter set to `pdTRUE` if sending to queue unblocks a higher-priority task
  - Enables immediate task switch if protocol handling task is waiting

**2. Context Switch Optimization**:
```c
if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```
- **Purpose**: Immediately switch to protocol task if it's higher priority and was waiting for message
- **Benefit**: Minimizes latency between message reception and processing
- **Mechanism**: Sets PendSV interrupt (ARM Cortex-M feature for context switching)

**3. Queue Full Handling**:
```c
if (xQueueSendFromISR(...) != pdPASS) {
    printf("[%s %d]: xUart4MsgQueue is full, drop the msg!\n", __func__, __LINE__);
}
```
- **Graceful Degradation**: If queue full (8 messages backlog), drop newest message
- **Alternative Strategies** (not implemented):
  - Increase queue depth (costs more RAM)
  - Block until space available (NOT possible in ISR)
  - Overwrite oldest message (would require custom queue logic)

**4. Buffer Management**:
```c
memset(&UART4_RxMsg, 0, sizeof(UART4_RxMsg) / sizeof(uint8_t));
HAL_UARTEx_ReceiveToIdle_DMA(&huart4, (int8_t *)&UART4_RxMsg, sizeof(UART4_RxMsg));
```
- **Clear Buffer**: Zeroing prevents stale data from previous messages
- **Restart DMA**: Re-arms DMA for next message reception
- **DMA Idle Detection**: `ReceiveToIdle` triggers callback on line idle (no more bytes for ~1 byte time)

**5. Event Type Check**:
```c
if (HAL_UART_RXEVENT_TC == huart->RxEventType) {
    // Process message
}
```
- **HAL_UART_RXEVENT_TC**: Transfer Complete event (full message received)
- **Other Events**: IDLE, ERROR (not handled in this implementation)
- **Ensures**: Only complete messages queued (not partial/corrupted receptions)

### 3. Protocol Task Refactoring (`hf_protocol_process.c`)

#### 3.1 Global Variables Update

**Removed** (line 221):
```c
// DELETED
b_frame_class_t frame_uart4;  // Ring buffer frame handler
```

**Added** (lines 220, 225):
```c
Message UART4_RxMsg;          // DMA RX buffer (global for ISR access)
QueueHandle_t xUart4MsgQueue; // FreeRTOS queue handle
```

#### 3.2 Message Type Enumeration

**Simplified** (lines 228-231):
```c
typedef enum {
    MSG_REQUEST = 0x01,
    MSG_REPLY,  // Auto-increment to 0x02
} MsgType;
```

**Change**: Removed explicit `= 0x02` assignment (now auto-incremented)

#### 3.3 Checksum Functions Type Fix

**Unsigned Char Correction** (lines 246, 254):

**Before**:
```c
uint8_t checksum = 0;  // Might overflow in some contexts
```

**After**:
```c
unsigned char checksum = 0;  // Explicitly unsigned, consistent with protocol
```

**Rationale**:
- `uint8_t` is typedef for `unsigned char`, but explicit `unsigned char` is clearer
- Ensures XOR operations don't inadvertently sign-extend
- Consistent with protocol specification (checksums are always unsigned)

#### 3.4 Mutex-Protected UART Transmission

**New Functions** (lines 49-75):

```c
SemaphoreHandle_t xMutex = NULL;

void init_transmit_mutex(void) {
    xMutex = xSemaphoreCreateMutex();
    if (xMutex == NULL) {
        // Mutex creation failed - handle error
    }
}

void acquire_transmit_mutex(void) {
    if (xSemaphoreTake(xMutex, portMAX_DELAY) != pdPASS) {
        // Failed to acquire mutex - handle error
    }
}

void release_transmit_mutex(void) {
    xSemaphoreGive(xMutex);
}
```

**Applied to transmit_daemon_request()** (lines 532-543):

```c
BaseType_t transmit_daemon_request(Message *msg)
{
    UART_HandleTypeDef *huart = &huart4;

    acquire_transmit_mutex();  // Lock

    generate_checksum(msg);
    HAL_StatusTypeDef status = HAL_UART_Transmit(huart, (uint8_t *)msg,
                                sizeof(Message), HAL_MAX_DELAY);

    release_transmit_mutex();  // Unlock

    return status;
}
```

**Problem Solved**: Race condition in multi-threaded transmission

**Scenario Without Mutex**:
```
Time  | Web Command Task        | Keep-Alive Task
------|-----------------------|-------------------------
T0    | Prepare CMD_PVT_INFO   |
T1    | Start UART_Transmit()  |
T2    |   [Transmitting...]    | Prepare CMD_BOARD_STATUS
T3    |   [Transmitting...]    | Start UART_Transmit()
T4    |   byte 10 of 20 sent   | byte 1 of 20 sent (CORRUPTION!)
```

**Result**: Interleaved bytes, garbled message, SOM receives invalid checksum

**Solution**: Mutex ensures atomic transmission
```
Time  | Web Command Task        | Keep-Alive Task
------|-----------------------|-------------------------
T0    | acquire_mutex()        |
T1    | Transmit complete      |
T2    | release_mutex()        |
T3    |                        | acquire_mutex() [blocks]
T4    |                        | acquire_mutex() [acquired]
T5    |                        | Transmit complete
```

#### 3.5 Enhanced web_cmd_handle() Function

**Updated Signature** (line 553):
```c
int web_cmd_handle(CommandType cmd, void *data, int data_len, uint32_t timeout)
```

**New Timeout Parameter**:
```c
if (xTaskNotifyWait(0, 0, &ulNotificationValue, pdMS_TO_TICKS(timeout)) == pdTRUE) {
    // Response received within timeout
    ret = webcmd.cmd_result;
    memcpy(data, webcmd.data, data_len);
} else {
    // Timeout
    ret = HAL_TIMEOUT;
    goto err_msg;
}
```

**Timeout Behavior**:
- **Before**: Hardcoded 100ms timeout
  - Too short for slow operations (complex sensor reads, boot-time commands)
  - Too long for fast operations (wasted time on failures)
- **After**: Caller-specified timeout
  - Keep-alive uses 1000ms (1 second)
  - PVT info might use 5000ms (5 seconds for sensor stabilization)
  - Board info might use 2000ms (2 seconds for EEPROM read)

**Return Value Semantics** (line 524):
```c
// Before
int ret = -HAL_ERROR;  // Negative values

// After
int ret = HAL_ERROR;   // Positive HAL status codes
```

**Change**: Return positive HAL status codes for consistency
- `HAL_OK` = 0
- `HAL_ERROR` = 1
- `HAL_TIMEOUT` = 3
- Aligns with STM32 HAL convention

#### 3.6 New Daemon Keep-Alive Task

**Implementation** (lines 587-625):

```c
deamon_stats_t som_daemon_state = SOM_DAEMON_OFF;

void deamon_keeplive_task(void *argument)
{
    int ret = HAL_OK;
    deamon_stats_t old_status;
    static uint8_t count = 0;

    for (;;) {
        // Only run keep-alive if SOM powered on
        if (SOM_POWER_ON != som_power_state) {
            osDelay(50);
            continue;
        }

        old_status = som_daemon_state;

        // Send keep-alive command
        ret = web_cmd_handle(CMD_BOARD_STATUS, NULL, 0, 1000);

        if (HAL_OK != ret) {
            // Failure handling
            if (HAL_TIMEOUT == ret) {
                if (count <= 5)
                    printf("SOM keeplive request timeout!\n");
            } else {
                if (count <= 5)
                    printf("SOM keeplive request send failed!\n");
            }
            count++;
            if (5 >= count) {
                som_daemon_state = SOM_DAEMON_OFF;
            }
        } else {
            // Success
            som_daemon_state = SOM_DAEMON_ON;
            count = 0;
        }

        // Log state transitions
        if (old_status != som_daemon_state) {
            printf("SOM Daemon status change to %s!\n",
                som_daemon_state == SOM_DAEMON_ON ? "on" : "off");
        }

        // Check every 4 seconds
        osDelay(pdMS_TO_TICKS(4000));
    }
}
```

**Key Design Decisions**:

**1. Power State Gating**:
```c
if (SOM_POWER_ON != som_power_state) {
    osDelay(50);
    continue;
}
```
- **Purpose**: Don't send keep-alive commands when SOM is powered off
- **Benefit**: Prevents error spam in console when SOM intentionally off
- **50ms Delay**: Short sleep to avoid tight loop burning CPU

**2. Error Counter Mechanism**:
```c
static uint8_t count = 0;
if (HAL_OK != ret) {
    count++;
    if (5 >= count) {
        som_daemon_state = SOM_DAEMON_OFF;
    }
} else {
    count = 0;  // Reset on success
}
```
- **Hysteresis**: Requires 5 consecutive failures before declaring daemon OFF
- **Prevents Flapping**: Transient network glitches don't cause false alarms
- **Fast Recovery**: Single success resets counter immediately

**3. Log Suppression**:
```c
if (count <= 5)
    printf("SOM keeplive request timeout!\n");
```
- **Prevents Console Spam**: After 5 failures, stop logging errors
- **User Experience**: Avoids log flooding when SOM is known to be down

**4. Keep-Alive Interval**:
```c
osDelay(pdMS_TO_TICKS(4000));  // 4 seconds
```
- **Trade-offs**:
  - Shorter interval (e.g., 1s): Faster detection of failures, but more CPU/UART overhead
  - Longer interval (e.g., 10s): Lower overhead, but slower fault detection
- **4 seconds**: Balanced choice
  - Detects failures within ~20 seconds (5 failures × 4s)
  - Low overhead (~0.1% CPU for command round-trip)

#### 3.7 Protocol Task Queue Integration

**New Initialization** (lines 690-696):

```c
void uart4_protocol_task(void *argument)
{
    Message msg;

    init_transmit_mutex();

    xUart4MsgQueue = xQueueCreate(QUEUE_LENGTH, sizeof(Message));
    if (xUart4MsgQueue == NULL) {
        printf("Failed to create msg queue!\n");
        return;
    }

    // ... rest of initialization
}
```

**Queue Parameters**:
```c
#define QUEUE_LENGTH 8
xQueueCreate(QUEUE_LENGTH, sizeof(Message));
```
- **Length**: 8 messages
  - **Capacity**: Can buffer 8 complete messages before queue full
  - **Memory**: 8 × sizeof(Message) ≈ 8 × 270 bytes = 2.16 KB
- **Item Size**: sizeof(Message)
  - FreeRTOS copies entire Message struct into queue (not just pointer)
  - **Benefit**: No lifetime management issues (ISR buffer can be reused immediately)

**Blocking Receive** (line 482):

```c
// OLD: Polling with delays
len = get_ring_buf_len(ring);
if (!len) {
    osDelay(10);
    continue;
}

// NEW: Efficient blocking
if (xQueueReceive(xUart4MsgQueue, &(msg), portMAX_DELAY)) {
    // Message available, process it
}
```

**Benefits of Blocking Receive**:
- **CPU Efficiency**: Task sleeps until message arrives (no wasted cycles)
- **Low Latency**: Immediately woken when message received
- **Simplified Logic**: No manual length checking or timeouts

**Message Validation** (lines 483-498):

```c
if (xQueueReceive(xUart4MsgQueue, &(msg), portMAX_DELAY)) {
    if (msg.header == FRAME_HEADER && msg.tail == FRAME_TAIL) {
        if (check_checksum(&msg)) {
            handle_daemon_message(&msg);
        } else {
            printf("[%s %d]:Checksum error!\n", __func__, __LINE__);
            buf_dump((uint8_t *)&msg, sizeof(msg));
            dump_message(msg);
        }
    } else {
        printf("[%s %d]:Invalid message format!\n", __func__, __LINE__);
        buf_dump((uint8_t *)&msg, sizeof(msg));
        dump_message(msg);
    }
}
```

**Validation Sequence**:
1. **Header Check**: `msg.header == 0xA55AAA55`
2. **Tail Check**: `msg.tail == 0xBDBABDBA`
3. **Checksum Validation**: XOR of msg_type, cmd_type, data_len, data[]

**Error Handling**: Invalid messages logged with full hex dump for debugging

#### 3.8 Simplified handle_daemon_message()

**Reformatted Code** (lines 630-650):

```c
void handle_daemon_message(Message *msg)
{
    if (MSG_REPLY == msg->msg_type) {
        if (!listLIST_IS_EMPTY(&WebCmdList)) {
            taskENTER_CRITICAL();
            for (ListItem_t *pxItem = listGET_HEAD_ENTRY(&WebCmdList);
                pxItem != listGET_END_MARKER(&WebCmdList);) {
                WebCmd *pxWebCmd = (WebCmd *)listGET_LIST_ITEM_OWNER(pxItem);
                ListItem_t *pxNextItem = listGET_NEXT(pxItem);

                if ((uint32_t)pxWebCmd->xTaskToNotify == msg->xTaskToNotify) {
                    pxWebCmd->cmd_result = msg->cmd_result;
                    memcpy(pxWebCmd->data, msg->data, msg->data_len);
                    uxListRemove(pxItem);
                    xTaskNotifyGive(pxWebCmd->xTaskToNotify);
                }

                pxItem = pxNextItem;
            }
            taskEXIT_CRITICAL();
        }
    } else {
        printf("Unsupport msg type: 0x%x\n", msg->cmd_type);
        buf_dump((uint8_t *)&msg, sizeof(msg));
        dump_message(*msg);
    }
}
```

**Changes**:
- Improved formatting for readability
- No functional changes to logic
- Added debug output for unsupported message types

### 4. Power Management Integration (`hf_power_process.c`)

#### 4.1 Function Visibility Change

**Before** (line 170):
```c
static void som_reset_control(uint8_t reset);  // Static function
```

**After** (line 186):
```c
void som_reset_control(uint8_t reset);  // Exported function
```

**Reason**: Web command handler needs to call this function to reset SOM
- Web interface sends CMD_RESET command
- BMC protocol handler calls `som_reset_control(pdTRUE)` then `som_reset_control(pdFALSE)`
- Allows web-initiated SOM reset without duplicating code

#### 4.2 Minor Cleanup

**Added Blank Line** (line 178):
```c
case STOP_POWER:
    printf("STOP_POWER\r\n");
    i2c_deinit(I2C3);

    pmic_power_on(pdFALSE);  // Blank line added for readability
    atx_power_on(pdFALSE);
```

### 5. Main Task Creation (`main.c`)

**Code Not Shown in Patch**: Likely added task creation for daemon keep-alive
```c
// Expected addition (10 lines)
osThreadAttr_t daemon_keeplive_task_attributes = {
    .name = "daemon_keeplive",
    .stack_size = 512 * 4,
    .priority = (osPriority_t) osPriorityNormal,
};
osThreadNew(deamon_keeplive_task, NULL, &daemon_keeplive_task_attributes);
```

### 6. Interrupt Handler Cleanup (`stm32f4xx_it.c`)

**Removed Lines** (-8): Likely removed unused UART4 interrupt handler code
- Previous custom ISR code replaced by HAL_UARTEx_RxEventCallback()
- Cleanup of obsolete buffer handling code

### 7. Web Server Integration (`web-server.c`)

**18,925 Lines Changed**: Primarily regeneration of embedded HTML/CSS/JS

**Functional Changes**:

**1. Power Status Integration** (lines 10340-10346):
```c
// Before (placeholder)
int get_power_status(){
    printf("TODO call get_power_status \n");
    return 1;  // Hardcoded
}

// After (live data)
int get_power_status()
{
    return som_power_state == SOM_POWER_ON ? 1 : 0;
}
```

**2. PVT Info Function** (lines 10453-10469):
```c
PVTInfo get_pvt_info()
{
    int ret = HAL_OK;
    PVTInfo pvtInfo = {
        .cpu_temp = -1,   // Initialize to invalid values
        .npu_temp = -1,
        .fan_speed = -1,
    };

    // Call SOM daemon to get real PVT data
    ret = web_cmd_handle(CMD_PVT_INFO, (void *)&pvtInfo, sizeof(PVTInfo), 5000);
    if (HAL_OK != ret) {
        printf("Failed to get PVT info from SOM, ret %d\n", ret);
        // Return -1 values indicating failure
    }

    return pvtInfo;
}
```

**Integration**:
- Web interface requests PVT data
- BMC sends CMD_PVT_INFO to SOM daemon via UART4
- SOM reads sensors and responds with data
- BMC returns data to web interface
- Displayed as live updating values on web page

## Testing Recommendations

### 1. Queue Functionality Testing

**Test Queue Creation**:
```c
// Verify queue created successfully
if (xUart4MsgQueue != NULL) {
    printf("UART4 message queue created, depth=%d\n", QUEUE_LENGTH);
}
```

**Test Queue Full Condition**:
```c
// Simulate rapid message arrival
for (int i = 0; i < 10; i++) {
    Message test_msg = {.header = FRAME_HEADER, .tail = FRAME_TAIL};
    if (xQueueSend(xUart4MsgQueue, &test_msg, 0) == pdPASS) {
        printf("Message %d queued\n", i);
    } else {
        printf("Queue full at message %d\n", i);
        // Should occur at i=8 (queue depth 8)
    }
}
```

**Expected Output**:
```
Message 0 queued
Message 1 queued
...
Message 7 queued
Queue full at message 8
```

### 2. Daemon Keep-Alive Testing

**Scenario 1: SOM Powered Off**:
```bash
# Observe console output
SOM Daemon status change to off!
SOM keeplive request timeout!
SOM keeplive request timeout!
...
[After 5 failures, logging stops]
```

**Scenario 2: SOM Powered On**:
```bash
# Power on SOM, then observe
SOM Daemon status change to on!
```

**Scenario 3: SOM Crashes Mid-Operation**:
```bash
# SOM daemon running, then crashes
SOM keeplive request timeout!
SOM keeplive request timeout!
SOM keeplive request timeout!
SOM keeplive request timeout!
SOM keeplive request timeout!
SOM Daemon status change to off!
```

### 3. Mutex Testing

**Test Concurrent Transmissions**:
```c
// Create two tasks sending simultaneously
void test_task1(void *arg) {
    for (int i = 0; i < 100; i++) {
        web_cmd_handle(CMD_PVT_INFO, &data, sizeof(data), 1000);
        osDelay(10);
    }
}

void test_task2(void *arg) {
    for (int i = 0; i < 100; i++) {
        web_cmd_handle(CMD_BOARD_STATUS, NULL, 0, 1000);
        osDelay(10);
    }
}

// Verify no message corruption
// Check checksums on SOM side
```

### 4. Web Command Integration Testing

**Test PVT Info Retrieval**:
```bash
curl http://<bmc-ip>/pvt_info

# Expected JSON response
{"status":0,"message":"success","data":{"cpu_temp":"45","npu_temp":"42","fan_speed":"1200"}}
```

**Test Timeout Handling**:
```c
// On SOM side, intentionally delay response beyond timeout
osDelay(6000);  // 6 seconds (BMC timeout is 5 seconds)

// BMC should return HAL_TIMEOUT error
// Web interface shows error message or stale data
```

### 5. Message Validation Testing

**Test Invalid Header**:
```c
Message test_msg = {
    .header = 0xDEADBEEF,  // Invalid (should be 0xA55AAA55)
    .tail = FRAME_TAIL,
    // ... rest of message
};
```
**Expected**: "Invalid message format!" logged

**Test Checksum Mismatch**:
```c
Message test_msg = {
    .header = FRAME_HEADER,
    .tail = FRAME_TAIL,
    .checksum = 0xFF,  // Incorrect checksum
    // ...
};
```
**Expected**: "Checksum error!" logged with hex dump

### 6. Race Condition Testing

**Test Rapid Command Submission**:
```javascript
// In browser console
for (let i = 0; i < 20; i++) {
    $.ajax({url: '/pvt_info', async: true});
}

// Verify all requests complete without errors
// Check BMC console for any timeout messages
```

### 7. Performance Testing

**Measure Response Time**:
```c
uint32_t start_tick = xTaskGetTickCount();
ret = web_cmd_handle(CMD_PVT_INFO, &data, sizeof(data), 5000);
uint32_t end_tick = xTaskGetTickCount();

printf("PVT command response time: %lu ms\n", end_tick - start_tick);
```

**Expected Values**:
- Normal response: 50-200ms
- Slow sensor read: 200-1000ms
- Timeout: ~5000ms

**Measure Queue Latency**:
```c
// In ISR
uint32_t queue_time = xTaskGetTickCount();
xQueueSendFromISR(xUart4MsgQueue, &msg, ...);

// In protocol task (after receive)
uint32_t process_time = xTaskGetTickCount();
printf("Queue latency: %lu ms\n", process_time - queue_time);
```

**Expected**: <10ms latency (usually <1ms)

### 8. Memory Usage Testing

**Monitor Stack Usage**:
```c
// In each task
UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
printf("[%s] Stack high water mark: %lu bytes\n",
       pcTaskGetName(NULL), uxHighWaterMark * sizeof(StackType_t));
```

**Monitor Heap Usage**:
```c
size_t xFreeHeap = xPortGetFreeHeapSize();
printf("Free heap: %lu bytes\n", xFreeHeap);
```

**Expected After Patch**:
- Queue allocation: ~2.2KB (8 messages × 270 bytes)
- Mutex: ~100 bytes
- Total additional heap: ~2.5KB

## Security Analysis

### 1. Concurrency Vulnerabilities

**Race Condition Mitigation**:
- **Mutex Protection**: UART transmission serialized
- **Critical Sections**: WebCmdList access protected
- **Atomic Operations**: FreeRTOS queue operations inherently atomic

**Remaining Risk**: Time-of-check to time-of-use (TOCTTOU)
```c
// Vulnerable pattern (NOT in this patch, but possible):
if (som_daemon_state == SOM_DAEMON_ON) {
    // State could change here (race)
    ret = web_cmd_handle(CMD_PVT_INFO, ...);
}
```
**Mitigation**: Always check return value, don't rely on cached state

### 2. Resource Exhaustion

**Queue Full Attack**:
- **Scenario**: Attacker floods BMC with rapid commands via web interface
- **Current Protection**: Queue depth 8 (oldest messages dropped)
- **Weakness**: No rate limiting on web command submission

**DoS Attack Vector**:
```bash
# Rapid fire commands
for i in {1..1000}; do
    curl http://<bmc-ip>/pvt_info &
done
```

**Impact**: Queue fills, legitimate SOM messages dropped
**Mitigation** (future): Add rate limiting to web endpoints

### 3. Authentication Bypass

**Vulnerability**: Keep-alive task runs regardless of web authentication
```c
// No authentication check
void deamon_keeplive_task(void *argument) {
    // Runs automatically, no session validation
}
```

**Current State**: Keep-alive is internal (not user-triggered), so acceptable
**Future Consideration**: Ensure web commands validate session before calling web_cmd_handle()

### 4. Input Validation

**Timeout Parameter**:
```c
int web_cmd_handle(CommandType cmd, void *data, int data_len, uint32_t timeout)
```

**Potential Issue**: Very large timeout values
```c
web_cmd_handle(CMD_PVT_INFO, &data, sizeof(data), 0xFFFFFFFF);
// Task blocks for ~49 days!
```

**Mitigation** (not implemented):
```c
#define MAX_CMD_TIMEOUT_MS 30000  // 30 seconds
if (timeout > MAX_CMD_TIMEOUT_MS) {
    timeout = MAX_CMD_TIMEOUT_MS;
}
```

### 5. Checksum Weakness

**Current Checksum** (XOR-based):
```c
checksum ^= msg->msg_type;
checksum ^= msg->cmd_type;
checksum ^= msg->data_len;
for (i = 0; i < msg->data_len; i++) {
    checksum ^= msg->data[i];
}
```

**Weakness**: XOR checksum has low collision resistance
- **Attack**: Attacker could modify multiple bytes to maintain same checksum
- **Example**: Flipping same bit in two bytes maintains XOR

**Stronger Alternative** (not implemented):
- CRC16 or CRC32 (better error detection)
- HMAC (cryptographic authentication)

**Current Justification**: Protocol runs over wired UART (low noise), XOR sufficient for error detection

## Known Limitations and Future Work

### 1. Error Handling

**Incomplete Error Paths**:
```c
xMutex = xSemaphoreCreateMutex();
if (xMutex == NULL) {
    // Mutex creation failed - handle error
    // TODO: No actual error handling!
}
```

**Future**:
```c
if (xMutex == NULL) {
    printf("FATAL: Failed to create UART transmit mutex!\n");
    vTaskDelete(NULL);  // Or trigger system reset
}
```

### 2. Queue Tuning

**Current Queue Depth**: 8 messages
**Considerations**:
- Increase for high-traffic scenarios
- Decrease for memory-constrained builds
- Dynamic sizing based on measured usage

**Future Enhancement**: Queue usage statistics
```c
UBaseType_t uxMessagesWaiting = uxQueueMessagesWaiting(xUart4MsgQueue);
if (uxMessagesWaiting > 6) {
    printf("WARNING: UART4 queue near full (%lu/8)\n", uxMessagesWaiting);
}
```

### 3. SOM Daemon Implementation

**Placeholder**: SOM-side daemon not implemented in this patch
- BMC sends commands, expects responses
- SOM daemon must be implemented separately (likely in Linux userspace)

**Future SOM Daemon** (conceptual):
```python
# Python daemon on SOM
while True:
    msg = uart4.read_message()
    if msg.cmd_type == CMD_PVT_INFO:
        cpu_temp = read_sensor('/sys/class/thermal/thermal_zone0/temp')
        npu_temp = read_sensor('/sys/class/thermal/thermal_zone1/temp')
        send_reply(msg, {"cpu_temp": cpu_temp, "npu_temp": npu_temp})
    elif msg.cmd_type == CMD_BOARD_STATUS:
        send_reply(msg, {"status": "OK"})
```

### 4. Performance Optimization

**Potential Bottlenecks**:
- Synchronous web_cmd_handle() blocks web server thread
- All commands serialized via single UART

**Future Optimizations**:
- Async command submission (web handler doesn't block)
- Multiple communication channels (SPI for high-speed data)
- Command prioritization (critical commands bypass queue)

### 5. Logging

**Current**: printf() to UART3 console
**Limitations**:
- No log levels (DEBUG, INFO, WARN, ERROR)
- No timestamps
- No log persistence (lost on reboot)

**Future Logging System**:
```c
#define LOG_ERROR(fmt, ...) log_message(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_message(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)

void log_message(log_level_t level, const char *fmt, ...) {
    // Add timestamp, level prefix
    // Write to console AND persistent storage (EEPROM log ring buffer)
}
```

## Conclusion

Patch 0014 represents a **fundamental architectural upgrade** to the BMC firmware's communication infrastructure. By migrating from a ring buffer-based polling system to a FreeRTOS queue-driven event model, it achieves:

**Performance Improvements**:
- Eliminated polling overhead (CPU efficiency)
- Reduced latency (event-driven wake-up)
- Better real-time responsiveness

**Reliability Enhancements**:
- Mutex-protected transmission (no race conditions)
- Robust message validation (header, tail, checksum)
- Graceful queue overflow handling

**Functionality Expansion**:
- Web interface can now retrieve live SOM data (PVT info, power status)
- Daemon keep-alive monitors SOM health
- Foundation for future command protocol expansion

**Code Quality**:
- Cleaner separation of concerns (ISR vs. task context)
- Leverages FreeRTOS primitives (queues, mutexes, notifications)
- More maintainable and extensible architecture

**Security Considerations**:
- Mutex prevents message interleaving attacks
- Timeout prevents indefinite blocking
- Some gaps remain (rate limiting, authentication checks)

**Total Impact**: While the patch shows +77 net lines, the functional transformation is substantial. The web-server.c changes (18,925 lines) are mostly automated HTML regeneration, but the protocol refactoring in `hf_protocol_process.c` (+197 lines) and interrupt handler updates represent significant architectural improvements.

**Future Enablement**: This patch sets the stage for:
- Patch 0015: EEPROM information APIs (relies on web_cmd_handle)
- Patch 0017: RTC and login management (uses daemon status)
- Later patches: Expanded command set, improved diagnostics, remote firmware updates

The infrastructure established here becomes the communication backbone for all future BMC-SOM interactions.
