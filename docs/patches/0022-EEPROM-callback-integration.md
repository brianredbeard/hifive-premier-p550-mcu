# Patch 0022: EEPROM Callback Integration in Web Server

## Metadata
- **Patch File**: `0022-WIN2030-15099-feat-web-server-Add-eeprom-callback-to.patch`
- **Author**: linmin <linmin@eswincomputing.com>
- **Date**: Mon, 13 May 2024 14:15:17 +0800
- **Ticket**: WIN2030-15099
- **Type**: feat (Feature enhancement)
- **Change-Id**: I05db0770e10d1e9e1bcd7aa017e176a19d7abee0

## Summary

This patch completes the integration between the web server interface and the EEPROM backend storage layer. Previously, web server callback functions returned placeholder responses with hardcoded data. This patch connects these callbacks to actual EEPROM read/write functions in `hf_common.c`, enabling persistent storage of user credentials, network configuration, DIP switch settings, and board information.

## Changelog

From the commit message:
1. **Call the EEPROM API in web-server callback** - Replace TODO stubs with actual EEPROM function calls
2. **Examples**: `save_sys_username_password()`, network configuration, DIP switch control, board info retrieval

## Statistics

- **Files Changed**: 3 files
- **Insertions**: 115 lines
- **Deletions**: 35 lines
- **Net Change**: +80 lines

### File Breakdown
- `Core/Inc/hf_common.h`: +2, -2 (function signature updates)
- `Core/Src/hf_common.c`: +34, -2 (new utility functions)
- `Core/Src/web-server.c`: +79, -31 (implement callbacks)

## Detailed Analysis

### 1. Username/Password Storage Integration

**Function Renaming for Clarity**:

Before (ambiguous naming):
```c
int es_get_admin_info(char *p_admin_name, char *p_admin_password);
int es_set_admin_info(char *p_admin_name, char *p_admin_password);
```

After (clear naming - hf_common.h lines 33-34):
```c
int es_get_username_password(char *p_admin_name, char *p_admin_password);
int es_set_username_password(const char *p_admin_name, const char *p_admin_password);
```

**Web Server Integration** (web-server.c lines 119-130):

```c
// Save user-configured username/password - 0=success, 1=fail
int save_sys_username_password(const char *username, const char *password) {
    assert(username!=NULL && password!=NULL);
    return es_set_username_password(username, password);
}

// Query user-configured username/password - 0=success, 1=fail
void get_sys_username_password(char *username, char *password) {
    es_get_username_password(username, password);
    return;
}
```

**EEPROM Backend** (hf_common.c lines 549-565):

```c
int es_get_username_password(char *p_admin_name, char *p_admin_password) {
    if (NULL == p_admin_name)
        return -1;
    if (NULL == p_admin_password)
        return -1;

    hf_i2c_mem_read(&hi2c1, AT24C_ADDR, EEPROM_USERNAME_PASSWORD_ADDR,
                    (uint8_t *)p_admin_name, EEPROM_USERNAME_PASSWORD_BUFFER_SIZE);

    // Password stored immediately after username in EEPROM
    // Username at offset 0, password at offset 32 (assuming 32-byte max each)
    strcpy(p_admin_password, p_admin_name + 32);
    return 0;
}

int es_set_username_password(const char *p_admin_name, const char *p_admin_password) {
    if (NULL == p_admin_name)
        return -1;
    if (NULL == p_admin_password)
        return -1;

    uint8_t buffer[EEPROM_USERNAME_PASSWORD_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    strncpy((char*)buffer, p_admin_name, 32);
    strncpy((char*)(buffer + 32), p_admin_password, 32);

    hf_i2c_mem_write(&hi2c1, AT24C_ADDR, EEPROM_USERNAME_PASSWORD_ADDR,
                     buffer, EEPROM_USERNAME_PASSWORD_BUFFER_SIZE);
    return 0;
}
```

**Security Notes**:
- Passwords stored in **plaintext** in EEPROM (critical vulnerability)
- No hashing, salting, or encryption applied
- Physical access to EEPROM = full credential compromise
- **EEPROM_USERNAME_PASSWORD_ADDR**: `0x0100` (defined in web-server.c)
- **Buffer Size**: 64 bytes total (32 username + 32 password)

### 2. Network Configuration Persistence

**Previous Implementation** (web-server.c - placeholder):
```c
NETInfo get_net_info() {
    printf("TODO call get_net_info\n");
    NETInfo example = {
        "192.168.1.1",
        "01:23:45:67:89:AB",
        "255.255.255.0",
        "192.168.1.254"
    };
    return example;
}
```

**New Implementation** (lines 137-174):

