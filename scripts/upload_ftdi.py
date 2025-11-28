#!/usr/bin/env python3
"""
Custom upload script for FT4232H debugging via OpenOCD.

This script is called by PlatformIO when upload_protocol = custom
and provides proper path resolution for OpenOCD and configuration files.

On macOS, this script uses sudo to access the FTDI device, as macOS's
DriverKit prevents normal user access to MPSSE mode.
"""

Import("env", "projenv")
import os
import sys
from os.path import join

# Get PlatformIO paths
platform = env.PioPlatform()
openocd_bin = join(platform.get_package_dir("tool-openocd") or "", "bin", "openocd")
openocd_scripts = join(platform.get_package_dir("tool-openocd") or "", "openocd", "scripts")

# Get project paths
project_dir = env["PROJECT_DIR"]
interface_cfg = join(project_dir, "boards", "ft4232h-mcu-jtag.cfg")

# Get firmware path - use ELF file which contains address information
firmware_path = str(env.get("BUILD_DIR") + "/firmware.elf")

def upload_ftdi(source, target, env):
    """Upload firmware using OpenOCD with FT4232H Interface 1."""

    # Build OpenOCD command
    # Use ELF file for programming - it contains flash address information
    cmd = [
        openocd_bin,
        "-s", openocd_scripts,
        "-f", interface_cfg,
        "-c", f"program {{{firmware_path}}} verify reset; shutdown"
    ]

    # On macOS, use sudo to access FTDI device
    if sys.platform == "darwin":
        cmd.insert(0, "sudo")
        print("Note: Using sudo for FTDI access on macOS (you may be prompted for password)")

    # Print command for debugging
    print("Upload command:")
    print(" ".join(cmd))

    # Execute OpenOCD
    return env.Execute(" ".join([f'"{arg}"' if " " in arg else arg for arg in cmd]))

# Replace upload action
env.Replace(UPLOADCMD=upload_ftdi)
