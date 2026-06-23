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

project(wamr_stm32 C ASM)

set(WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-micro-runtime)

# --- WAMR configuration: minimal footprint for Cortex-M4 ---
set(WAMR_BUILD_PLATFORM "linux")  # use linux platform layer as base
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

# Override the platform config to use our custom platform
set(SHARED_PLATFORM_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/stm32_platform.cmake)

include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

# --- MCU flags ---
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MCU_FLAGS} -Wall -ffunction-sections -fdata-sections -Os")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${MCU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MCU_FLAGS} -T${CMAKE_CURRENT_SOURCE_DIR}/stm32l476rg.ld -Wl,--gc-sections --specs=nosys.specs")

add_library(vmlib ${WAMR_RUNTIME_LIB_SOURCE})
target_compile_options(vmlib PRIVATE -Wno-unused-parameter -Wno-sign-compare)

add_executable(wamr_stm32.elf
    src/startup_stm32l476rg.c
    src/main.c
    src/platform_stm32.c
    ${CMAKE_CURRENT_BINARY_DIR}/hello_wasm.c
)

target_include_directories(wamr_stm32.elf PRIVATE
    src
    ${WAMR_ROOT_DIR}/core/iwasm/include
    ${WAMR_ROOT_DIR}/core/shared/platform/include
)

target_link_libraries(wamr_stm32.elf vmlib -lm)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/hello_wasm.c
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/embed_wasm.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-apps/hello_small.wasm
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Converting hello.wasm to C array"
)

add_custom_command(TARGET wamr_stm32.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary wamr_stm32.elf wamr_stm32.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex wamr_stm32.elf wamr_stm32.hex
    COMMAND ${CMAKE_SIZE} wamr_stm32.elf
    COMMENT "Generating .bin and .hex, showing size"
)
