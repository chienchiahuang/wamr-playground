#!/bin/bash
set -e

BOARD="${1:-nucleo_l476rg}"
IMAGE_NAME="wamr-zephyr-builder"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Map board names to WAMR build targets
case "$BOARD" in
    nucleo_l476rg)
        WAMR_TARGET="THUMBV7"
        ;;
    nucleo_n657x0)
        WAMR_TARGET="THUMBV7"
        ;;
    *)
        echo "Unknown board: $BOARD"
        echo "Supported boards: nucleo_l476rg, nucleo_n657x0"
        exit 1
        ;;
esac

echo "=== Building WAMR for ${BOARD} (${WAMR_TARGET}) ==="

# Build Docker image if not already built
if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "--- Building Docker image (first time, may take a few minutes) ---"
    docker build -t "$IMAGE_NAME" "$SCRIPT_DIR"
fi

# Run build inside Docker
mkdir -p "$SCRIPT_DIR/output"
docker run --rm \
    -v "$SCRIPT_DIR/src:/root/zephyrproject/application/src" \
    -v "$SCRIPT_DIR/boards:/root/zephyrproject/application/boards" \
    -v "$SCRIPT_DIR/prj.conf:/root/zephyrproject/application/prj.conf" \
    -v "$SCRIPT_DIR/CMakeLists.txt:/root/zephyrproject/application/CMakeLists.txt" \
    -v "$SCRIPT_DIR/output:/output" \
    "$IMAGE_NAME" \
    bash -c "
        cd /root/zephyrproject && \
        west build -p always -b ${BOARD} application -- \
            -DWAMR_BUILD_TARGET=${WAMR_TARGET} && \
        cp build/zephyr/zephyr.elf /output/${BOARD}.elf && \
        cp build/zephyr/zephyr.bin /output/${BOARD}.bin && \
        cp build/zephyr/zephyr.hex /output/${BOARD}.hex 2>/dev/null; \
        echo '=== Build complete ===' && \
        arm-zephyr-eabi-size build/zephyr/zephyr.elf
    "

echo ""
echo "Output files in host-zephyr/output/:"
ls -la "$SCRIPT_DIR/output/${BOARD}".*
echo ""
echo "To flash: openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \\"
echo "  -c \"program output/${BOARD}.elf verify reset exit\""
