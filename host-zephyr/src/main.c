#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "wasm_export.h"
#include "supervisor.h"

static char global_heap_buf[WASM_GLOBAL_HEAP_SIZE];

int main(void)
{
    printk("--- WAMR OTA Runtime ---\n");

    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(init_args));
    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf = global_heap_buf;
    init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);

    if (!wasm_runtime_full_init(&init_args)) {
        printk("WAMR init failed\n");
        return 1;
    }

    supervisor_start();
    return 0;
}
