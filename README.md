# WAMR Playground

Run WebAssembly on multiple platforms using
[wasm-micro-runtime](https://github.com/bytecodealliance/wasm-micro-runtime) (WAMR).

## Supported Targets

| Target | Directory | OS | Status |
|--------|-----------|-----|--------|
| macOS / Linux | `host-maclinux/` | POSIX | working |
| STM32L476RG bare-metal | `host-stm32/` | None | working |
| STM32L476RG FreeRTOS | `host-freertos/` | FreeRTOS | working |
| STM32L476RG Zephyr | `host-zephyr/` | Zephyr 3.7.0 | working |
| nRF52840DK Zephyr | `host-zephyr/` | Zephyr 3.7.0 | working |

## Prerequisites

| Tool | macOS/Linux | STM32 bare-metal | STM32 FreeRTOS | Zephyr (build) | Zephyr (flash) |
|------|-------------|------------------|----------------|----------------|-----------------|
| CMake >= 3.14 | required | required | required | — | — |
| WASI SDK | required | required | required | — | — |
| Clang/GCC | required | — | — | — | — |
| arm-none-eabi-gcc | — | required | required | — | — |
| Docker | — | — | — | required | — |
| OpenOCD | — | for flashing | for flashing | — | nucleo_l476rg |
| JLinkExe | — | — | — | — | nrf52840dk |

### Flash Tool Installation

| Board | Tool | macOS | Linux |
|-------|------|-------|-------|
| nucleo_l476rg | OpenOCD | `brew install open-ocd` | `sudo apt install openocd` |
| nrf52840dk | JLinkExe | `brew install --cask segger-jlink` | [Download from SEGGER](https://www.segger.com/downloads/jlink/) |

> **macOS note:** After installing SEGGER J-Link, go to **System Settings → Privacy & Security → Full Disk Access** and enable your terminal app (Terminal or VS Code).

## Project Structure

```
wasm-micro-runtime/        # WAMR submodule
wasm-apps/
  hello.c                  # WASI hello world (~54KB wasm)
  hello_small.c            # Minimal hello world (~526B wasm, libc-builtin)
FreeRTOS-Kernel/           # FreeRTOS submodule
host-maclinux/             # Desktop host runtime
host-stm32/                # Bare-metal Cortex-M4 (no OS)
host-freertos/             # FreeRTOS on STM32L476 (real threading)
host-zephyr/               # Zephyr RTOS (Docker-based build)
  Dockerfile               # Zephyr SDK + west workspace
  build.sh                 # Build & flash script
  boards/
    nucleo_l476rg.conf     # STM32 Nucleo board overlay
    nrf52840dk_nrf52840.conf # nRF52840DK board overlay
  src/
    main.c                 # Same for all boards
```

## Step 1: Compile the Wasm App

### Full WASI version (macOS/Linux)

```bash
/opt/wasi-sdk/bin/clang -O2 -o wasm-apps/hello.wasm wasm-apps/hello.c
```

### Minimal version (STM32 / Zephyr / any platform)

```bash
/opt/wasi-sdk/bin/clang --target=wasm32 -nostdlib \
    -Wl,--no-entry -Wl,--export=main -Wl,--allow-undefined \
    -Wl,--initial-memory=65536 -Wl,-z,stack-size=4096 \
    -O2 -fno-builtin \
    -o wasm-apps/hello_small.wasm wasm-apps/hello_small.c
```

## Step 2a: Build & Run on macOS or Linux

```bash
mkdir -p host-maclinux/build && cd host-maclinux/build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

./iwasm_hello ../../wasm-apps/hello.wasm
```

Expected output:
```
Hello World from WebAssembly!
Wasm app executed successfully.
```

## Step 2b: Build & Flash — STM32 Bare-Metal

First, compile `hello_small.wasm` (Step 1 above), then:

```bash
mkdir -p host-stm32/build && cd host-stm32/build
cmake ..
cmake --build . -j$(nproc)
```

Flash with OpenOCD:
```bash
openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
    -c "program wamr_stm32.elf verify reset exit"
```

Serial output at **115200 baud**:
```
--- WAMR on STM32L476RG ---
Hello World from WebAssembly!
Wasm executed OK!
--- Done ---
```

## Step 2c: Build & Flash — STM32 FreeRTOS

Same hardware as bare-metal, but runs WAMR as a FreeRTOS task with real threading support.
Includes OTA supervisor for hot-swapping wasm modules over UART.
PLL configured for 80 MHz (vs 4 MHz default MSI).

```bash
git submodule update --init FreeRTOS-Kernel
mkdir -p host-freertos/build && cd host-freertos/build
cmake ..
cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

Flash with OpenOCD:
```bash
openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
    -c "program wamr_freertos.elf verify reset exit"
```

Serial output at **115200 baud**:
```
--- WAMR OTA on FreeRTOS ---
[loader] UART RX ready
[supervisor] loading default wasm
[supervisor] wasm running, waiting for OTA uploads...
Hello World from WebAssembly!
[runner] elapsed: 1 ms
```

OTA upload (same as Zephyr target):
```bash
python3 tools/wasm_send.py -p /dev/tty.usbmodem* wasm-apps/ota_test.wasm
```

> **STM32 note:** ST-Link V2 VCP needs firmware ≥ V2J46 for serial RX.
> Update via **STM32CubeProgrammer → Firmware Update → Upgrade**.

### Memory Layout

| Region | Used | Available | Contents |
|--------|------|-----------|----------|
| Flash | 113 KB | 1024 KB | Code + wasm binary |
| SRAM1 | ~91 KB | 96 KB | .data, .bss, FreeRTOS heap (16KB), newlib heap (64KB) |
| SRAM2 | 24 KB | 32 KB | WAMR pool |

## Step 2d: Build & Flash — Zephyr (Docker)

No local Zephyr SDK needed — everything runs inside Docker.

### Build

```bash
cd host-zephyr
./build.sh nucleo_l476rg          # STM32 Nucleo
./build.sh nrf52840dk_nrf52840    # nRF52840DK
```

First run builds the Docker image (~5 min). Output files land in `host-zephyr/output/`.

### Flash

```bash
./build.sh flash nucleo_l476rg          # flash via OpenOCD + ST-Link
./build.sh flash nrf52840dk_nrf52840    # flash via JLinkExe
```

Or build + flash in one step:

```bash
./build.sh --flash nucleo_l476rg
./build.sh --flash nrf52840dk_nrf52840
```

### Rebuild Docker Image

If you change `west_lite.yml` or `Dockerfile`, force an image rebuild:

```bash
./build.sh --rebuild nrf52840dk_nrf52840
```

### Serial Output

Connect at **115200 baud** to see:

```
*** Booting Zephyr OS build v3.7.0 ***
--- WAMR OTA Runtime ---
[supervisor] loading default wasm
[supervisor] wasm running, waiting for OTA uploads...
Hello World from WebAssembly!
[runner] elapsed: 1 ms
```

## Wasm OTA (Over-The-Air Updates)

The Zephyr firmware includes a **supervisor + wasm runner** architecture that allows hot-swapping wasm modules over UART without reflashing the MCU.

### Architecture

```
Supervisor thread (always running)
  ├── Boots default wasm from embedded header
  ├── Listens on UART for incoming .wasm binaries
  └── On upload: stop current → load new → restart
        On failure: fall back to default wasm

Wasm runner thread (managed by supervisor)
  └── Executes the current wasm module
```

### OTA Upload

**Prerequisites:** `pyserial` (`pip install pyserial`)

1. Build & flash the firmware:
   ```bash
   cd host-zephyr
   ./build.sh --flash nrf52840dk_nrf52840
   ```

2. Find your serial port:
   ```bash
   ls /dev/tty.usbmodem*     # macOS
   ls /dev/ttyACM*            # Linux
   ```

3. Write a new wasm app:
   ```c
   // wasm-apps/my_app.c
   extern int printf(const char *fmt, ...);
   int main(void) {
       printf("Hello from OTA!\n");
       return 0;
   }
   ```

4. Compile to wasm:
   ```bash
   /opt/wasi-sdk/bin/clang --target=wasm32 -nostdlib \
       -Wl,--no-entry -Wl,--export=main -Wl,--allow-undefined \
       -Wl,--initial-memory=65536 -Wl,-z,stack-size=4096 \
       -O2 -fno-builtin \
       -o wasm-apps/my_app.wasm wasm-apps/my_app.c
   ```

5. Send over serial:
   ```bash
   python3 tools/wasm_send.py -p /dev/tty.usbmodem* wasm-apps/my_app.wasm
   ```

6. The device responds `OK` and the new wasm runs immediately — no reboot needed.

### OTA Protocol

Binary protocol over UART (shared with console, 115200 baud):

```
MAGIC(4B "WASM") + CMD(1B) + SIZE(4B LE) + '\n' + PAYLOAD(SIZE B) + CRC32(4B LE)
```

| CMD | Description |
|-----|-------------|
| `0x01` | Upload wasm binary |
| `0x02` | Query status |

| Response | Meaning |
|----------|---------|
| `OK` | Upload received, wasm swapped successfully |
| `ERR:CRC` | CRC-32 mismatch, upload rejected |
| `ERR:SIZE` | Payload too large for buffer (max 8KB) |
| `ERR:LOAD` | wasm_runtime_load failed, fell back to default |

### Max Wasm Binary Size

| Board | Max size | OTA status |
|-------|----------|------------|
| nRF52840DK | 8 KB | Working (ISR-based UART RX) |
| nucleo_l476rg | 2 KB | Builds, OTA requires ST-Link V2 firmware ≥ V2J46 |

> **STM32 note:** The ST-Link V2 VCP needs a firmware update for host→device serial RX.
> Update via **STM32CubeProgrammer → Firmware Update → Upgrade**.

### Adding a New Board

1. Create `host-zephyr/boards/<board_name>.conf` with board-specific Kconfig
2. Add the board's HAL to `west_lite.yml` if needed
3. Add the board to the `case` in `build.sh`
4. Run `./build.sh --rebuild <board_name>` (first time, to pull the new HAL)

## Memory Footprint

### STM32L476RG Bare-Metal

| Resource | Used | Available |
|----------|------|-----------|
| Flash | ~104 KB | 1024 KB |
| SRAM1 | ~76 KB | 96 KB |
| SRAM2 | 24 KB (WAMR pool) | 32 KB |

### STM32L476RG Zephyr

| Resource | Used | Available |
|----------|------|-----------|
| Flash | 92 KB (8.8%) | 1024 KB |
| RAM | 97.6 KB (99.3%) | 96 KB |

### nRF52840DK Zephyr (with OTA supervisor)

| Resource | Used | Available |
|----------|------|-----------|
| Flash | 99 KB (9.4%) | 1024 KB |
| RAM | 108 KB (41.2%) | 256 KB |

### Key WAMR Settings for Small Footprint

- `WAMR_BUILD_INTERP=1` — interpreter only (no AOT/JIT)
- `WAMR_BUILD_FAST_INTERP=1` — faster interpreter variant
- `WAMR_BUILD_MINI_LOADER=1` — smaller wasm loader
- `WAMR_BUILD_LIBC_BUILTIN=1` — lightweight libc (printf, memcpy, etc.)
- `WAMR_BUILD_LIBC_WASI=0` — disable WASI (not available on bare metal)
- All optional features disabled (SIMD, ref types, threads, etc.)

## Bare-Metal Gotchas

1. **Enable FPU before C code** — WAMR compiled with `-mfloat-abi=hard` emits VFP instructions. The Cortex-M4 FPU is off by default; enable CPACR in a `naked` Reset_Handler before calling any C function.
2. **Copy wasm from flash to RAM** — `wasm_runtime_load` modifies the buffer in-place. Passing a flash pointer causes a HardFault.
3. **Provide `_sbrk`** — newlib's `malloc` needs it; the `nosys.specs` stub returns -1. Implement it growing from `end` (linker symbol).
4. **Use SRAM2 for WAMR pool** — keeps SRAM1 free for the wasm linear memory (64KB+) allocated by `malloc` at runtime.
5. **Don't `#define os_printf printf`** — use function declarations instead. The macro causes issues with WAMR's libc-builtin printf wrapper.
