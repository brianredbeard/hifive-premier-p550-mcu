# Patch Convergence Strategy

**Status:** In Progress - Infrastructure Setup Complete
**Current Position:** Patch 0001 applied, checkpoint created
**Target:** Patch 0109 with validated convergence

---

## Overview

This document tracks the application of 109 patches from `patches-2025-11-05/` to the STM32F407VET6 BMC firmware codebase. The patches originate from a different baseline, making them inherently non-reversible via `git apply -R`. This strategy ensures convergence to the correct final state through multi-layered validation.

## Core Challenge

**Baseline Mismatch:** Patches were generated from a repository state different from ours (c7b75f4). This manifests as:
- Rejection files even from clean application
- Context mismatches (same line numbers, different content)
- Non-reversibility (`git apply -R` fails)

**Solution:** Accept forward-only operations with demonstrable convergence through:
1. Architectural fingerprinting
2. Functional validation at checkpoints
3. Checkpoint-based quasi-bisection
4. Dual-branch history management

---

## Checkpoint Strategy

### Major Checkpoints

| Patch | Status | Theme | Key Features |
|-------|--------|-------|--------------|
| 0001 | ✅ Complete | Baseline | Power mgmt, protocol lib, GPIO control |
| 0010 | ⏳ Pending | Foundation | Complete basic debugging |
| 0030 | ⏳ Pending | INA226 Integration | Web server + power monitoring |
| 0060 | ⏳ Pending | I2C Reliability | Error recovery, EEPROM handling |
| 0090 | ⏳ Pending | Robustness | Factory reset, CRC32, boot modes |
| 0109 | ⏳ Pending | Production Ready | Telnet, URL decode, input validation |

### Checkpoint Contents

Each checkpoint preserves:
- **Branch:** `checkpoint-XXXX` (frozen git branch)
- **Tag:** `checkpoint-XXXX-tag` (easy reference)
- **Metrics:** Function signatures, struct layouts, LOC, file count
- **Binary:** Built firmware (if build succeeds)
- **Cumulative Patch:** Full diff from baseline to this point
- **README:** Validation status, changes summary

**Location:** `checkpoints/checkpoint-XXXX/`

---

## Current State

### Patch 0001: ✅ Applied

**Commit:** bb38275
**Author:** xuxiang <xuxiang@eswincomputing.com>
**Date:** Fri Apr 19 10:38:21 2024 +0800

**Changes:**
- 41 files changed (+7,434, -1,073)
- 71 rejected hunks manually resolved
- New files: protocol library, power management, board init, GPIO/I2C/SPI handlers
- Core modifications: FreeRTOS config, HAL configuration, Makefile, main.c

**Key Findings:**
- Patches expect different copyright years (2024 vs 2025 in our repo)
- Whitespace sensitivity critical - NO `--whitespace=fix` allowed
- Manual rejection resolution required and expected
- Build path issues (GCC toolchain hardcoded to `/home/xxbringup/...`)

**Checkpoint:** `checkpoint-0001` created at `checkpoints/checkpoint-0001/`

---

## Validation Framework

### Three-Layer Validation

#### Layer 1: Architectural Fingerprints
**Purpose:** Track structural invariants

```bash
metrics/function_signatures.txt   # API inventory
metrics/struct_layouts.txt         # Data structures
metrics/module_dependencies.txt    # Include graph
metrics/file_count.txt            # File inventory
metrics/loc.txt                   # Lines of code
```

#### Layer 2: Semantic Validation
**Purpose:** Verify behavior at checkpoints

```yaml
patch_0010:
  - build_succeeds
  - power_sequence_transitions
  - ethernet_link_detection

patch_0030:
  - i2c_communication_functional
  - ina226_power_reading
  - web_interface_renders

# ... (see validation/checkpoints.yaml for complete definitions)
```

#### Layer 3: Binary Artifacts
**Purpose:** Concrete verification

```bash
build/STM32F407VET6_BMC.elf    # Firmware binary
metrics/binary_symbols.txt      # Symbol table
metrics/binary_size.txt         # Memory usage
metrics/memory_map.txt          # Section layout
```

### Convergence Indicators

**✅ Good Signs (Converging):**
- Rejection count decreasing over patch sequence
- Function signatures stabilizing
- Build warnings decreasing
- Binary size tracking expected growth

