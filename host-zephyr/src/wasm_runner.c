#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "wasm_export.h"
#include "wasm_runner.h"

#define WASM_STACK_SIZE 4096
#define WASM_HEAP_SIZE  4096

bool
wasm_application_execute_main(wasm_module_inst_t module_inst, int32_t argc,
                              char *argv[]);

void
wasm_runner_init(void)
{
}

bool
wasm_runner_execute(uint8_t *buf, uint32_t size, const char *name)
{
    char error_buf[64];

    wasm_module_t module = wasm_runtime_load(
        buf, size, error_buf, sizeof(error_buf));
    if (!module) {
        printk("[runner] load %s failed: %s\n", name, error_buf);
        return false;
    }

    wasm_module_inst_t inst = wasm_runtime_instantiate(
        module, WASM_STACK_SIZE, WASM_HEAP_SIZE,
        error_buf, sizeof(error_buf));
    if (!inst) {
        printk("[runner] instantiate %s failed: %s\n", name, error_buf);
        wasm_runtime_unload(module);
        return false;
    }

    wasm_application_execute_main(inst, 0, NULL);

    const char *exc = wasm_runtime_get_exception(inst);
    if (exc) {
        printk("[runner] %s exception: %s\n", name, exc);
        wasm_runtime_clear_exception(inst);
    }

    wasm_runtime_deinstantiate(inst);
    wasm_runtime_unload(module);

    return true;
}
