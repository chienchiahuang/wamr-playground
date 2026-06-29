#ifndef WASM_RUNNER_H
#define WASM_RUNNER_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_MODULES 3

void wasm_runner_init(void);

bool wasm_runner_load(int slot, const char *name, uint8_t *buf, uint32_t size);

void wasm_runner_unload(int slot);

bool wasm_runner_run(int slot);

void wasm_runner_run_all(void);

bool wasm_runner_loaded(int slot);

#endif
