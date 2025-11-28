# HiFive Premier P550 BMC Firmware

Baseboard Management Controller (BMC) firmware for the HiFive Premier P550 (HF106) development platform, running on STM32F407VET6 microcontroller.

## Platform Overview

The **HiFive Premier P550 (HF106)** is a development platform based on ESWIN's EIC7700 Edge Computing SoC integrated with SiFive's P550 RISC-V cores. It consists of:

- **HF106S** - System on Module (SOM) with EIC7700 SoC
- **HF106C** - Carrier board with STM32F407VET6 BMC

The BMC provides power management, monitoring, remote console access, and system control for the platform.

## Features

- **Power Management**: SOM power sequencing, power button handling, ATX power control
- **Monitoring**: I2C-based power monitoring (INA226, PAC1934), temperature sensors
- **Remote Access**:
  - Web-based management interface (HTTP server)
  - Telnet console access (SOM and MCU consoles)
  - FreeRTOS CLI over UART3
- **Communication**:
  - SPI slave interface for SOM communication
  - UART-based protocol for SOM interaction
- **Storage**: EEPROM management for board configuration and carrier board info
- **Real-Time OS**: FreeRTOS with LwIP TCP/IP stack
- **Diagnostics**: System monitoring, RTC, watchdog, LED control

## Project Status

✅ **Ready for use** - PlatformIO-based build system with all patches integrated and firmware building cleanly.

Current version: **BMC v2.8.2**

## Quick Start

### Prerequisites

- **PlatformIO** - Modern embedded development platform
  - Install via: https://platformio.org/install
  - Or use PlatformIO IDE (VS Code extension)
- **Git** - Version control

PlatformIO automatically handles:
- ARM GCC toolchain installation
- STM32 HAL/CMSIS drivers
- FreeRTOS and LwIP middleware dependencies

### Build Instructions

1. **Clone the repository:**
   ```bash
   git clone https://github.com/eswincomputing/hifive-premier-p550-mcu-patches.git
   cd hifive-premier-p550-mcu-patches
   ```

2. **Build the firmware:**
   ```bash
   pio run -e debug-ftdi
   ```

   Available environments:
   - `debug-ftdi` - Onboard FT4232H JTAG debugging (recommended)
   - `debug-stlink` - External ST-Link/V2 debugger
   - `debug-jlink` - J-Link via JTAG header

3. **Build artifacts** will be generated in `.pio/build/debug-ftdi/`:
   - `firmware.elf` - ELF format for debugging
   - `firmware.bin` - Raw binary for flashing (273 KB)
   - `firmware.hex` - Intel HEX format

4. **Flash the firmware:**
   ```bash
   pio run -e debug-ftdi -t upload
   ```

5. **Clean build artifacts:**
   ```bash
   pio run -t clean
   ```

### Serial Console Access

The BMC provides console access via UART3 (115200 baud):

```bash
pio device monitor -e debug-ftdi
```

Or use your preferred serial terminal (screen, minicom, etc.).

## Development Workflow

### Debugging

PlatformIO provides integrated debugging support:

```bash
# Start GDB debug session
pio debug -e debug-ftdi
```

Debug configurations are pre-configured for:
- **FT4232H** - Onboard JTAG (Interface 1)
- **ST-Link/V2** - External debugger
- **J-Link** - Via JTAG header (J18)

### Testing

Run static analysis:
```bash
pio check -e debug-ftdi
```

### Advanced: STM32CubeMX Integration (Optional)

The project includes `STM32F407VET6_BMC.ioc` for hardware configuration changes.

⚠️ **Important**: This is a PlatformIO project. CubeMX is only needed for regenerating pin configurations or peripheral settings. The build system is PlatformIO, not CubeMX-generated Makefiles.

If modifying hardware configuration:
1. Open `STM32F407VET6_BMC.ioc` in STM32CubeMX 6.10.0
2. Make configuration changes
3. Generate code (PlatformIO structure is preserved)
4. Rebuild with `pio run`

### Historical Context: Patch Series

This repository evolved from a patch-based workflow (109 patches in `patches-2025-11-05/`). These document the complete development history from CubeMX baseline to production firmware.

**Note**: Patches are already applied. The current codebase is the final integrated state ready for PlatformIO development.

## Project Structure (Updated 2025-11-28)

This project follows **PlatformIO standard layout** with a clean root directory:

