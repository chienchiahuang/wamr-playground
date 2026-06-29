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
wasm-micro-runtime/            # WAMR submodule
FreeRTOS-Kernel/               # FreeRTOS submodule
wasm-apps/
  sensor.c                     # Button reader module
  actuator.c                   # LED controller module
  alert.c                      # Alert notification module
  alert_updated.c              # Alternative alert (OTA demo)
  hello.c / hello_small.c      # Simple test apps
tools/
  wasm_send.py                 # OTA upload tool (serial)
host-maclinux/                 # Desktop host runtime
host-stm32/                    # Bare-metal Cortex-M4 (no OS)
host-freertos/                 # FreeRTOS on STM32L476 (OTA)
host-zephyr/                   # Zephyr RTOS (multi-module OTA)
  src/
    main.c                     # WAMR init + native API registration
    supervisor.c               # Module lifecycle + OTA handler
    wasm_runner.c              # Load → run → unload per module
    wasm_flash.c               # Flash persistence (3 slots)
    uart_loader.c              # UART OTA protocol parser
    native_api.c               # GPIO buttons/LEDs + shared state
  boards/
    nrf52840dk_nrf52840.overlay # Flash partition layout
```

## Software Architecture (nRF52840DK Multi-Module)

```
┌─────────────────────────────────────────────────────────────┐
│                    Host PC                                  │
│  wasm_send.py -s <slot> module.wasm                        │
│       │ UART (115200 baud)                                 │
└───────┼─────────────────────────────────────────────────────┘
        │
┌───────▼─────────────────────────────────────────────────────┐
│  nRF52840DK Firmware                                       │
│                                                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Supervisor Thread (100ms cycle)                     │  │
│  │                                                      │  │
│  │  ┌──────────┐   ┌──────────┐   ┌──────────┐        │  │
│  │  │ sensor   │──▶│ actuator │──▶│  alert   │        │  │
│  │  │  .wasm   │   │  .wasm   │   │  .wasm   │        │  │
│  │  └────┬─────┘   └────┬─────┘   └────┬─────┘        │  │
│  │       │              │              │               │  │
│  │  load→run→unload  load→run→unload  load→run→unload  │  │
│  └──────────────────────┼──────────────────────────────┘  │
│                         │                                  │
│  ┌──────────────────────▼──────────────────────────────┐  │
│  │  Native API (C, registered globally)                │  │
│  │                                                      │  │
│  │  read_button(n) ──▶ GPIO Input  (4 buttons)         │  │
│  │  set_led(n, on) ──▶ GPIO Output (4 LEDs)            │  │
│  │  set_data(k, v) ──▶ Shared State [16 slots]         │  │
│  │  get_data(k)    ◀── Shared State [16 slots]         │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                            │
│  ┌─────────────────────────────────────────────────────┐  │
│  │  UART Loader                                        │  │
│  │  Protocol: MAGIC + CMD + SLOT + SIZE + payload + CRC│  │
│  │  ISR → Ring Buffer (1KB) → Protocol Parser          │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                            │
│  ┌─────────────────────────────────────────────────────┐  │
│  │  Flash Storage (nRF52840 internal flash)            │  │
│  │                                                      │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐          │  │
│  │  │ Slot 0   │  │ Slot 1   │  │ Slot 2   │          │  │
│  │  │ sensor   │  │ actuator │  │ alert    │          │  │
│  │  │ 16KB     │  │ 16KB     │  │ 16KB     │          │  │
│  │  │ @0xF4000 │  │ @0xF8000 │  │ @0xFC000 │          │  │
│  │  └──────────┘  └──────────┘  └──────────┘          │  │
│  │  Header: magic + size + CRC32 + version             │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                            │
│  ┌─────────────────────────────────────────────────────┐  │
│  │  WAMR Runtime (79KB heap pool)                      │  │
│  │  Interpreter mode, one module loaded at a time      │  │
│  └─────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────┘

