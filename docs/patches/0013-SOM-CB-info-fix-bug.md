# Patch 0013: Add SOM/CB Info and Fix Bugs

## Metadata
- **Patch File**: `0013-WIN2030-15279-fix-bmc-add-som-cd-info-fix-bug.patch`
- **Author**: yuan junhui <yuanjunhui@eswincomputing.com>
- **Date**: Thu, 9 May 2024 11:17:11 +0800
- **Ticket**: WIN2030-15279
- **Type**: fix (Bug fix with feature additions)
- **Change-Id**: I3dea439a8804722887c3931a59d74b934be21df3

## Summary

This patch enhances the web interface with board information display capabilities and resolves critical usability issues. It introduces two new AJAX endpoints for retrieving SOM (System-on-Module) and Carrier Board information, improves the DIP switch configuration UI by converting from text inputs to radio buttons, and fixes a race condition where power status buttons remained disabled during data loading.

## Changelog

The official changelog from the commit message:
1. **Add boardinfo sominfo, bcinfo** - Introduces structured board information retrieval
2. **Add format dip-switch redio** - Converts DIP switch UI from text fields to radio button controls
3. **Fixbug power on change, disabled before getdata** - Resolves button disable state during async loading

## Statistics

- **Files Changed**: 1 file
- **File Modified**: `Core/Src/web-server.c`
- **Insertions**: 222 lines
- **Deletions**: 63 lines
- **Net Change**: +159 lines

This represents a substantial enhancement to the web interface with improved user experience and additional hardware information display.

## Detailed Changes

### 1. Board Information Infrastructure

#### 1.1 SOM (System-on-Module) Information Structure

**New Data Structure**:
```c
typedef struct {
    int magicNumber;
    int formatVersionNumber;
    int productIdentifier;
    int pcbRevision;
    char boardSerialNumber[18];
} SOMSimpleInfo;
```

**Purpose**: Encapsulates essential identification and version information for the System-on-Module.

**Fields Explained**:
- **magicNumber**: Validation marker to ensure EEPROM data integrity (e.g., 0xDEADBEEF)
- **formatVersionNumber**: Structure version for forward/backward compatibility
- **productIdentifier**: Unique product SKU or model number
- **pcbRevision**: PCB hardware revision (e.g., A, B, C, DVB2)
- **boardSerialNumber**: Unique serial number string (up to 17 characters + null terminator)

**Getter Function** (lines 309-319):
```c
SOMSimpleInfo get_som_info(){
    printf("TODO call get_som_info\n");
    SOMSimpleInfo example= {
        0,      // magicNumber
        1,      // formatVersionNumber
        2,      // productIdentifier
        3,      // pcbRevision
        "v1.10.1"  // boardSerialNumber
    };
    return example;
}
```

**Implementation Note**: This is a placeholder returning hardcoded example data. Future patches (notably patch 0015) will implement actual EEPROM reading functionality to retrieve real board information. The TODO comment indicates this is intentionally incomplete.

#### 1.2 Carrier Board Information Structure

**New Data Structure**:
```c
typedef struct {
    int magicNumber;
    int formatVersionNumber;
    int productIdentifier;
    int pcbRevision;
    char boardSerialNumber[18];
} CBSimpleInfo;  // CB = Carrier Board
```

**Design Note**: The structure is identical to SOMSimpleInfo, suggesting a unified board information format across both the SOM and carrier board. This design choice simplifies code reuse and maintains consistency.

**Getter Function** (lines 329-339):
```c
CBSimpleInfo get_cb_info(){
    printf("TODO call get_cb_info\n");
    CBSimpleInfo example= {
        0,      // magicNumber
        1,      // formatVersionNumber
        2,      // productIdentifier
        3,      // pcbRevision
        "v1.10.1"  // boardSerialNumber
    };
    return example;
}
```

**Future Implementation**: Like get_som_info(), this placeholder will be replaced with EEPROM access code to read actual carrier board identification data from non-volatile storage.