**⚠️ Warning Signs (Potential Divergence):**
- Rejection count increasing
- New undefined symbols appearing
- Binary size deviating >20% from expectations

**🛑 Critical Abort Signals:**
- Same hunks rejecting repeatedly (>3 patches)
- Core data structures mismatched
- Build failures unresolvable
- Previously passing tests now failing

---

## Quasi-Reversibility: Bisection Without Revert

### The Problem

Traditional git bisect requires:
```bash
git bisect good
git bisect bad
git revert <commit>  # ← IMPOSSIBLE with our patches
```

### The Solution

**Checkpoint-Based Binary Search:**

```
Good: checkpoint-0001
Bad:  checkpoint-0109

Test: checkpoint-0060
  → Bug exists
  → Now: Good=0001, Bad=0060

Test: checkpoint-0030
  → Bug does NOT exist
  → Now: Good=0030, Bad=0060

Test: checkpoint-0045 (created on-demand)
  → Bug exists
  → Now: Good=0030, Bad=0045

Result: Bug introduced between patches 0030-0045 (15 patches)
```

**Refinement:** Create intermediate checkpoints every 5 patches to narrow further.

**Reconstruction Instead of Revert:**
```bash
# To "revert" to patch 0030:
git checkout c7b75f4  # baseline
git apply checkpoints/checkpoint-0030/cumulative.patch

# Now at state equivalent to patch 0030
```

---

## Patch Application Workflow

### Per-Patch Procedure

1. **Pre-check:** Ensure clean working directory
2. **Apply:** `git apply --reject <patch>`  (NO `--whitespace=fix`!)
3. **Analyze:** Count and categorize rejections
4. **Resolve:** Apply resolution policy based on rejection type
5. **Stage:** `git add -A` then `git reset HEAD build/ logs/`
6. **Commit:** Preserve original author/date metadata
7. **Validate:** Run build (if toolchain available)
8. **Metrics:** Collect architectural fingerprints
9. **Divergence Check:** Compare against baseline

### Rejection Resolution Policy

| Type | Action | Rationale |
|------|--------|-----------|
| Copyright year | Auto-apply | Cosmetic, safe |
| Whitespace | Careful manual | Must preserve exact spacing |
| Context mismatch | Manual review | May indicate drift |
| Code change | Deep analysis | Critical for correctness |

### Abort vs Continue

**Abort If:**
- Same rejection in >3 consecutive patches
- Rejection in core (protocol, power management)
- Build broken >30 minutes
- Previous test now failing

**Continue If:**
- Copyright/whitespace only
- Peripheral code affected
- Build still succeeds
- All tests still pass

---

## Known Issues

### Build System

**Problem:** Makefile has hardcoded GCC path:
```makefile
GCC_PATH = /home/xxbringup/dvb/gcc-arm-none-eabi-10.3-2021.10/bin
```

**Impact:**
- Builds fail on systems without this exact path
- Binary artifacts cannot be generated
- Layer 3 validation (binary) incomplete

**Workaround:**
- Update `GCC_PATH` in Makefile if ARM GCC available locally
- OR skip binary validation for now
- OR use CI/CD with correct toolchain

**Status:** Accepted limitation for local development

### Copyright Years

**Pattern:** Repository has `Copyright (c) 2025` but patches expect `2024`

**Reason:** CubeMX regenerated files with current year

**Resolution:** Applied sed batch fix for copyright years during patch 0001

**Going Forward:** May encounter more copyright conflicts - apply same pattern

---

## Next Steps

### Immediate (Phase 2)

1. ✅ Create infrastructure scripts
2. ✅ Create checkpoint-0001
3. ⏳ Apply patches 0002-0010
4. ⏳ Create checkpoint-0010
5. ⏳ Validate Foundation complete

### Short Term (Phases 3-5)

- Apply patches through checkpoint 0060
- Validate I2C reliability milestone
- Refine rejection resolution automation

### Long Term (Phases 6-7)

- Complete patch series to 0109
- Comprehensive final validation
- Create clean-history branch for upstream

---

## References

**Plan:** `/Users/bharrington/.claude/plans/adaptive-beaming-eagle.md`
**Patches:** `patches-2025-11-05/`
**Documentation:** `docs/patches/README.md`
**Scripts:** `scripts/`
**Checkpoints:** `checkpoints/`

---

**Last Updated:** 2025-11-25
**Current Patch:** 0001
**Next Milestone:** 0010
