#!/bin/bash
# Create a validated checkpoint for a specific patch number

set -e

PATCH=$1

if [ -z "$PATCH" ]; then
    echo "Usage: $0 <patch_number>"
    echo "Example: $0 10"
    exit 1
fi

PATCH_PADDED=$(printf '%04d' $PATCH)
NAME="checkpoint-${PATCH_PADDED}"

echo "=== Creating Checkpoint at Patch ${PATCH_PADDED} ==="

# Ensure clean state
if ! git diff-index --quiet HEAD --; then
    echo "ERROR: Working directory not clean"
    git status --short
    exit 1
fi

# Create checkpoint branch (frozen state)
echo "Creating branch: ${NAME}"
git branch ${NAME} 2>/dev/null || echo "Branch ${NAME} already exists"

# Tag for easy reference
echo "Creating tag: ${NAME}-tag"
git tag -a ${NAME}-tag -m "Validated checkpoint at patch ${PATCH}" -f

# Create checkpoint directory
CHECKPOINT_DIR="checkpoints/${NAME}"
mkdir -p ${CHECKPOINT_DIR}

# Collect metrics
echo "Collecting metrics..."
mkdir -p ${CHECKPOINT_DIR}/metrics

# Function signatures (if ctags available)
if command -v ctags >/dev/null 2>&1; then
    ctags -x --c-kinds=fp Core/Inc/*.h 2>/dev/null | sort > ${CHECKPOINT_DIR}/metrics/function_signatures.txt || true
fi

# Struct layouts
grep -r "^typedef struct\|^struct.*{" Core/Inc/ 2>/dev/null | sort > ${CHECKPOINT_DIR}/metrics/struct_layouts.txt || true

# File count and LOC
find Core LWIP Middlewares Drivers -name "*.c" -o -name "*.h" 2>/dev/null | wc -l > ${CHECKPOINT_DIR}/metrics/file_count.txt || true
find Core LWIP Middlewares Drivers -name "*.c" -o -name "*.h" 2>/dev/null | xargs wc -l > ${CHECKPOINT_DIR}/metrics/loc.txt || true

# Build if makefile exists
if [ -f "Makefile" ]; then
    echo "Building firmware..."
    if make clean && make; then
        echo "✓ Build successful"
        mkdir -p ${CHECKPOINT_DIR}/build
        cp build/STM32F407VET6_BMC.elf ${CHECKPOINT_DIR}/build/ 2>/dev/null || true
        cp build/STM32F407VET6_BMC.bin ${CHECKPOINT_DIR}/build/ 2>/dev/null || true
        cp build/STM32F407VET6_BMC.hex ${CHECKPOINT_DIR}/build/ 2>/dev/null || true

        # Binary metrics
        if [ -f "build/STM32F407VET6_BMC.elf" ]; then
            arm-none-eabi-nm build/STM32F407VET6_BMC.elf 2>/dev/null | sort > ${CHECKPOINT_DIR}/metrics/binary_symbols.txt || true
            arm-none-eabi-size build/STM32F407VET6_BMC.elf > ${CHECKPOINT_DIR}/metrics/binary_size.txt || true
            arm-none-eabi-objdump -h build/STM32F407VET6_BMC.elf > ${CHECKPOINT_DIR}/metrics/memory_map.txt || true
        fi
    else
        echo "⚠️  Build failed - checkpoint created anyway"
    fi
else
    echo "⚠️  No Makefile found - skipping build"
fi

# Create cumulative patch from master
echo "Creating cumulative patch..."
git diff c7b75f4..HEAD > ${CHECKPOINT_DIR}/cumulative.patch

# Create metadata file
cat > ${CHECKPOINT_DIR}/README.md <<EOF
# Checkpoint ${PATCH_PADDED}

**Created:** $(date)
**Commit:** $(git rev-parse HEAD)
**Branch:** $(git branch --show-current)

## Changes from Baseline

\`\`\`
$(git diff --stat c7b75f4..HEAD)
\`\`\`

## Validation Status

- Build: $([ -f "${CHECKPOINT_DIR}/build/STM32F407VET6_BMC.elf" ] && echo "✓ Success" || echo "✗ Failed or skipped")
- Metrics: $([ -f "${CHECKPOINT_DIR}/metrics/file_count.txt" ] && echo "✓ Collected" || echo "✗ Failed")

## Files Modified

\`\`\`
$(git diff --name-status c7b75f4..HEAD | head -50)
\`\`\`

EOF

echo "✓ Checkpoint ${NAME} created successfully"
echo "  Branch: ${NAME}"
echo "  Tag: ${NAME}-tag"
echo "  Directory: ${CHECKPOINT_DIR}"
