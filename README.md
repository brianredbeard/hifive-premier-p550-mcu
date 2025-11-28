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

✅ **Ready for use** - All 109 patches have been successfully integrated and the firmware builds cleanly.

Current version: **BMC v2.8** (based on patch series through 0109)

## Quick Start

### Prerequisites

- **STM32CubeMX 6.10.0** - For regenerating initialization code (if needed)
- **ARM GCC Toolchain** - Cross-compiler for ARM Cortex-M4
  - Tested with: ARM GNU Toolchain 14.3.Rel1 or later
  - Download from: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
- **Make** - Build automation
- **Git** - Version control

### Build Instructions

1. **Clone the repository:**
   ```bash
   git clone https://github.com/eswincomputing/hifive-premier-p550-mcu-patches.git
   cd hifive-premier-p550-mcu-patches
   ```

2. **Set up ARM GCC toolchain:**

   **Option A** - Toolchain in PATH (recommended):
   ```bash
   # Ensure arm-none-eabi-gcc is in your PATH
   which arm-none-eabi-gcc
   ```

   **Option B** - Specify toolchain path:
   ```bash
   # Edit Makefile and set GCC_PATH variable
   # Example: GCC_PATH = /opt/gcc-arm-none-eabi-14.3/bin
   ```

3. **Build the firmware:**
   ```bash
   make
   ```

4. **Build artifacts** will be generated in `build/`:
   - `STM32F407VET6_BMC.elf` - ELF format for debugging (404 KB)
   - `STM32F407VET6_BMC.bin` - Raw binary for flashing (301 KB)
   - `STM32F407VET6_BMC.hex` - Intel HEX format (846 KB)

5. **Clean build artifacts:**
   ```bash
   make clean
   ```

### Flashing the Firmware

Refer to the [HiFive Premier P550 MCU User Manual](https://www.sifive.com/document-file/premier-p550-mcu-user-manual) for detailed instructions on:
- Connecting to the STM32 debug interface (SWD)
- Flashing firmware using OpenOCD or STM32CubeProgrammer
- Initial configuration and setup

## Development Workflow

### Regenerating from STM32CubeMX (Optional)

If you need to modify peripheral configurations:

1. **Install STM32CubeMX 6.10.0** from https://www.st.com/stm32cubemx

   ⚠️ **Important**: Use version 6.10.0 specifically - patches were generated against this version.

2. **Open the project:**
   ```bash
   # Launch STM32CubeMX and open:
   STM32F407VET6_BMC.ioc
   ```

3. **Make configuration changes** in CubeMX GUI

4. **Generate code** from CubeMX

5. **Restore the Makefile:**
   ```bash
   git checkout Makefile
   ```

   Note: The Makefile must be restored because CubeMX may reorder source files, which would conflict with the patch structure.

6. **Rebuild:**
   ```bash
   make clean
   make
   ```

### Working with Patches

This repository includes 109 patches in `patches-2025-11-05/` that document the complete development history from baseline CubeMX-generated code to the final BMC firmware.

**Note**: The patches are already applied in this repository. You only need to apply them if starting from scratch with a fresh CubeMX-generated baseline.

**To apply patches from scratch:**

1. Generate baseline code from `STM32F407VET6_BMC.ioc` using CubeMX 6.10.0
2. Replace generated Makefile with the one from this repository
3. Apply patches:
   ```bash
   git am --ignore-space-change --ignore-whitespace patches-2025-11-05/*.patch
   ```

For detailed patch application history and validation, see [CONVERGENCE.md](CONVERGENCE.md).

## Repository Structure

```
hifive-premier-p550-mcu-patches/
├── Core/                          # Application source code
│   ├── Inc/                       # Header files
│   │   ├── hf_common.h           # Core system definitions
│   │   ├── hf_i2c.h              # I2C subsystem
│   │   ├── hf_power_process.h    # Power management
│   │   └── ...
│   └── Src/                       # Source files
│       ├── main.c                # Main application
│       ├── hf_common.c           # Core system implementation
│       ├── hf_board_init.c       # Board initialization
│       ├── hf_power_process.c    # Power management
│       ├── hf_i2c.c              # I2C communication
│       ├── hf_protocol_process.c # Protocol handling
│       ├── web-server.c          # HTTP server
│       ├── console.c             # CLI console
│       ├── telnet_server.c       # Telnet server
│       ├── protocol_lib/         # Communication protocol library
│       └── ...
├── Drivers/                       # STM32 drivers
│   ├── CMSIS/                    # ARM CMSIS (Apache-2.0)
│   ├── STM32F4xx_HAL_Driver/     # STM32 HAL (BSD-3-Clause)
│   └── BSP/                      # Board support (LAN8742 Ethernet PHY)
├── Middlewares/                   # Third-party middleware
│   └── Third_Party/
│       ├── FreeRTOS/             # FreeRTOS RTOS (MIT)
│       └── LwIP/                 # LwIP TCP/IP stack (BSD-3-Clause)
├── LWIP/                          # LwIP configuration
│   ├── App/                      # LwIP application layer
│   └── Target/                   # STM32-specific port
├── docs/                          # Documentation
│   └── patches/                  # Individual patch documentation
├── patches-2025-11-05/            # 109 patch files
├── scripts/                       # Helper scripts
├── Makefile                       # Build system
├── STM32F407VET6_BMC.ioc         # STM32CubeMX configuration
├── STM32F407VETx_FLASH.ld        # Linker script
├── openocd.cfg                   # OpenOCD debugger configuration
├── CONTRIBUTORS.md               # License and attribution
├── project.spdx                  # SPDX license manifest
├── CONVERGENCE.md                # Patch integration history
└── README.md                     # This file
```

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
- **[CONVERGENCE.md](CONVERGENCE.md)** - Patch integration history and validation
- **[docs/patches/](docs/patches/)** - Individual patch documentation (60+ files)
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

**Last Updated**: 2025-11-26
**Firmware Version**: BMC v2.8
**Build Status**: ✅ Passing
