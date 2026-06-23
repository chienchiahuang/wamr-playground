# WAMR Hello World — macOS, Linux, STM32L476RG

Run WebAssembly "Hello World" on three platforms using
[wasm-micro-runtime](https://github.com/bytecodealliance/wasm-micro-runtime) (WAMR).

## Prerequisites

| Tool | macOS/Linux | STM32 |
|------|-------------|-------|
| CMake >= 3.14 | required | required |
| WASI SDK | required (compile `.wasm`) | required (compile `.wasm`) |
| Clang/GCC | host compiler | — |
| arm-none-eabi-gcc | — | required |
| ST-Link / OpenOCD | — | for flashing |

Install WASI SDK: https://github.com/aspect-build/rules_go/wiki/WASI-SDK

## Project Structure

```
wasm-micro-runtime/    # WAMR submodule
wasm-apps/
  hello.c              # Hello world source (WASI, ~54KB wasm)
  hello_small.c        # Minimal hello world (libc-builtin, ~500B wasm)
host-maclinux/         # Host runtime for macOS and Linux
host-stm32/            # Bare-metal host for STM32L476RG Nucleo
```

## Step 1: Compile the Wasm App

### Full WASI version (macOS/Linux)

```bash
/opt/wasi-sdk/bin/clang -O2 -o wasm-apps/hello.wasm wasm-apps/hello.c
```

### Minimal version (STM32 or any platform)

```bash
/opt/wasi-sdk/bin/clang --target=wasm32 -nostdlib \
    -Wl,--no-entry -Wl,--export=_start -Wl,--allow-undefined \
    -O2 -fno-builtin \
    -o wasm-apps/hello_small.wasm wasm-apps/hello_small.c
```

## Step 2a: Build & Run on macOS or Linux

```bash
mkdir -p host-maclinux/build && cd host-maclinux/build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# Run with the WASI wasm
./iwasm_hello ../../wasm-apps/hello.wasm

# Or run with the minimal wasm
./iwasm_hello ../../wasm-apps/hello_small.wasm
```

Expected output:
```
Hello World from WebAssembly!
Wasm app executed successfully.
```

## Step 2b: Build & Flash on STM32L476RG

First, compile `hello_small.wasm` (Step 1 above), then:

```bash
mkdir -p host-stm32/build && cd host-stm32/build
cmake ..
cmake --build . -j$(nproc)
```

This produces `wamr_stm32.bin`, `wamr_stm32.hex`, and `wamr_stm32.elf`.

### Flash with ST-Link

```bash
st-flash write wamr_stm32.bin 0x08000000
```

Or with OpenOCD:

```bash
openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
    -c "program wamr_stm32.elf verify reset exit"
```

### View Output

Connect a serial terminal to the Nucleo's ST-Link virtual COM port at **115200 baud**:

```bash
# macOS
screen /dev/cu.usbmodem* 115200

# Linux
screen /dev/ttyACM0 115200
```

Expected output:
```
--- WAMR on STM32L476RG ---
Hello World from WebAssembly!
Wasm executed OK!
--- Done ---
```

## Memory Footprint (STM32)

| Section | Size | Notes |
|---------|------|-------|
| .text (flash) | ~99 KB | WAMR interpreter + libc-builtin |
| .data (flash) | ~4 KB | Initialized data |
| .bss (SRAM1) | ~4 KB | Variables (excl. WAMR pool) |
| WAMR pool (SRAM2) | 24 KB | WAMR internal allocator |
| Wasm linear mem (SRAM1) | 64+8 KB | Allocated at runtime via malloc |
| Wasm binary (flash) | ~526 B | hello_small.wasm embedded |
| **Total flash** | **~104 KB / 1024 KB** | |
| **Total RAM** | **~100 KB / 128 KB** | |

### Key WAMR Settings for Small Footprint

- `WAMR_BUILD_INTERP=1` — interpreter only (no AOT/JIT)
- `WAMR_BUILD_FAST_INTERP=1` — faster interpreter variant
- `WAMR_BUILD_MINI_LOADER=1` — smaller wasm loader
- `WAMR_BUILD_LIBC_BUILTIN=1` — lightweight libc (printf, memcpy, etc.)
- `WAMR_BUILD_LIBC_WASI=0` — disable WASI (not available on bare metal)
- All optional features disabled (SIMD, ref types, threads, etc.)

### STM32 Bare-Metal Gotchas

1. **Enable FPU before C code** — WAMR compiled with `-mfloat-abi=hard` emits VFP instructions. The Cortex-M4 FPU is off by default; enable CPACR in a `naked` Reset_Handler before calling any C function.
2. **Copy wasm from flash to RAM** — `wasm_runtime_load` modifies the buffer in-place. Passing a flash pointer causes a HardFault.
3. **Provide `_sbrk`** — newlib's `malloc` needs it; the `nosys.specs` stub returns -1. Implement it growing from `end` (linker symbol).
4. **Use SRAM2 for WAMR pool** — keeps SRAM1 free for the wasm linear memory (64KB+) allocated by `malloc` at runtime.
5. **Don't `#define os_printf printf`** — use function declarations instead. The macro causes issues with WAMR's libc-builtin printf wrapper.
