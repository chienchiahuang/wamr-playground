#ifndef WASM_RUNNER_H
#define WASM_RUNNER_H

#include <stdint.h>
#include <stdbool.h>

void wasm_runner_init(void);

bool wasm_runner_load_embedded(void);

bool wasm_runner_load_buffer(uint8_t *buf, uint32_t size);

void wasm_runner_start(void);

void wasm_runner_stop(void);

void wasm_runner_unload(void);

bool wasm_runner_exited(void);

#endif