```c
NETInfo get_net_info() {
    NETInfo example;
    ip4_addr_t ip4_addr;
    uint8_t buf[4];
    char *p_addr;
    uint8_t mac[6];

    /* Get IP address */
    es_get_mcu_ipaddr(buf);
    IP4_ADDR(&ip4_addr, buf[0], buf[1], buf[2], buf[3]);
    p_addr = ip4addr_ntoa(&ip4_addr);
    strcpy(example.ipaddr, p_addr);

    /* Get netmask */
    es_get_mcu_netmask(buf);
    IP4_ADDR(&ip4_addr, buf[0], buf[1], buf[2], buf[3]);
    p_addr = ip4addr_ntoa(&ip4_addr);
    strcpy(example.subnetwork, p_addr);

    /* Get gateway */
    es_get_mcu_gateway(buf);
    IP4_ADDR(&ip4_addr, buf[0], buf[1], buf[2], buf[3]);
    p_addr = ip4addr_ntoa(&ip4_addr);
    strcpy(example.gateway, p_addr);

    /* Get MAC address */
    es_get_mcu_mac(mac);
    memset(example.macaddr, 0, sizeof(example.macaddr));
    snprintf(example.macaddr, sizeof(example.macaddr),
             "%02x.%02x.%02x.%02x.%02x.%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return example;
}
```

**Set Network Configuration** (lines 178-204):

```c
int set_net_info(NETInfo netinfo) {
    u32_t naddr;
    u32_t haddr;
    uint8_t mac[6];

    /* Set IP address */
    naddr = ipaddr_addr(netinfo.ipaddr);
    haddr = PP_NTOHL(naddr);  // Network to host byte order
    es_set_mcu_ipaddr((uint8_t *)&haddr);

    /* Set netmask */
    naddr = ipaddr_addr(netinfo.subnetwork);
    haddr = PP_NTOHL(naddr);
    es_set_mcu_netmask((uint8_t *)&haddr);

    /* Set gateway */
    naddr = ipaddr_addr(netinfo.gateway);
    haddr = PP_NTOHL(naddr);
    es_set_mcu_gateway((uint8_t *)&haddr);

    /* Set MAC address */
    hexstr2mac(mac, netinfo.macaddr);
    es_set_mcu_mac(mac);

    return 0;
}
```

**Endianness Handling**:
- lwIP's `ipaddr_addr()` returns network byte order (big-endian)
- `PP_NTOHL()` converts network to host (little-endian on STM32)
- EEPROM storage uses host byte order for consistency

### 3. MAC Address Parsing Utility

**New Function** (hf_common.c lines 48-72):

```c
static int hex2num(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

void hexstr2mac(uint8_t *mac, char *hexstr) {
    int i = 0;

    while (i < 6) {
        if (' ' == *hexstr || ':' == *hexstr || '"' == *hexstr || '\'' == *hexstr) {
            hexstr++;  // Skip separator characters
            continue;
        }
        *(mac + i) = (hex2num(*hexstr) << 4) | hex2num(*(hexstr + 1));
        i++;
        hexstr += 2;
    }
}
```

**Supported Formats**:
- `"01:23:45:67:89:AB"` (colon-separated)
- `"01 23 45 67 89 AB"` (space-separated)
- `"'01:23:45:67:89:AB'"` (quoted)
- `"012345678AB"` (continuous hex string)

**Robustness**:
- Skips over separators automatically
- Handles mixed case (A-F or a-f)
- Processes exactly 6 bytes (MAC address length)
- No buffer overflow checking (assumes valid input)

### 4. DIP Switch Configuration

**Previous Stub**:
```c
DIPSwitchInfo get_dip_switch() {
    printf("TODO call get_dip_switch\n");
    DIPSwitchInfo dipSwitchInfo;
    dipSwitchInfo.dip01 = 0;
    dipSwitchInfo.dip02 = 0;
    dipSwitchInfo.dip03 = 1;
    dipSwitchInfo.dip04 = 1;
    return dipSwitchInfo;
}
```

**Real Implementation** (lines 211-228):

```c
DIPSwitchInfo get_dip_switch() {
    uint8_t som_dip_switch_state;
    DIPSwitchInfo dipSwitchInfo;

    es_get_som_dip_switch_soft_state(&som_dip_switch_state);

    // Extract individual bit states
    dipSwitchInfo.dip01 = 0x1 & som_dip_switch_state;
    dipSwitchInfo.dip02 = (0x2 & som_dip_switch_state) >> 1;
    dipSwitchInfo.dip03 = (0x4 & som_dip_switch_state) >> 2;
    dipSwitchInfo.dip04 = (0x8 & som_dip_switch_state) >> 3;

    return dipSwitchInfo;
}

int set_dip_switch(DIPSwitchInfo dipSwitchInfo) {
    uint8_t som_dip_switch_state;

    // Pack individual bits into byte
    som_dip_switch_state = ((0x1 & dipSwitchInfo.dip04) << 3) |
                           ((0x1 & dipSwitchInfo.dip03) << 2) |
                           ((0x1 & dipSwitchInfo.dip02) << 1) |
                           (0x1 & dipSwitchInfo.dip01);

    return es_set_som_dip_switch_soft_state(som_dip_switch_state);
}
```