### 2. Web Interface Enhancements

#### 2.1 Board Information Display Section

**HTML Structure Added** (lines 166-202):

```html
<div>
    <h3>Board Information</h3>

    <!-- SOM Info Section -->
    <h4>SOM Info</h4>
    <div class="network-row">
        <label>magicNumber:</label>
        <input type="text" id="som_magicNumber" value="0" style="width: 100px;" disabled> <br>
    </div>
    <div class="network-row">
        <label>formatVersionNumber:</label>
        <input type="text" id="som_formatVersionNumber" value="0" style="width: 100px;" disabled> <br>
    </div>
    <div class="network-row">
        <label>productIdentifier:</label>
        <input type="text" id="som_productIdentifier" value="0" style="width: 100px;" disabled> <br>
    </div>
    <div class="network-row">
        <label>pcbRevision:</label>
        <input type="text" id="som_pcbRevision" value="0" style="width: 100px;" disabled> <br>
    </div>
    <div class="network-row">
        <label>boardSerialNumber:</label>
        <input type="text" id="som_boardSerialNumber" value="0" style="width: 100px;" disabled> <br>
    </div>
    <button id="board-info-som-refresh">refresh</button>

    <!-- Carrier Board Info Section -->
    <h4>CarrierBoard Info</h4>
    <div class="network-row">
        <label>magicNumber:</label>
        <input type="text" id="cb_magicNumber" value="0" style="width: 100px;" disabled> <br>
    </div>
    <!-- ... similar structure for CB fields ... -->
    <button id="board-info-cb-refresh">refresh</button>
</div>
```

**Design Decisions**:
- **Read-Only Fields**: All inputs are `disabled`, making them display-only (not editable by users)
- **Fixed Width**: 100px width provides consistent layout across all fields
- **Default Values**: Initialize to "0" to indicate data not yet loaded
- **Separate Refresh Buttons**: Individual buttons for SOM and CB allow independent data updates

#### 2.2 AJAX Handlers for Board Information

**SOM Information Retrieval** (lines 112-130):

```javascript
$('#board-info-som-refresh').click(function() {
    $.ajax({
        async: false,  // Synchronous request (blocks until complete)
        url: '/board_info_som',
        type: 'GET',
        success: function(response) {
            $('#som_magicNumber').val(response.data.magicNumber);
            $('#som_formatVersionNumber').val(response.data.formatVersionNumber);
            $('#som_productIdentifier').val(response.data.productIdentifier);
            $('#som_pcbRevision').val(response.data.pcbRevision);
            $('#som_boardSerialNumber').val(response.data.boardSerialNumber);
            console.log('board-info-som-refresh refreshed successfully.');
        },
        error: function(xhr, status, error) {
            console.error('Error refreshing board-info-som-refresh:', error);
        }
    });
});
```

**Key Implementation Details**:
- **Synchronous AJAX**: `async: false` ensures request completes before page becomes interactive
  - **Rationale**: Prevents user interaction with partially loaded data
  - **Trade-off**: Blocks UI during request (acceptable for fast local BMC response)
- **Direct JSON Access**: `response.data.magicNumber` assumes JSON response structure
- **Error Logging**: Errors logged to console for developer debugging

**Carrier Board Information Retrieval** (lines 131-149):

Similar structure to SOM handler, targeting `/board_info_cb` endpoint and updating `cb_*` form fields.

**Auto-Refresh on Page Load** (lines 157-158):

```javascript
$('#board-info-som-refresh').click();
$('#board-info-cb-refresh').click();
```

**Purpose**: Automatically triggers data retrieval when page loads, ensuring board information is immediately visible without requiring user to click refresh buttons.

### 3. Backend API Endpoints

#### 3.1 GET /board_info_som Endpoint

**Implementation** (lines 372-398):

