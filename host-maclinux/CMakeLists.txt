cmake_minimum_required(VERSION 3.14)
project(iwasm_hello)

set(WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-micro-runtime)

if(NOT DEFINED WAMR_BUILD_PLATFORM)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(WAMR_BUILD_PLATFORM "darwin")
  else()
    set(WAMR_BUILD_PLATFORM "linux")
  endif()
endif()

if(NOT DEFINED WAMR_BUILD_TARGET)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)")
    set(WAMR_BUILD_TARGET "AARCH64")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WAMR_BUILD_TARGET "X86_64")
  else()
    set(WAMR_BUILD_TARGET "X86_32")
  endif()
endif()

set(WAMR_BUILD_INTERP 1)
set(WAMR_BUILD_FAST_INTERP 1)
set(WAMR_BUILD_AOT 0)
set(WAMR_BUILD_JIT 0)
set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI 1)
set(WAMR_BUILD_MULTI_MODULE 0)
set(WAMR_BUILD_MINI_LOADER 0)
set(WAMR_BUILD_REF_TYPES 1)
set(WAMR_BUILD_BULK_MEMORY 1)

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

add_library(vmlib ${WAMR_RUNTIME_LIB_SOURCE})

add_executable(iwasm_hello main.c)
target_link_libraries(iwasm_hello vmlib -lm -lpthread)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_link_libraries(iwasm_hello -ldl)
endif()

set(WASI_SDK_DIR "$ENV{WASI_SDK}" CACHE PATH "WASI SDK path")
if(NOT WASI_SDK_DIR)
    set(WASI_SDK_DIR "/opt/wasi-sdk")
endif()

set(WASM_SRC_FULL ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-apps/hello.c)
set(WASM_OUT_FULL ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-apps/hello.wasm)
set(WASM_SRC_SMALL ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-apps/hello_small.c)
set(WASM_OUT_SMALL ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-apps/hello_small.wasm)

add_custom_command(OUTPUT ${WASM_OUT_FULL}
    COMMAND ${WASI_SDK_DIR}/bin/clang -O2 -o ${WASM_OUT_FULL} ${WASM_SRC_FULL}
    DEPENDS ${WASM_SRC_FULL}
    COMMENT "Compiling hello.c → hello.wasm (WASI)"
)

add_custom_command(OUTPUT ${WASM_OUT_SMALL}
    COMMAND ${WASI_SDK_DIR}/bin/clang --target=wasm32 -nostdlib
        -Wl,--no-entry -Wl,--export=main -Wl,--allow-undefined
        -Wl,--initial-memory=65536 -Wl,-z,stack-size=4096
        -O2 -fno-builtin
        -o ${WASM_OUT_SMALL} ${WASM_SRC_SMALL}
    DEPENDS ${WASM_SRC_SMALL}
    COMMENT "Compiling hello_small.c → hello_small.wasm (minimal)"
)

add_custom_target(wasm_apps ALL DEPENDS ${WASM_OUT_FULL} ${WASM_OUT_SMALL})