**Bit Packing**:
- Byte layout: `[unused:4][DIP4:1][DIP3:1][DIP2:1][DIP1:1]`
- Bit 0 = DIP switch 1 (LSB)
- Bit 3 = DIP switch 4
- Bits 4-7 unused (reserved for expansion to 8-position switch)

**Software vs Hardware Control**:
- "Soft state" allows web interface to override physical DIP switch
- Actual hardware switch position read separately
- Software setting takes precedence when enabled
- Enables remote configuration without physical access

### 5. Carrier Board Information Retrieval

**Previous Stub**:
```c
CBSimpleInfo get_cb_info() {
    printf("TODO call get_cb_info\n");
    CBSimpleInfo example = {
        0,  // magicNumber
        1,  // formatVersionNumber
        2,  // productIdentifier
        3,  // pcbRevision
        "v1.10.1"  // boardSerialNumber
    };
    return example;
}
```

**Real Implementation** (lines 244-265):

```c
CBSimpleInfo get_cb_info() {
    CarrierBoardInfo carrierBoardInfo;
    CBSimpleInfo example;

    es_get_carrier_borad_info(&carrierBoardInfo);

    example.magicNumber = carrierBoardInfo.magicNumber;
    example.formatVersionNumber = carrierBoardInfo.formatVersionNumber;
    example.productIdentifier = carrierBoardInfo.productIdentifier;
    example.pcbRevision = carrierBoardInfo.pcbRevision;

    memcpy(example.boardSerialNumber, carrierBoardInfo.boardSerialNumber,
           sizeof(example.boardSerialNumber));

    return example;
}
```

**Data Source**: EEPROM region dedicated to carrier board identification
- **Magic Number**: Validates EEPROM data integrity (e.g., 0xCAFEBABE)
- **Format Version**: Enables backward compatibility for structure changes
- **Product ID**: Identifies specific board model
- **PCB Revision**: Hardware version (A, B, C, etc.)
- **Serial Number**: Unique board identifier for tracking and support

### 6. Printf Redirection Bug Fix

**Issue**: `_write()` function was accidentally disabled.

**Before** (hf_common.c):
```c
#if 0  // Function disabled!
int _write(int fd, char *ch, int len) {
    // ...printf redirection to UART...
}
#endif
```

**After** (lines 25, 32 - patch 0024 preview):
```c
// #if 0 removed, function now active
int _write(int fd, char *ch, int len) {
    uint8_t val = '\r';
    int length = len;

    for (int i = 0; i < len; i++) {
        if (*ch == '\n') {
            HAL_UART_Transmit(&huart3, &val, 1, 1000);
        }
        HAL_UART_Transmit(&huart3, (uint8_t*)ch++, 1, 1000);
    }
    return length;
}
```

**Impact**:
- `printf()` relies on `_write()` for output redirection
- Without this function, printf would have no effect
- Fix ensures debug messages appear on UART3 console
- Critical for remote debugging and diagnostics

## Security Analysis

### Vulnerabilities Introduced

1. **Plaintext Password Storage** (SEVERITY: CRITICAL):
```c
// Passwords stored without encryption
strncpy((char*)(buffer + 32), p_admin_password, 32);
hf_i2c_mem_write(&hi2c1, AT24C_ADDR, EEPROM_USERNAME_PASSWORD_ADDR,
                 buffer, EEPROM_USERNAME_PASSWORD_BUFFER_SIZE);
```
**Attack**: Physical EEPROM dump reveals credentials
**Mitigation**: Should use bcrypt, scrypt, or at minimum SHA-256 hashing

2. **No Input Validation in MAC Parsing**:
```c
void hexstr2mac(uint8_t *mac, char *hexstr) {
    // No length checking on hexstr input
    // No validation of hex characters
    // Buffer overflow possible if hexstr malformed
}
```
**Attack**: Malformed MAC string could cause buffer overrun
**Mitigation**: Add bounds checking and validation

3. **No Encryption for Network Config**:
- Network settings stored in plaintext EEPROM
- Includes IP addresses, MAC address, gateway
- Information disclosure via physical access

### Positive Security Aspects