```c
}else if(strncmp(path, "/board_info_som",15)==0 ){
    printf("get ,location: /board_info_som \n");
    SOMSimpleInfo simpleInfo=get_som_info();

    char json_response[BUF_SIZE_256]={0};
    // Create JSON format string
    char *json_response_patt = "{\"status\":0,\"message\":\"success\",\"data\":{\"magicNumber\":\"%d\",\"formatVersionNumber\":\"%d\",\"productIdentifier\":\"%d\",\"pcbRevision\":\"%d\",\"boardSerialNumber\":\"%s\"}}";
    sprintf(json_response, json_response_patt, simpleInfo.magicNumber, simpleInfo.formatVersionNumber, simpleInfo.productIdentifier, simpleInfo.pcbRevision, simpleInfo.boardSerialNumber);

    // Send HTTP headers
    const char *header = "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json\r\n"
                        "Connection: close\r\n"
                        "Content-Length: %d\r\n\r\n";

    char response_header[BUF_SIZE_512];
    sprintf(response_header, header, strlen(json_response));

    printf("strlen(response_header):%d,strlen(json_response):%d \n", strlen(response_header), strlen(json_response));

    netconn_write(conn, response_header, strlen(response_header), NETCONN_COPY);
    netconn_write(conn, json_response, strlen(json_response), NETCONN_COPY);
}
```

**Response Format**:
```json
{
    "status": 0,
    "message": "success",
    "data": {
        "magicNumber": "0",
        "formatVersionNumber": "1",
        "productIdentifier": "2",
        "pcbRevision": "3",
        "boardSerialNumber": "v1.10.1"
    }
}
```

**Security Considerations**:
- **No Authentication Check**: Endpoint does not validate session or credentials
  - **Vulnerability**: Unauthenticated users can retrieve board information
  - **Mitigation** (future patch): Add session validation before returning data
- **Buffer Overflow Risk**: Fixed-size `json_response[BUF_SIZE_256]` buffer
  - **Risk**: Long board serial numbers could overflow buffer
  - **Current Safety**: 18-char limit on boardSerialNumber prevents overflow
  - **Improvement**: Use dynamic allocation or add bounds checking

**HTTP Header Construction**:
- **Content-Type**: `application/json` signals JSON response to client
- **Connection: close**: HTTP/1.0 style connection management (close after response)
- **Content-Length**: Dynamic calculation ensures browser knows exact response size

#### 3.2 GET /board_info_cb Endpoint

**Implementation** (lines 400-422):

Nearly identical to `/board_info_som` endpoint, calling `get_cb_info()` instead and populating `cb_*` JSON fields. Code duplication suggests opportunity for refactoring with a generic board info function.

**Potential Refactoring**:
```c
// Future improvement
void send_board_info_response(struct netconn *conn, void *info, const char *field_prefix) {
    // Generic implementation
}
```

### 4. DIP Switch UI Enhancement

#### 4.1 Problem Statement

**Original Implementation** (deleted lines 24-35, 43-50):

```javascript
// Old approach: Text input fields
$('#dip01').val(response.data.dip01);
$('#dip02').val(response.data.dip02);
// ... dip03-dip08

var dip01 = $('#dip01').val();
var dip02 = $('#dip02').val();
// ... dip03-dip08
```

```html
<!-- Old HTML -->
dip01:<input type="text" id="dip01" style="width: 60px;" disabled> <br>
dip02:<input type="text" id="dip02" style="width: 60px;" disabled> <br>
<!-- ... dip03-dip08 -->
```

**Issues with Original Approach**:
1. **Ambiguous Values**: Text fields could contain any value ("0", "1", "true", "false", "on", "off", etc.)
2. **No Visual Feedback**: Users couldn't easily see ON/OFF state at a glance
3. **Prone to Errors**: Copy-paste errors or typos could introduce invalid states
4. **Inconsistent Representation**: No standardized display format

#### 4.2 Improved Implementation

**New Radio Button Approach** (lines 236-255):