```
hifive-premier-p550-mcu-patches/
├── src/                           # Application source files (28 .c files)
│   ├── main.c                    # Main application entry point
│   ├── hf_common.c               # Core system implementation
│   ├── hf_power_process.c        # Power management state machine
│   ├── hf_i2c.c                  # I2C HAL (INA226, PAC1934, EEPROM)
│   ├── console.c                 # FreeRTOS CLI implementation
│   ├── web-server.c              # HTTP server
│   └── ...                       # Telnet servers, protocols, etc.
├── include/                       # Application headers (20 .h files)
│   ├── protocol_lib/             # Communication protocol library
│   ├── hf_common.h               # Core system definitions
│   ├── main.h                    # GPIO pin definitions
│   ├── FreeRTOSConfig.h          # RTOS configuration
│   ├── lwipopts.h                # LwIP TCP/IP configuration
│   └── ...
├── boards/                        # Board configurations
│   └── ft4232h-mcu-jtag.cfg      # OpenOCD config for onboard JTAG
├── scripts/                       # Build automation
│   ├── upload_ftdi.py            # FT4232H upload script
│   └── renode_build.py           # Renode simulation builder
├── docs/                          # Documentation
│   ├── restructure-notes.md      # Migration notes
│   └── debugging/                # Debug setup guides
├── platformio.ini                 # PlatformIO build configuration
├── CONTRIBUTORS.md                # Licensing and attribution
├── project.spdx                   # SPDX license manifest
└── README.md                      # This file
```

**What's NOT in this repo** (automatically provided by PlatformIO):
- ❌ No `Drivers/` - STM32 HAL/CMSIS provided by framework
- ❌ No `Middlewares/` - FreeRTOS/LwIP provided by lib_deps
- ❌ No `Makefile` - PlatformIO handles build system
- ❌ No startup files or linker scripts - Framework-provided
- ❌ No `.ioc` file needed for building - PlatformIO-native

This results in a clean, minimal repository focused on application code.

## Hardware Requirements

- **Microcontroller**: STM32F407VET6 (ARM Cortex-M4F, 168 MHz)
- **Flash**: 512 KB
- **RAM**: 192 KB (128 KB main + 64 KB CCM)
- **Peripherals Used**:
  - Ethernet MAC (LAN8742 PHY)
  - I2C (I2C1, I2C3)
  - SPI (SPI1, SPI2)
  - UART (UART3, UART4, UART6)
  - RTC (Real-Time Clock)
  - CRC (Hardware CRC32)
  - Timers, GPIO, DMA

## Memory Usage

Current firmware footprint (from build output):

```
   text    data     bss     dec     hex filename
 293024    8312   18040  319376   4df90 build/STM32F407VET6_BMC.elf
```

- **Code (text)**: 293 KB
- **Initialized data**: 8 KB
- **Uninitialized data (BSS)**: 18 KB
- **Total**: 319 KB

## Licensing

This project is licensed under **GPL-2.0-only** with the following third-party components:

- **STM32 CMSIS** - Apache License 2.0 (ARM/STMicroelectronics)
- **STM32 HAL Driver** - BSD-3-Clause (STMicroelectronics)
- **FreeRTOS** - MIT License (Amazon.com, Inc.)
- **LwIP** - BSD-3-Clause (Swedish Institute of Computer Science)

All third-party licenses are GPL-compatible. See [CONTRIBUTORS.md](CONTRIBUTORS.md) for complete licensing information and [project.spdx](project.spdx) for machine-readable SPDX manifest.

### Copyright

Copyright 2024 Beijing ESWIN Computing Technology Co., Ltd.

**Primary Authors:**
- LinMin - Core architecture
- XuXiang - Main framework, SPI, CRC
- Yuan Junhui - Web server, HTTP, networking
- Huang Yifeng - I2C, power monitoring, GPIO

See [CONTRIBUTORS.md](CONTRIBUTORS.md) for complete author attribution.

## Documentation

- **[CONTRIBUTORS.md](CONTRIBUTORS.md)** - Complete licensing and attribution
- **[project.spdx](project.spdx)** - SPDX 2.3 license manifest
- **[docs/restructure-notes.md](docs/restructure-notes.md)** - PlatformIO migration notes
- **[docs/debugging/](docs/debugging/)** - Debug setup guides
- **[HiFive Premier P550 MCU User Manual](https://www.sifive.com/document-file/premier-p550-mcu-user-manual)** - Official hardware documentation

## Support and Community

- **Issues**: Report bugs or request features via GitHub Issues
- **Official Documentation**: https://www.sifive.com/boards/hifive-premier-p550
- **Vendor**: Beijing ESWIN Computing Technology Co., Ltd.

## Contributing

Contributions are welcome! Please ensure:

1. Code follows existing style and conventions
2. New features include appropriate documentation
3. Commits have clear, descriptive messages
4. Changes maintain GPL-2.0-only compatibility
5. Third-party code retains original copyright notices

## Acknowledgments

- **ESWIN Computing** - Original firmware development
- **SiFive** - HiFive Premier P550 platform
- **STMicroelectronics** - STM32 HAL and CubeMX tools
- **ARM** - CMSIS framework
- **Amazon** - FreeRTOS RTOS
- **Swedish Institute of Computer Science** - LwIP TCP/IP stack

---

**Last Updated**: 2025-11-28
**Firmware Version**: BMC v2.8.2
**Build System**: PlatformIO
**Build Status**: ✅ Passing
