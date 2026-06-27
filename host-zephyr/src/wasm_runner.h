#ifndef WASM_RUNNER_H
#define WASM_RUNNER_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_MODULES 3

void wasm_runner_init(void);

bool wasm_runner_execute(uint8_t *buf, uint32_t size, const char *name);

#endif