```html
<div class="network-row">
    <label>dip01:</label>
    <input type="radio" name="dip01" value="0" id="dip01-false">
    <label for="dip01-false">close</label>
    <input type="radio" name="dip01" value="1" id="dip01-true">
    <label for="dip01-true">open</label>
</div>
<div class="network-row">
    <label>dip02:</label>
    <input type="radio" name="dip02" value="0" id="dip02-false">
    <label for="dip02-false">close</label>
    <input type="radio" name="dip02" value="1" id="dip02-true">
    <label for="dip02-true">open</label>
</div>
<!-- Similar for dip03 and dip04 -->
```

**Benefits**:
- **Binary Choice**: Radio buttons enforce exactly two states (0 or 1)
- **Visual Clarity**: Clearly shows which state is selected
- **Mutually Exclusive**: HTML radio button semantics prevent both being selected
- **Accessibility**: Properly labeled for screen readers and keyboard navigation

**JavaScript Update** (lines 32-35, 51-54):

```javascript
// New approach: Radio button selection
$('input[name="dip01"][value="' + response.data.dip01 + '"]').prop('checked', true);
$('input[name="dip02"][value="' + response.data.dip02 + '"]').prop('checked', true);
$('input[name="dip03"][value="' + response.data.dip03 + '"]').prop('checked', true);
$('input[name="dip04"][value="' + response.data.dip04 + '"]').prop('checked', true);

var dip01 = $('input[name="dip01"]:checked').val();
var dip02 = $('input[name="dip02"]:checked').val();
var dip03 = $('input[name="dip03"]:checked').val();
var dip04 = $('input[name="dip04"]:checked').val();
```

**jQuery Selector Breakdown**:
- `$('input[name="dip01"][value="' + response.data.dip01 + '"]')`:
  - Selects radio button with name="dip01" AND value matching data from server
  - Attribute selector syntax: `[attribute="value"]`
- `.prop('checked', true)`: Sets the HTML `checked` property (not attribute)
  - **prop() vs attr()**: `prop()` modifies DOM property (correct for checked state)
- `$('input[name="dip01"]:checked')`: Selects the currently checked radio button
  - `:checked` pseudo-selector filters to checked state only

#### 4.3 DIP Switch Count Reduction

**Code Changes** (lines 268-275, 279-289, 426-440):

**Before**:
```c
typedef struct {
    int dip01;
    int dip02;
    int dip03;
    int dip04;
    int dip05;  // Removed
    int dip06;  // Removed
    int dip07;  // Removed
    int dip08;  // Removed
} DIPSwitchInfo;
```

**After**:
```c
typedef struct {
    int dip01;
    int dip02;
    int dip03;
    int dip04;
} DIPSwitchInfo;
```

**Rationale for Reduction**:
- **Hardware Limitation**: Only 4 DIP switches actually used in current board revision
- **Simplification**: Reduces memory footprint and code complexity
- **Future Extensibility**: Can be expanded if future boards require more switches

**Default Values Change**:
```c
// Old defaults
dipSwitchInfo.dip05 = 1;
dipSwitchInfo.dip06 = 1;
dipSwitchInfo.dip07 = 1;
dipSwitchInfo.dip08 = 1;

// New defaults (dip03/dip04 changed from 0 to 1)
dipSwitchInfo.dip03 = 1;
dipSwitchInfo.dip04 = 1;
```

**JSON Response Updated** (lines 350-355):

```c
// Old pattern
char *json_response_patt = "{\"status\":0,\"message\":\"success\",\"data\":{\"dip01\":\"%d\",\"dip02\":\"%d\",\"dip03\":\"%d\",\"dip04\":\"%d\",\"dip05\":\"%d\",\"dip06\":\"%d\",\"dip07\":\"%d\",\"dip08\":\"%d\"}}";

// New pattern (4 switches only)
char *json_response_patt = "{\"status\":0,\"message\":\"success\",\"data\":{\"dip01\":\"%d\",\"dip02\":\"%d\",\"dip03\":\"%d\",\"dip04\":\"%d\"}}";
```

