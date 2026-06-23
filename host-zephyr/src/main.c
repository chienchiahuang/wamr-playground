#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "wasm_export.h"
#include "wasm_hello.h"

#define WASM_STACK_SIZE 4096
#define WASM_HEAP_SIZE  4096
#define IWASM_THREAD_STACK 4096

static char global_heap_buf[WASM_GLOBAL_HEAP_SIZE];

bool
wasm_application_execute_main(wasm_module_inst_t module_inst, int argc,
                              char *argv[]);

static void
iwasm_main(void *p1, void *p2, void *p3)
{
    (void)p1; (void)p2; (void)p3;

    uint32_t start = k_uptime_get_32();

    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(init_args));
    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf = global_heap_buf;
    init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);

    if (!wasm_runtime_full_init(&init_args)) {
        printk("WAMR init failed\n");
        return;
    }

    char error_buf[64];
    wasm_module_t module = wasm_runtime_load(
        (uint8_t *)wasm_hello, wasm_hello_len, error_buf, sizeof(error_buf));
    if (!module) {
        printk("Load failed: %s\n", error_buf);
        goto fail1;
    }

    wasm_module_inst_t inst = wasm_runtime_instantiate(
        module, WASM_STACK_SIZE, WASM_HEAP_SIZE, error_buf, sizeof(error_buf));
    if (!inst) {
        printk("Instantiate failed: %s\n", error_buf);
        goto fail2;
    }

    if (wasm_runtime_lookup_function(inst, "main")
        || wasm_runtime_lookup_function(inst, "__main_argc_argv")) {
        wasm_application_execute_main(inst, 0, NULL);
    }
    else {
        printk("No main function found\n");
    }

    const char *exception = wasm_runtime_get_exception(inst);
    if (exception)
        printk("Exception: %s\n", exception);

    wasm_runtime_deinstantiate(inst);

fail2:
    wasm_runtime_unload(module);
fail1:
    wasm_runtime_destroy();

    uint32_t elapsed = k_uptime_get_32() - start;
    printk("elapsed: %u ms\n", elapsed);
}

K_THREAD_STACK_DEFINE(iwasm_stack, IWASM_THREAD_STACK);
static struct k_thread iwasm_thread;

int main(void)
{
    printk("--- WAMR on Zephyr ---\n");
    k_thread_create(&iwasm_thread, iwasm_stack, IWASM_THREAD_STACK,
                    iwasm_main, NULL, NULL, NULL, 5, 0, K_NO_WAIT);
    return 0;
}
