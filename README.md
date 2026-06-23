# WAMR Playground

Run WebAssembly on multiple platforms using
[wasm-micro-runtime](https://github.com/bytecodealliance/wasm-micro-runtime) (WAMR).

## Supported Targets

| Target | Directory | OS | Status |
|--------|-----------|-----|--------|
| macOS / Linux | `host-maclinux/` | POSIX | working |
| STM32L476RG bare-metal | `host-stm32/` | None | working |
| STM32L476RG Zephyr | `host-zephyr/` | Zephyr 3.7.0 | working |

## Prerequisites

| Tool | macOS/Linux | STM32 bare-metal | STM32 Zephyr |
|------|-------------|------------------|--------------|
| CMake >= 3.14 | required | required | — |
| WASI SDK | required | required | — |
| Clang/GCC | required | — | — |
| arm-none-eabi-gcc | — | required | — |
| Docker | — | — | required |
| OpenOCD | — | for flashing | for flashing |

## Project Structure

```
wasm-micro-runtime/        # WAMR submodule
wasm-apps/
  hello.c                  # WASI hello world (~54KB wasm)
  hello_small.c            # Minimal hello world (~526B wasm, libc-builtin)
host-maclinux/             # Desktop host runtime
host-stm32/                # Bare-metal Cortex-M4 (no OS)
host-zephyr/               # Zephyr RTOS (Docker-based build)
  Dockerfile               # Zephyr SDK + west workspace
  build.sh                 # ./build.sh <board>
  boards/
    nucleo_l476rg.conf      # Per-board Kconfig
  src/
    main.c                  # Same for all boards
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

## Step 2c: Build & Flash — Zephyr (Docker)

No local Zephyr SDK needed — everything runs inside Docker:

```bash
cd host-zephyr
./build.sh nucleo_l476rg
```

First run builds the Docker image (~5 min). Output files land in `host-zephyr/output/`.

Flash with OpenOCD:
```bash
openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
    -c "program output/nucleo_l476rg.elf verify reset exit"
```

Serial output (Zephyr console):
```
*** Booting Zephyr OS build v3.7.0 ***
--- WAMR on Zephyr ---
Hello World from WebAssembly!
elapsed: 21 ms
```

### Adding a New Board

1. Create `host-zephyr/boards/<board_name>.conf` with board-specific Kconfig
2. Add the board to the `case` in `build.sh`
3. Run `./build.sh <board_name>`

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
