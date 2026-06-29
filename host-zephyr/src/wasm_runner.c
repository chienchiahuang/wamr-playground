#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "wasm_export.h"
#include "wasm_runner.h"

#define WASM_STACK_SIZE 2048
#define WASM_HEAP_SIZE  1024

bool
wasm_application_execute_main(wasm_module_inst_t module_inst, int32_t argc,
                              char *argv[]);

static const char *slot_names[MAX_MODULES] = { "sensor", "actuator", "alert" };

static struct {
    wasm_module_t module;
    wasm_module_inst_t inst;
} modules[MAX_MODULES];

void
wasm_runner_init(void)
{
    memset(modules, 0, sizeof(modules));
}

bool
wasm_runner_load(int slot, const char *name, uint8_t *buf, uint32_t size)
{
    if (slot < 0 || slot >= MAX_MODULES)
        return false;

    wasm_runner_unload(slot);

    char error_buf[64];
    LoadArgs load_args = { 0 };
    load_args.wasm_binary_freeable = true;

    modules[slot].module = wasm_runtime_load_ex(
        buf, size, &load_args, error_buf, sizeof(error_buf));
    if (!modules[slot].module) {
        printk("[runner] load %s failed: %s\n", name, error_buf);
        return false;
    }

    modules[slot].inst = wasm_runtime_instantiate(
        modules[slot].module, WASM_STACK_SIZE, WASM_HEAP_SIZE,
        error_buf, sizeof(error_buf));
    if (!modules[slot].inst) {
        printk("[runner] instantiate %s failed: %s\n", name, error_buf);
        wasm_runtime_unload(modules[slot].module);
        modules[slot].module = NULL;
        return false;
    }

    printk("[runner] %s loaded and ready\n", name);
    return true;
}

void
wasm_runner_unload(int slot)
{
    if (slot < 0 || slot >= MAX_MODULES)
        return;
    if (modules[slot].inst) {
        wasm_runtime_deinstantiate(modules[slot].inst);
        modules[slot].inst = NULL;
    }
    if (modules[slot].module) {
        wasm_runtime_unload(modules[slot].module);
        modules[slot].module = NULL;
    }
}

bool
wasm_runner_run(int slot)
{
    if (slot < 0 || slot >= MAX_MODULES || !modules[slot].inst)
        return false;

    wasm_application_execute_main(modules[slot].inst, 0, NULL);

    const char *exc = wasm_runtime_get_exception(modules[slot].inst);
    if (exc) {
        printk("[runner] %s exception: %s\n", slot_names[slot], exc);
        wasm_runtime_clear_exception(modules[slot].inst);
        return false;
    }
    return true;
}

void
wasm_runner_run_all(void)
{
    for (int i = 0; i < MAX_MODULES; i++) {
        if (modules[i].inst)
            wasm_runner_run(i);
    }
}

bool
wasm_runner_loaded(int slot)
{
    if (slot < 0 || slot >= MAX_MODULES)
        return false;
    return modules[slot].inst != NULL;
}