### 5. Power Status Button Fix

#### 5.1 Problem Description

**Original Behavior**:
- When user clicked "refresh" on power status, button text remained "unknown"
- Button remained enabled during AJAX request
- If user clicked button multiple times rapidly, multiple concurrent requests could occur
- UI felt unresponsive and didn't provide feedback that data was loading

#### 5.2 Solution Implementation

**Power On Status Loading State** (lines 74-77, 85):

```javascript
$('#power-on-refresh').click(function() {
    $('#power-on-status').val('loading,,,');  // Visual feedback
    $('#power-on-change').prop("disabled", true);  // Prevent multiple clicks
    $('#power-on-change').text('loading,,,');  // Button shows loading state

    $.ajax({
        async: false,
        url: '/power_status',
        type: 'GET',
        contentType: 'application/x-www-form-urlencoded',
        success: function(response) {
            $('#power-on-change').prop("disabled", false);  // Re-enable button
            if(response.data.power_status==="0"){
                $('#power-on-status').val('powerOFF');
                $('#power-on-change').text('powerON');
            } else {
                $('#power-on-status').val('powerON');
                $('#power-on-change').text('powerOFF');
            }
            // ... error handling
        }
    });
});
```

**State Flow**:
1. **Before Request**:
   - Display field: "loading,,,"
   - Button: disabled, text "loading,,,"
2. **During Request**:
   - User cannot click button (disabled)
   - Visual feedback shows something is happening
3. **After Success**:
   - Button re-enabled
   - Display shows actual power state
   - Button text shows next action ("powerON" or "powerOFF")

**Power Retention Status Loading State** (lines 93-96, 102):

Identical pattern applied to power retention status:
```javascript
$('#power-retention-refresh').click(function() {
    $('#power-retention-status').val('loading,,,');
    $('#power-retention-change').prop("disabled", true);
    $('#power-retention-change').text('loading,,,');
    // ... AJAX request
    $('#power-retention-change').prop("disabled", false);  // Re-enable after success
});
```

**Default HTML State** (lines 207, 211, 216, 220):

```html
<!-- Initial state shows "loading,,," -->
<label>Power Status:</label>
<input type="text" id="power-on-status" value="loading,,," style="width: 100px;" disabled>

<label>Power Status Change:</label>
<button id="power-on-change" disabled>loading,,,</button>

<label>Power Retention:</label>
<input type="text" id="power-retention-status" value="loading,,," style="width: 100px;" disabled>

<label>Power Retention Change:</label>
<button id="power-retention-change" disabled>loading,,,</button>
```

**UX Improvement**: Page loads with buttons already showing "loading,,," state, signaling to user that data is being fetched. This is better than showing "unknown" or "0" which might be interpreted as actual system state.

### 6. Minor Fixes and Improvements

#### 6.1 jQuery Library Size Correction

**Change** (lines 363-364, 367):

```c
// Before
sprintf(response_header, http_jquery_200_patt, strlen(data_jquery_min_js)-91);
netconn_write(conn, data_jquery_min_js, strlen(data_jquery_min_js)-91, NETCONN_COPY);

// After
sprintf(response_header, http_jquery_200_patt, strlen(data_jquery_min_js)-90);
netconn_write(conn, data_jquery_min_js, strlen(data_jquery_min_js)-90, NETCONN_COPY);
```

**Explanation**:
- jQuery library embedded as C string array ends with extra null terminator or padding
- `-91` was incorrect offset, causing last character to be truncated
- `-90` is the correct offset to exclude null terminator and padding
- **Impact**: Prevents potential JavaScript syntax error from incomplete jQuery library

#### 6.2 Button Order Swap

**Change** (line 264):

```html
<!-- Before -->
<button id="net-work-update">update</button> <button id="net-work-refresh">refresh</button>

<!-- After -->
<button id="net-work-refresh">refresh</button> <button id="net-work-update">update</button>
```

