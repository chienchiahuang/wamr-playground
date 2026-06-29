WASI_SDK ?= /opt/wasi-sdk
WASI_CLANG := $(WASI_SDK)/bin/clang

# --- Wasm apps ---
.PHONY: wasm clean clean-maclinux clean-stm32 clean-freertos clean-zephyr clean-all

wasm: wasm-apps/hello.wasm wasm-apps/hello_small.wasm

wasm-apps/hello.wasm: wasm-apps/hello.c
	$(WASI_CLANG) -O2 -o $@ $<

wasm-apps/hello_small.wasm: wasm-apps/hello_small.c
	$(WASI_CLANG) --target=wasm32 -nostdlib \
		-Wl,--no-entry -Wl,--export=main -Wl,--allow-undefined \
		-Wl,--initial-memory=65536 -Wl,-z,stack-size=4096 \
		-O2 -fno-builtin \
		-o $@ $<

# --- macOS / Linux ---
.PHONY: maclinux
maclinux:
	mkdir -p host-maclinux/build
	cd host-maclinux/build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j$$(sysctl -n hw.ncpu 2>/dev/null || nproc)

clean-maclinux:
	rm -rf host-maclinux/build

# --- STM32 bare-metal ---
.PHONY: stm32-nucleo flash-stm32-nucleo
stm32-nucleo:
	mkdir -p host-stm32/build
	cd host-stm32/build && cmake .. && cmake --build . -j$$(sysctl -n hw.ncpu 2>/dev/null || nproc)

flash-stm32-nucleo:
	openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
		-c "program host-stm32/build/wamr_stm32.elf verify reset exit"

clean-stm32:
	rm -rf host-stm32/build

# --- STM32 FreeRTOS ---
.PHONY: freertos-nucleo flash-freertos-nucleo clean-freertos
freertos-nucleo:
	git submodule update --init FreeRTOS-Kernel
	mkdir -p host-freertos/build
	cd host-freertos/build && cmake .. && cmake --build . -j$$(sysctl -n hw.ncpu 2>/dev/null || nproc)

flash-freertos-nucleo:
	openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
		-c "program host-freertos/build/wamr_freertos.elf verify reset exit"

clean-freertos:
	rm -rf host-freertos/build

# --- Zephyr (Docker) ---
.PHONY: zephyr-nucleo zephyr-nrf52840 flash-zephyr-nucleo flash-zephyr-nrf52840
zephyr-nucleo:
	cd host-zephyr && ./build.sh nucleo_l476rg

zephyr-nrf52840:
	cd host-zephyr && ./build.sh nrf52840dk

flash-zephyr-nucleo:
	cd host-zephyr && ./build.sh flash nucleo_l476rg

flash-zephyr-nrf52840:
	cd host-zephyr && ./build.sh flash nrf52840dk

clean-zephyr:
	cd host-zephyr && ./build.sh clean

# --- Clean all ---
clean: clean-maclinux clean-stm32 clean-freertos clean-zephyr

clean-all: clean
	rm -f wasm-apps/hello.wasm wasm-apps/hello_small.wasm
