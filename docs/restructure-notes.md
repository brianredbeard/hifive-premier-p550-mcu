# PlatformIO Restructure Notes

## Migration Date
2025-11-27

## Changes Made

### Directory Structure
- **src/** - All application source files (.c)
  - Migrated from: Core/Src/, LWIP/App/, LWIP/Target/
  - Includes protocol library files (flattened from protocol_lib/)

- **include/** - All application headers (.h)
  - Migrated from: Core/Inc/ (selective), LWIP/App/, LWIP/Target/
  - Includes protocol_lib/ subdirectory

- **Core/** - Preserved for STM32CubeMX compatibility
  - Core/Src/ now contains symlinks to src/
  - Core/Inc/ retained with system headers

### Git History
All files moved using `git mv` to preserve commit history.
Use `git log --follow <file>` to see full history.

### Build System
- Updated platformio.ini to use src/ as primary source directory
- Core/Src/ excluded from build (contains symlinks)
- Both Core/Inc/ and include/ in include path

## Why This Structure?

1. **PlatformIO Convention**: src/ and include/ are standard in PlatformIO projects
2. **History Preservation**: git mv maintains full commit history
3. **CubeMX Compatibility**: Symlinks in Core/ allow CubeMX regeneration
4. **Clean Separation**: Application code in src/include/, vendor code in Drivers/Middlewares/

## Note: Skipped Task 7 Symlinks

During this restructure, we chose NOT to implement the original plan of creating symlinks in the Core/ directory. Instead, Core/Src is now empty, reflecting the migration of all source files to the src/ directory.