**Rationale**:
- Standard UX pattern: read operations (refresh) before write operations (update)
- Reduces accidental clicks on destructive "update" button
- Consistency with other sections (DIP switch already had refresh before update)

## Integration Points

### Data Flow Architecture

```
User Browser
    |
    | HTTP GET /board_info_som
    v
Web Server (web-server.c)
    |
    | Call get_som_info()
    v
Placeholder Function (returns hardcoded data)
    |
    | [Future: EEPROM read via I2C]
    v
JSON Response
    |
    | response.data.magicNumber, etc.
    v
jQuery Updates DOM
```

**Future Enhancement Path** (Patch 0015):
```
get_som_info()
    |
    v
es_get_carrier_board_info()  [Patch 0015 function]
    |
    v
hf_i2c_mem_read(&hi2c1, AT24C_ADDR, offset, ...)
    |
    v
AT24C256 EEPROM (via I2C1 bus)
```

### Session Management Gap

**Current State**: Board information endpoints have no authentication
```c
else if(strncmp(path, "/board_info_som",15)==0 ){
    // No session check!
    SOMSimpleInfo simpleInfo=get_som_info();
    // ... send response
}
```

**Security Implication**:
- Any user on network can retrieve board serial numbers and revision information
- Could aid reconnaissance for targeted attacks
- Violates principle of least privilege

**Future Fix** (Later Patches):
```c
else if(strncmp(path, "/board_info_som",15)==0 ){
    if(found_session_user_name == NULL || strlen(found_session_user_name) == 0) {
        send_redirect(conn, "/login.html", NULL);
        return;
    }
    // Authenticated user: proceed with info retrieval
}
```

## Testing Recommendations

### 1. Functional Testing

**Board Information Display**:
```bash
# Test SOM info endpoint directly
curl http://<bmc-ip>/board_info_som

# Expected response
{"status":0,"message":"success","data":{"magicNumber":"0","formatVersionNumber":"1","productIdentifier":"2","pcbRevision":"3","boardSerialNumber":"v1.10.1"}}

# Test CB info endpoint
curl http://<bmc-ip>/board_info_cb

# Verify same structure
```

**Web Interface Testing**:
1. Load `/info.html` in browser
2. Verify "Board Information" section appears with two subsections
3. Check that SOM and CB fields populate automatically on page load
4. Click "refresh" buttons and verify data updates
5. Inspect browser console for any errors

**DIP Switch UI**:
1. Navigate to DIP Switch section
2. Verify radio buttons display (not text inputs)
3. Click refresh, verify correct radio button selected based on response
4. Toggle radio buttons and click update
5. Refresh and verify selection persists

### 2. Power Status Testing

**Loading State Verification**:
```javascript
// In browser console, slow down AJAX for testing
$('#power-on-refresh').click(function() {
    $('#power-on-status').val('loading,,,');
    $('#power-on-change').prop("disabled", true);
    $('#power-on-change').text('loading,,,');

    // Manually add delay for testing
    setTimeout(function() {
        $.ajax({
            // ... original AJAX call
        });
    }, 2000);  // 2 second delay to observe loading state
});
```

**Test Cases**:
1. Click power-on-refresh button
2. Immediately verify:
   - Display shows "loading,,,"
   - Button is disabled (grayed out)
   - Button text is "loading,,,"
3. After response:
   - Button becomes enabled
   - Text changes to "powerON" or "powerOFF"
4. Rapid click test: Click refresh button 5 times quickly
   - Should not trigger 5 concurrent requests (button disabled prevents this)

### 3. Regression Testing

**Verify Unchanged Functionality**:
- Login/logout flow still works
- Network configuration page functional
- PVT info retrieval still operational
- Other info.html sections not affected by changes

