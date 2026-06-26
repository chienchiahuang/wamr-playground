cmake_minimum_required(VERSION 3.14)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

project(wamr_freertos C ASM)

# --- FreeRTOS ---
set(FREERTOS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../FreeRTOS-Kernel)

set(FREERTOS_SOURCES
    ${FREERTOS_DIR}/tasks.c
    ${FREERTOS_DIR}/queue.c
    ${FREERTOS_DIR}/list.c
    ${FREERTOS_DIR}/timers.c
    ${FREERTOS_DIR}/event_groups.c
    ${FREERTOS_DIR}/stream_buffer.c
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F/port.c
    ${FREERTOS_DIR}/portable/MemMang/heap_4.c
)

# --- WAMR ---
set(WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-micro-runtime)

set(WAMR_BUILD_PLATFORM "freertos")
set(WAMR_BUILD_TARGET "THUMBV7_VFP")

set(WAMR_BUILD_INTERP 1)
set(WAMR_BUILD_FAST_INTERP 1)
set(WAMR_BUILD_AOT 0)
set(WAMR_BUILD_JIT 0)
set(WAMR_BUILD_FAST_JIT 0)
set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI 0)
set(WAMR_BUILD_MULTI_MODULE 0)
set(WAMR_BUILD_MINI_LOADER 1)
set(WAMR_BUILD_LIB_PTHREAD 0)
set(WAMR_BUILD_REF_TYPES 0)
set(WAMR_BUILD_SIMD 0)
set(WAMR_BUILD_BULK_MEMORY 0)
set(WAMR_BUILD_SHARED_MEMORY 0)
set(WAMR_BUILD_THREAD_MGR 0)
set(WAMR_BUILD_TAIL_CALL 0)
set(WAMR_BUILD_DEBUG_INTERP 0)
set(WAMR_BUILD_MEMORY_PROFILING 0)
set(WAMR_BUILD_PERF_PROFILING 0)
set(WAMR_BUILD_DUMP_CALL_STACK 0)
set(WAMR_BUILD_GLOBAL_HEAP_POOL 0)
set(WAMR_DISABLE_HW_BOUND_CHECK 1)

set(SHARED_PLATFORM_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/stm32_platform.cmake)

include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

# --- MCU flags ---
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MCU_FLAGS} -Wall -ffunction-sections -fdata-sections -Os")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${MCU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MCU_FLAGS} -T${CMAKE_CURRENT_SOURCE_DIR}/stm32l476rg.ld -Wl,--gc-sections --specs=nosys.specs")

# --- Libraries ---
add_library(vmlib ${WAMR_RUNTIME_LIB_SOURCE})
target_compile_options(vmlib PRIVATE -Wno-unused-parameter -Wno-sign-compare)
target_include_directories(vmlib PRIVATE
    ${FREERTOS_DIR}/include
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(freertos ${FREERTOS_SOURCES})
target_include_directories(freertos PUBLIC
    ${FREERTOS_DIR}/include
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(freertos PRIVATE -Wno-unused-parameter)

# --- Executable ---
add_executable(wamr_freertos.elf
    src/startup_stm32l476rg.c
    src/main.c
    src/supervisor.c
    src/wasm_runner.c
    src/uart_loader.c
    ${CMAKE_CURRENT_BINARY_DIR}/hello_wasm.c
)

target_include_directories(wamr_freertos.elf PRIVATE
    src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FREERTOS_DIR}/include
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F
    ${WAMR_ROOT_DIR}/core/iwasm/include
    ${WAMR_ROOT_DIR}/core/shared/platform/include
)

target_link_libraries(wamr_freertos.elf vmlib freertos -lm)

# --- Wasm compilation ---
set(WASI_SDK_DIR "$ENV{WASI_SDK}" CACHE PATH "WASI SDK path")
if(NOT WASI_SDK_DIR)
    set(WASI_SDK_DIR "/opt/wasi-sdk")
endif()
set(WASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-apps/hello_small.c)
set(WASM_OUT ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-apps/hello_small.wasm)

add_custom_command(OUTPUT ${WASM_OUT}
    COMMAND ${WASI_SDK_DIR}/bin/clang --target=wasm32 -nostdlib
        -Wl,--no-entry -Wl,--export=main -Wl,--allow-undefined
        -Wl,--initial-memory=65536 -Wl,-z,stack-size=4096
        -O2 -fno-builtin
        -o ${WASM_OUT} ${WASM_SRC}
    DEPENDS ${WASM_SRC}
    COMMENT "Compiling hello_small.c → hello_small.wasm"
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/hello_wasm.c
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/embed_wasm.cmake
    DEPENDS ${WASM_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Embedding hello_small.wasm as C array"
)

add_custom_command(TARGET wamr_freertos.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary wamr_freertos.elf wamr_freertos.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex wamr_freertos.elf wamr_freertos.hex
    COMMAND ${CMAKE_SIZE} wamr_freertos.elf
    COMMENT "Generating .bin and .hex, showing size"
)
