#!/bin/bash
set -e

REBUILD=false
FLASH=false
FLASH_ONLY=false

while [ $# -gt 0 ]; do
    case "$1" in
        --rebuild) REBUILD=true; shift ;;
        --flash)   FLASH=true; shift ;;
        flash)     FLASH_ONLY=true; shift ;;
        clean)
            echo "--- Cleaning host-zephyr ---"
            rm -rf "$(cd "$(dirname "$0")" && pwd)/output"
            echo "Removed output/"
            exit 0
            ;;
        *)         break ;;
    esac
done

BOARD="${1:-nucleo_l476rg}"
IMAGE_NAME="wamr-zephyr-builder"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Map board names to WAMR build targets
case "$BOARD" in
    nucleo_l476rg)
        WAMR_TARGET="THUMBV7"
        ;;
    nrf52840dk_nrf52840)
        WAMR_TARGET="THUMBV7"
        ;;
    *)
        echo "Unknown board: $BOARD"
        echo "Supported boards: nucleo_l476rg, nrf52840dk_nrf52840"
        exit 1
        ;;
esac

flash_board() {
    local HEX="$SCRIPT_DIR/output/${BOARD}.hex"
    local ELF="$SCRIPT_DIR/output/${BOARD}.elf"

    case "$BOARD" in
        nucleo_l476rg)
            if command -v openocd >/dev/null 2>&1; then
                echo "=== Flashing ${BOARD} via OpenOCD + ST-Link ==="
                openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
                    -c "program ${ELF} verify reset exit"
            else
                echo "ERROR: openocd not found."
                echo ""
                echo "Install:  brew install open-ocd        (macOS)"
                echo "          sudo apt install openocd      (Linux)"
                echo ""
                echo "Then run: ./build.sh flash ${BOARD}"
                exit 1
            fi
            ;;
        nrf52840dk_nrf52840)
            if command -v JLinkExe >/dev/null 2>&1; then
                echo "=== Flashing ${BOARD} via JLinkExe ==="
                JLinkExe -device nRF52840_xxAA -if SWD -speed 4000 -autoconnect 1 <<JLINK
erase
loadfile ${HEX}
r
g
exit
JLINK
            elif command -v nrfjprog >/dev/null 2>&1; then
                echo "=== Flashing ${BOARD} via nrfjprog ==="
                nrfjprog --program "${HEX}" --chiperase --verify --reset
            else
                echo "ERROR: No flash tool found for ${BOARD}."
                echo ""
                echo "Install one of:"
                echo "  brew install --cask segger-jlink                  (JLinkExe — recommended)"
                echo "  brew install --cask nordic-nrf-command-line-tools  (nrfjprog)"
                echo ""
                echo "Then run: ./build.sh flash ${BOARD}"
                exit 1
            fi
            ;;
    esac
}

# Flash-only mode: skip build
if $FLASH_ONLY; then
    if [ ! -f "$SCRIPT_DIR/output/${BOARD}.hex" ] && [ ! -f "$SCRIPT_DIR/output/${BOARD}.elf" ]; then
        echo "No firmware found in output/. Run ./build.sh ${BOARD} first."
        exit 1
    fi
    flash_board
    exit 0
fi

# Compile wasm app and generate header
WASM_SRC="$SCRIPT_DIR/../wasm-apps/hello_small.c"
WASM_FILE="$SCRIPT_DIR/../wasm-apps/hello_small.wasm"
WASM_HEADER="$SCRIPT_DIR/src/wasm_hello.h"
WASI_CLANG="${WASI_SDK:-/opt/wasi-sdk}/bin/clang"

if [ ! -x "$WASI_CLANG" ]; then
    echo "ERROR: WASI SDK not found at $WASI_CLANG"
    echo "Install: https://github.com/aspect-build/aspect-workflows/wiki/Install-WASI-SDK"
    echo "Or set WASI_SDK=/path/to/wasi-sdk"
    exit 1
fi

echo "--- Compiling hello_small.c → hello_small.wasm ---"
"$WASI_CLANG" --target=wasm32 -nostdlib \
    -Wl,--no-entry -Wl,--export=main -Wl,--allow-undefined \
    -Wl,--initial-memory=65536 -Wl,-z,stack-size=4096 \
    -O2 -fno-builtin \
    -o "$WASM_FILE" "$WASM_SRC"

echo "--- Generating wasm_hello.h from hello_small.wasm ---"
{
    echo "unsigned char wasm_hello[] = {"
    xxd -i < "$WASM_FILE"
    echo "};"
    echo "unsigned int wasm_hello_len = $(wc -c < "$WASM_FILE" | tr -d ' ');"
} > "$WASM_HEADER"

echo "=== Building WAMR for ${BOARD} (${WAMR_TARGET}) ==="

# Build Docker image if not already built (or --rebuild requested)
if $REBUILD; then
    echo "--- Rebuilding Docker image ---"
    docker rmi "$IMAGE_NAME" 2>/dev/null || true
fi
if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "--- Building Docker image (may take a few minutes) ---"
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
        /root/zephyrproject/zephyr-sdk/arm-zephyr-eabi/bin/arm-zephyr-eabi-size build/zephyr/zephyr.elf
    "

echo ""
echo "Output files in host-zephyr/output/:"
ls -la "$SCRIPT_DIR/output/${BOARD}".*

# Flash if requested
if $FLASH; then
    echo ""
    flash_board
fi
