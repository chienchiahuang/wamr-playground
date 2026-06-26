#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "wasm_export.h"
#include "wasm_runner.h"
#include "wasm_hello.h"

#ifndef WASM_STACK_SIZE
#define WASM_STACK_SIZE 4096
#endif
#ifndef WASM_HEAP_SIZE
#define WASM_HEAP_SIZE  4096
#endif
#define RUNNER_THREAD_STACK 4096
#define RUNNER_STOP_TIMEOUT_MS 5000

bool
wasm_application_execute_main(wasm_module_inst_t module_inst, int argc,
                              char *argv[]);

static wasm_module_t current_module;
static wasm_module_inst_t current_inst;

static struct k_thread runner_thread;
static K_THREAD_STACK_DEFINE(runner_stack, RUNNER_THREAD_STACK);
static struct k_sem runner_done;
static volatile bool runner_active;

void
wasm_runner_init(void)
{
    k_sem_init(&runner_done, 0, 1);
    current_module = NULL;
    current_inst = NULL;
    runner_active = false;
}

static void
runner_entry(void *p1, void *p2, void *p3)
{
    (void)p1; (void)p2; (void)p3;

    runner_active = true;
    uint32_t start = k_uptime_get_32();

    if (wasm_runtime_lookup_function(current_inst, "main")
        || wasm_runtime_lookup_function(current_inst, "__main_argc_argv")) {
        wasm_application_execute_main(current_inst, 0, NULL);
    } else {
        printk("[runner] no main function found\n");
    }

    const char *exc = wasm_runtime_get_exception(current_inst);
    if (exc)
        printk("[runner] exception: %s\n", exc);

    uint32_t elapsed = k_uptime_get_32() - start;
    printk("[runner] elapsed: %u ms\n", elapsed);

    runner_active = false;
    k_sem_give(&runner_done);
}

bool
wasm_runner_load_embedded(void)
{
    wasm_runner_unload();

    char error_buf[64];
    current_module = wasm_runtime_load(
        (uint8_t *)wasm_hello, wasm_hello_len, error_buf, sizeof(error_buf));
    if (!current_module) {
        printk("[runner] load embedded failed: %s\n", error_buf);
        return false;
    }

    current_inst = wasm_runtime_instantiate(
        current_module, WASM_STACK_SIZE, WASM_HEAP_SIZE,
        error_buf, sizeof(error_buf));
    if (!current_inst) {
        printk("[runner] instantiate embedded failed: %s\n", error_buf);
        wasm_runtime_unload(current_module);
        current_module = NULL;
        return false;
    }

    return true;
}

bool
wasm_runner_load_buffer(uint8_t *buf, uint32_t size)
{
    wasm_runner_unload();

    char error_buf[64];
    current_module = wasm_runtime_load(
        buf, size, error_buf, sizeof(error_buf));
    if (!current_module) {
        printk("[runner] load failed: %s\n", error_buf);
        return false;
    }

    current_inst = wasm_runtime_instantiate(
        current_module, WASM_STACK_SIZE, WASM_HEAP_SIZE,
        error_buf, sizeof(error_buf));
    if (!current_inst) {
        printk("[runner] instantiate failed: %s\n", error_buf);
        wasm_runtime_unload(current_module);
        current_module = NULL;
        return false;
    }

    return true;
}

void
wasm_runner_start(void)
{
    k_sem_reset(&runner_done);
    k_thread_create(&runner_thread, runner_stack, RUNNER_THREAD_STACK,
                    runner_entry, NULL, NULL, NULL, 5, 0, K_NO_WAIT);
    k_thread_name_set(&runner_thread, "wasm_runner");
}

void
wasm_runner_stop(void)
{
    if (!runner_active)
        return;

    wasm_runtime_terminate(current_inst);

    if (k_sem_take(&runner_done, K_MSEC(RUNNER_STOP_TIMEOUT_MS)) != 0) {
        printk("[runner] stop timeout, aborting thread\n");
        k_thread_abort(&runner_thread);
        runner_active = false;
    }
}

void
wasm_runner_unload(void)
{
    if (current_inst) {
        wasm_runtime_deinstantiate(current_inst);
        current_inst = NULL;
    }
    if (current_module) {
        wasm_runtime_unload(current_module);
        current_module = NULL;
    }
}

bool
wasm_runner_exited(void)
{
    return !runner_active && current_inst != NULL;
}