**DIP Switch Backward Compatibility**:
```bash
# POST to dip_switch endpoint with old 8-switch format
curl -X POST http://<bmc-ip>/dip_switch \
     -d "dip01=1&dip02=0&dip03=1&dip04=0&dip05=1&dip06=1&dip07=1&dip08=1"

# Should handle gracefully (ignore dip05-08)
```

### 4. Security Testing

**Unauthenticated Access Test**:
```bash
# Without session cookie
curl -v http://<bmc-ip>/board_info_som

# Current behavior: Returns data (VULNERABLE)
# Expected future behavior: 302 redirect to login
```

**Buffer Overflow Test**:
```c
// Simulate very long board serial number
SOMSimpleInfo test_info = {
    .boardSerialNumber = "This_is_a_very_long_serial_number_exceeding_18_chars_OVERFLOW"
};

// Test if JSON response buffer overflows
// Current: Should be caught by 18-char limit in struct
// Future: Add explicit validation
```

### 5. Cross-Browser Testing

Test on multiple browsers to ensure radio button behavior is consistent:
- Chrome/Chromium
- Firefox
- Safari
- Edge

**Specific Test**: Radio button jQuery selectors
```javascript
// Verify this works across browsers
$('input[name="dip01"][value="1"]').prop('checked', true);
```

## Known Limitations and Future Work

### 1. Placeholder Implementation

**Current State**:
- `get_som_info()` returns hardcoded example data
- `get_cb_info()` returns hardcoded example data

**Future Implementation** (Patch 0015):
- Read from EEPROM via I2C
- Parse binary structures
- Validate CRC32 checksums
- Handle invalid/corrupt EEPROM data gracefully

### 2. Missing Authentication

**Vulnerability**:
```
GET /board_info_som HTTP/1.1
Host: <bmc-ip>
[No authentication required]

HTTP/1.1 200 OK
[Board serial numbers exposed]
```

**Fix in Later Patches**:
- Add session validation to all API endpoints
- Redirect to login if no valid session
- Implement proper CSRF protection

### 3. DIP Switch Limitations

**Current Constraints**:
- Only 4 switches supported (dip01-dip04)
- No validation of switch combinations
- No description of what each switch controls

**Future Enhancements**:
- Add tooltips explaining each DIP switch function
- Implement validation (e.g., certain combinations invalid)
- Support for 8 switches if future hardware requires it

### 4. Error Handling Gaps

**Missing Error Conditions**:
```javascript
error: function(xhr, status, error) {
    console.error('Error refreshing board-info-som-refresh:', error);
    // No user-visible error message!
    // Fields remain showing old data or "0"
}
```

**Improvement**:
```javascript
error: function(xhr, status, error) {
    $('#som_magicNumber').val('ERROR');
    $('#som_formatVersionNumber').val('ERROR');
    // ... set all fields to indicate error
    alert('Failed to retrieve SOM information: ' + error);
}
```

### 5. Race Conditions

**Potential Issue**:
If page loads slowly, auto-refresh clicks might execute before DOM is fully ready:

```javascript
$(document).ready(function() {
    // ... all AJAX handler definitions

    // Auto-refresh triggers
    $('#board-info-som-refresh').click();  // What if button doesn't exist yet?
});
```

**Current Mitigation**: jQuery's `$(document).ready()` ensures DOM is loaded
**Future Improvement**: Add explicit checks or use promises

## Architectural Considerations

### 1. Code Duplication

**Observation**: `/board_info_som` and `/board_info_cb` endpoints are nearly identical

**Refactoring Opportunity**:
```c
// Generic helper function
void send_board_info_json(struct netconn *conn, void *info_struct,
                          int magic, int ver, int prod, int pcb, char *serial) {
    char json_response[BUF_SIZE_256];
    sprintf(json_response,
            "{\"status\":0,\"message\":\"success\",\"data\":"
            "{\"magicNumber\":\"%d\",\"formatVersionNumber\":\"%d\","
            "\"productIdentifier\":\"%d\",\"pcbRevision\":\"%d\","
            "\"boardSerialNumber\":\"%s\"}}",
            magic, ver, prod, pcb, serial);

    send_json_response(conn, json_response);
}

// Simplified endpoint handlers
else if(strcmp(path, "/board_info_som")==0) {
    SOMSimpleInfo info = get_som_info();
    send_board_info_json(conn, &info, info.magicNumber, info.formatVersionNumber,
                         info.productIdentifier, info.pcbRevision, info.boardSerialNumber);
}
```

