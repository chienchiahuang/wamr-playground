#ifndef WASM_FLASH_H
#define WASM_FLASH_H

#include <stdint.h>
#include <stdbool.h>

#define WASM_SLOT_A  0
#define WASM_SLOT_B  1

int wasm_flash_init(void);

bool wasm_flash_valid(int slot);

uint32_t wasm_flash_version(int slot);

uint32_t wasm_flash_read(int slot, uint8_t *buf, uint32_t buf_size);

int wasm_flash_write(int slot, const uint8_t *data, uint32_t size);

#endif
