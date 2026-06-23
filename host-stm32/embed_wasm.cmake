# Converts hello_small.wasm to a C array for embedding in flash
set(WASM_FILE "${CMAKE_CURRENT_LIST_DIR}/../wasm-apps/hello_small.wasm")
set(OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/hello_wasm.c")

file(READ "${WASM_FILE}" WASM_HEX HEX)
string(LENGTH "${WASM_HEX}" WASM_HEX_LEN)
math(EXPR WASM_SIZE "${WASM_HEX_LEN} / 2")

set(C_ARRAY "")
set(i 0)
set(byte_index 0)
while(i LESS WASM_HEX_LEN)
    string(SUBSTRING "${WASM_HEX}" ${i} 2 BYTE)
    math(EXPR col "${byte_index} % 16")
    if(byte_index GREATER 0 AND col EQUAL 0)
        string(APPEND C_ARRAY ",\n    ")
    elseif(byte_index GREATER 0)
        string(APPEND C_ARRAY ",")
    else()
        string(APPEND C_ARRAY "\n    ")
    endif()
    string(APPEND C_ARRAY "0x${BYTE}")
    math(EXPR i "${i} + 2")
    math(EXPR byte_index "${byte_index} + 1")
endwhile()

file(WRITE "${OUTPUT_FILE}"
"#include <stdint.h>

__attribute__((section(\".wasm_bin\"), aligned(4)))
const uint8_t wasm_hello_bin[] = {${C_ARRAY}
};

const uint32_t wasm_hello_bin_len = ${WASM_SIZE};
")