Data Flow per Cycle:
  sensor.wasm:   read_button(0..3) → set_data(0..3, state)
  actuator.wasm: get_data(0..3) → set_led(0..3, state)
  alert.wasm:    get_data(0..3) → printf("[ALERT] ...")
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
make stm32-nucleo
```

Flash:
```bash
make flash-stm32-nucleo
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
make freertos-nucleo
```

Flash:
```bash
make flash-freertos-nucleo
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
make zephyr-nucleo         # STM32 Nucleo
make zephyr-nrf52840       # nRF52840DK
```

First run builds the Docker image (~5 min). Output files land in `host-zephyr/output/`.

### Flash

```bash
make flash-zephyr-nucleo       # flash via OpenOCD + ST-Link
make flash-zephyr-nrf52840     # flash via JLinkExe
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
--- WAMR Multi-Module Runtime ---
[native] GPIO ready: 4 buttons, 4 LEDs
[flash] 3 slots ready (16384 bytes each)
[loader] UART RX ready
[runner] sensor loaded and ready
[runner] actuator loaded and ready
[runner] alert loaded and ready
[supervisor] all modules loaded, running...
```

Press buttons → LEDs light up, UART shows `[ALERT] Button X pressed!`

## Multi-Module Wasm Architecture

The Zephyr nRF52840DK firmware runs **three independent wasm modules** in a 100ms cycle.
Each module can be OTA-updated independently over UART.
All modules are **loaded once at boot and kept in RAM** — no per-cycle flash overhead.

### Architecture

```
Supervisor (100ms cycle):
  ┌─ sensor.wasm    → read_button(0..3) → set_data()
  ├─ actuator.wasm  → get_data() → set_led(0..3)
  └─ alert.wasm     → get_data() → printf("[ALERT] ...")

Native API (C, registered globally via wasm_runtime_register_natives):
  read_button(n)     → GPIO input (4 buttons on DK)
  set_led(n, on)     → GPIO output (4 LEDs on DK)
  set_data(key, val) → shared key-value store (int32[16])
  get_data(key)      → shared key-value store (int32[16])
```

Modules communicate through `set_data`/`get_data` — a C-side shared array
that persists between module executions. Updating `alert.wasm`
doesn't affect sensor or actuator logic.

### Execution Model

All 3 modules are loaded from flash at boot and stay instantiated in RAM.
Each cycle calls `execute_main()` on each — no load/unload overhead.
Only reloads a module when it receives an OTA update.

| Approach | Per-module overhead | WAMR pool | Version |
|----------|-------------------|-----------|---------|
| Load from flash per cycle | ~1.4 ms | 79 KB | v0.3.0 |
| **Keep loaded in RAM** | **~0.1 ms** | **220 KB** | **v0.3.1** |

### Flash Slots (3 × 16KB)

Each module is stored in its own flash slot and persists across reboots:

| Slot | Module | Flash address |
|------|--------|---------------|
| 0 | sensor.wasm | 0xF4000 |
| 1 | actuator.wasm | 0xF8000 |
| 2 | alert.wasm | 0xFC000 |

### OTA Individual Modules

**Prerequisites:** `pyserial` (`pip install pyserial`)

1. Build & flash:
   ```bash
   make zephyr-nrf52840
   make flash-zephyr-nrf52840
   ```

2. Press buttons → LEDs light up, UART shows `[ALERT] Button X pressed!`

3. Update only the alert logic:
   ```bash
   python3 tools/wasm_send.py -p /dev/tty.usbmodem* -s 2 wasm-apps/alert_updated.wasm
   ```

4. Now alerts show `[ALERT-v2]` — sensor and actuator unchanged.

5. Power cycle → alert_updated persists in flash slot 2.

### Writing Custom Modules

Modules import native functions from `"env"`:

```c
// Available imports:
extern int read_button(int n);     // n=0..3, returns 0 or 1
extern void set_led(int n, int on); // n=0..3
extern void set_data(int key, int value); // key=0..15
extern int get_data(int key);      // key=0..15
extern int printf(const char *fmt, ...); // UART output