1. **Input Sanitization Started**:
- MAC parsing skips quotes and separators
- Prevents some injection attacks

2. **Const Correctness**:
```c
int es_set_username_password(const char *p_admin_name, const char *p_admin_password);
```
- Prevents accidental modification of input strings
- Compiler can optimize better

3. **NULL Pointer Checks**:
```c
if (NULL == p_admin_name)
    return -1;
if (NULL == p_admin_password)
    return -1;
```
- Prevents crashes from NULL dereference
- Returns error code for caller to handle

## Testing Recommendations

### Functional Tests

1. **Username/Password Persistence**:
```bash
# Set via web interface
curl -X POST http://<bmc-ip>/modify_account \
  -d "username=newuser&password=newpass"

# Power cycle BMC

# Verify persistence
curl -X POST http://<bmc-ip>/login \
  -d "username=newuser&password=newpass"
# Should succeed
```

2. **Network Configuration**:
```bash
# Set static IP
curl -X POST http://<bmc-ip>/network \
  -d "ipaddr=192.168.10.50&subnetwork=255.255.255.0&gateway=192.168.10.1&macaddr=AA:BB:CC:DD:EE:FF"

# Reboot BMC

# Verify new IP active
ping 192.168.10.50
```

3. **DIP Switch Virtual Control**:
```bash
# Set all switches ON via web
curl -X POST http://<bmc-ip>/dip_switch \
  -d "dip01=1&dip02=1&dip03=1&dip04=1"

# Verify stored
curl http://<bmc-ip>/dip_switch
# Expected: {"data":{"dip01":"1","dip02":"1",...}}
```

4. **MAC Address Parsing**:
Test various formats:
- `"01:23:45:67:89:AB"`
- `"01-23-45-67-89-AB"`
- `"'01:23:45:67:89:AB'"`
- Malformed: `"XX:XX:XX:XX:XX:XX"` (should fail gracefully)

### EEPROM Integrity Tests

1. **Write Cycling**:
```c
// Stress test EEPROM wear leveling
for (int i = 0; i < 1000; i++) {
    set_net_info(config);
    NETInfo readback = get_net_info();
    assert(memcmp(&config, &readback, sizeof(config)) == 0);
}
```

2. **Power Loss During Write**:
- Initiate EEPROM write
- Cut power mid-write
- Verify data integrity on next boot
- Should detect corruption via checksums

### Security Tests

1. **Password Extraction**:
```bash
# Connect I2C EEPROM reader to AT24C chip
# Read address 0x0100 (EEPROM_USERNAME_PASSWORD_ADDR)
# Verify password is visible in plaintext
```

2. **Buffer Overflow in MAC Parsing**:
```bash
# Send oversized MAC address
curl -X POST http://<bmc-ip>/network \
  -d "macaddr=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA..."
# Monitor for crash or memory corruption
```

## Integration Notes

### EEPROM Memory Map

Based on this patch and referenced code:

| Address | Size | Content |
|---------|------|---------|
| 0x0000  | ?    | SOM Board Info |
| 0x0100  | 64   | Username (32) + Password (32) |
| 0x0200  | ?    | Network Configuration |
| 0x0300  | ?    | Carrier Board Info |
| ...     | ...  | ... |

**Note**: Exact memory map would be in `hf_common.h` EEPROM address defines

### Backward Compatibility

**Breaking Changes**: None
- Functions renamed but old names likely never called in production
- Data formats in EEPROM unchanged
- Web API endpoints unchanged

**Migration**: Not required (new functionality)

## Performance Impact

**Negligible**:
- EEPROM I2C read: ~1ms per transaction
- EEPROM write: ~5ms (includes write cycle polling)
- Total impact: <10ms added latency to web requests
- Acceptable for human-interface operations

## Related Patches

**Depends On**:
- Patch 0002: Protocol library (defines EEPROM structures)
- Patch 0013: Board info structures
- Patch 0017: Web authentication framework

**Enables**:
- Patch 0024: Printf redirection fix (related code)
- Future: Secure credential storage (should be added)

## Future Work

Based on this implementation:

1. **Hash Passwords**: Replace plaintext with bcrypt/scrypt
2. **Validate MAC Input**: Add bounds checking to `hexstr2mac()`
3. **Encrypt Sensitive Data**: Network config shouldn't be plaintext
4. **Wear Leveling**: Implement if EEPROM writes become frequent
5. **Checksums**: Add CRC to detect corruption (partially done in later patches)

## Conclusion

This patch represents a critical milestone: transitioning from a prototype web interface with fake data to a production-ready system with persistent configuration storage. However, the plaintext password storage introduces a critical security vulnerability that should be addressed before production deployment.