### 2. JSON Formatting

**Current Approach**: Manual sprintf() string construction
```c
sprintf(json_response, "{\"status\":0,\"message\":\"success\",\"data\":{...}}");
```

**Risks**:
- Prone to syntax errors (missing quotes, commas)
- Difficult to maintain as responses grow complex
- No automatic escaping of special characters in strings

**Future Consideration**: Use a lightweight JSON library
- cJSON (very small footprint)
- jsmn (JSON parser, would need separate formatter)
- Or implement minimal JSON builder helper functions

### 3. Memory Management

**Buffer Allocation**:
```c
char json_response[BUF_SIZE_256]={0};  // 256 bytes on stack
char response_header[BUF_SIZE_512];     // 512 bytes on stack
```

**Stack Usage Analysis**:
- Multiple 256-512 byte buffers per request
- Nested function calls could accumulate stack usage
- STM32F407 has 192KB RAM, but stack size limited per task

**Recommendation**: Monitor stack usage with FreeRTOS tools
```c
UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
printf("HTTP handler stack remaining: %lu bytes\n", uxHighWaterMark * sizeof(StackType_t));
```

## Security Analysis

### Vulnerability Assessment

**1. Unauthenticated Information Disclosure** (HIGH SEVERITY)
- **CVE Potential**: Yes
- **Attack Vector**: Network-accessible API endpoint
- **Exploit**:
  ```bash
  curl http://<bmc-ip>/board_info_som
  curl http://<bmc-ip>/board_info_cb
  # Retrieves serial numbers, revisions without authentication
  ```
- **Impact**: Reconnaissance information for targeted attacks
- **Mitigation**: Add session validation (implemented in patch 0017)

**2. Prototype Pollution via DIP Switch** (LOW SEVERITY)
- **Attack Vector**: POST to `/dip_switch` with extra parameters
- **Example**:
  ```
  POST /dip_switch
  dip01=1&dip02=0&__proto__[isAdmin]=true
  ```
- **Current Protection**: C backend, not JavaScript - not vulnerable to prototype pollution
- **Actual Risk**: Ignored extra parameters (dip05-08 removed), benign

**3. Loading State Race Condition** (LOW SEVERITY)
- **Scenario**: User clicks power button rapidly during loading state
- **Current Protection**: Button disabled during AJAX request
- **Remaining Risk**: If JavaScript error occurs, button might remain disabled forever
- **Mitigation**: Add timeout to re-enable button after max expected response time

## Conclusion

Patch 0013 represents a significant usability enhancement to the BMC web interface, adding essential board information display and improving the user experience for power status and DIP switch configuration. The conversion from text inputs to radio buttons for DIP switches demonstrates attention to user interface design principles.

However, the patch also highlights ongoing security concerns, particularly the lack of authentication on new API endpoints. The placeholder nature of the board information retrieval functions indicates this is part of a larger development effort, with full EEPROM integration to come in subsequent patches.

**Key Achievements**:
- Structured board information retrieval framework
- Improved DIP switch UI with radio buttons
- Better user feedback during async operations (loading states)
- Foundation for EEPROM-backed board identification

**Remaining Work** (addressed in later patches):
- Implement actual EEPROM reading (patch 0015)
- Add authentication to board info endpoints (patch 0017)
- Error handling and validation improvements
- Code refactoring to reduce duplication

**Total Impact**: +159 lines, transforming web interface from basic configuration tool into comprehensive board information display system.
