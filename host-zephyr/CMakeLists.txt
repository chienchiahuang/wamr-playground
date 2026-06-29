cmake_minimum_required(VERSION 3.14)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wamr_ota)

enable_language(ASM)

set(WAMR_BUILD_PLATFORM "zephyr")

if(NOT DEFINED WAMR_BUILD_TARGET)
  set(WAMR_BUILD_TARGET "THUMBV7")
endif()

set(WAMR_BUILD_INTERP 1)
set(WAMR_BUILD_FAST_INTERP 1)
set(WAMR_BUILD_AOT 0)
set(WAMR_BUILD_JIT 0)
set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI 0)
set(WAMR_BUILD_MINI_LOADER 0)
set(WAMR_BUILD_MULTI_MODULE 0)
set(WAMR_BUILD_REF_TYPES 0)
set(WAMR_BUILD_SIMD 0)

set(WAMR_BUILD_GLOBAL_HEAP_POOL 1)

if(BOARD STREQUAL "nucleo_l476rg")
  set(WAMR_BUILD_GLOBAL_HEAP_SIZE 73728)
  set(WASM_OTA_BUF_SIZE 2048)
  set(WASM_STACK_SIZE 2048)
  set(WASM_HEAP_SIZE 2048)
else()
  set(WAMR_BUILD_GLOBAL_HEAP_SIZE 225280)
  set(WASM_OTA_BUF_SIZE 8192)
  set(WASM_STACK_SIZE 2048)
  set(WASM_HEAP_SIZE 0)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-micro-runtime)
  set(WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-micro-runtime)
else()
  set(WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../modules/wasm-micro-runtime)
endif()

include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

target_sources(app PRIVATE
    ${WAMR_RUNTIME_LIB_SOURCE}
    src/main.c
    src/supervisor.c
    src/wasm_runner.c
    src/uart_loader.c
    src/wasm_flash.c
    src/native_api.c
)

target_compile_definitions(app PRIVATE
    WASM_OTA_BUF_SIZE=${WASM_OTA_BUF_SIZE}
    WASM_STACK_SIZE=${WASM_STACK_SIZE}
    WASM_HEAP_SIZE=${WASM_HEAP_SIZE}
)
target_compile_options(app PRIVATE -Wno-type-limits)