int main(void) {
    // your logic here
    return 0;
}
```

Compile and OTA:
```bash
/opt/wasi-sdk/bin/clang --target=wasm32 -nostdlib \
    -Wl,--no-entry -Wl,--export=main -Wl,--allow-undefined \
    -Wl,--initial-memory=65536 -Wl,-z,stack-size=4096 \
    -O2 -fno-builtin -o my_module.wasm my_module.c

python3 tools/wasm_send.py -p /dev/tty.usbmodem* -s <slot> my_module.wasm
```

### OTA Protocol

Binary protocol over UART (shared with console, 115200 baud):

```
MAGIC(4B "WASM") + CMD(1B) + SLOT(1B) + SIZE(4B LE) + '\n' + PAYLOAD + CRC32(4B LE)
```

| CMD | Description |
|-----|-------------|
| `0x01` | Upload wasm to target slot |
| `0x02` | Query status |

| Response | Meaning |
|----------|---------|
| `OK` | Upload received, module updated |
| `ERR:CRC` | CRC-32 mismatch, upload rejected |
| `ERR:SIZE` | Payload too large for buffer (max 8KB) |
| `ERR:SLOT` | Invalid slot number |
| `ERR:FLASH` | Flash write failed |
| `ERR:LOAD` | wasm_runtime_load failed, fell back to default |

### OTA Flow

1. Host sends wasm binary with target slot number
2. Device validates CRC-32, writes to flash slot
3. Unloads old module from RAM, reloads from flash
4. New module runs on next cycle — no reboot needed
5. Power cycle → module persists in flash

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

### nRF52840DK Zephyr (multi-module, v0.3.1)

| Resource | Used | Available | Details |
|----------|------|-----------|---------|
| Flash | 117 KB (11.2%) | 1024 KB | Firmware + 3 × 16KB wasm slots |
| RAM | 252 KB (96.1%) | 256 KB | 220KB WAMR pool (3 modules × 64KB linear mem) |

### RAM Breakdown (nRF52840DK)

| Component | Size | Notes |
|-----------|------|-------|
| WAMR heap pool | 220 KB | Holds 3 modules simultaneously |
| Wasm OTA buffer | 8 KB | Receives incoming wasm |
| UART ring buffer | 1 KB | ISR → supervisor |
| Thread stacks | 6 KB | Supervisor + main + interrupt |
| Zephyr kernel | ~4 KB | Kernel objects, device state |
| Log buffer | 4 KB | Zephyr log system |
| Embedded wasm defaults | ~2 KB | Factory fallback |
| Shared state + misc | ~7 KB | GPIO, native symbols |

### Key WAMR Settings

- `WAMR_BUILD_INTERP=1` — interpreter only (no AOT/JIT)
- `WAMR_BUILD_FAST_INTERP=1` — faster interpreter variant
- `WAMR_BUILD_LIBC_BUILTIN=1` — lightweight libc (printf, memcpy, etc.)
- `WAMR_BUILD_LIBC_WASI=0` — disable WASI (not available on bare metal)
- All optional features disabled (SIMD, ref types, threads, etc.)

## Bare-Metal Gotchas

1. **Enable FPU before C code** — WAMR compiled with `-mfloat-abi=hard` emits VFP instructions. The Cortex-M4 FPU is off by default; enable CPACR in a `naked` Reset_Handler before calling any C function.
2. **Copy wasm from flash to RAM** — `wasm_runtime_load` modifies the buffer in-place. Passing a flash pointer causes a HardFault.
3. **Provide `_sbrk`** — newlib's `malloc` needs it; the `nosys.specs` stub returns -1. Implement it growing from `end` (linker symbol).
4. **Use SRAM2 for WAMR pool** — keeps SRAM1 free for the wasm linear memory (64KB+) allocated by `malloc` at runtime.
5. **Don't `#define os_printf printf`** — use function declarations instead. The macro causes issues with WAMR's libc-builtin printf wrapper.
