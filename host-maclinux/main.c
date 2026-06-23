#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wasm_export.h"

static char global_heap_buf[512 * 1024];

static void
read_wasm_file(const char *path, uint8_t **buf, uint32_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    *size = (uint32_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    *buf = malloc(*size);
    if (!*buf || fread(*buf, 1, *size, f) != *size) {
        fprintf(stderr, "Failed to read %s\n", path);
        exit(1);
    }
    fclose(f);
}

int main(int argc, char *argv[])
{
    const char *wasm_path = argc > 1 ? argv[1] : "hello.wasm";

    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(init_args));
    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf = global_heap_buf;
    init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);

    if (!wasm_runtime_full_init(&init_args)) {
        fprintf(stderr, "wasm_runtime_full_init failed\n");
        return 1;
    }

    uint8_t *wasm_buf = NULL;
    uint32_t wasm_size = 0;
    read_wasm_file(wasm_path, &wasm_buf, &wasm_size);

    char error_buf[128];
    wasm_module_t module = wasm_runtime_load(wasm_buf, wasm_size,
                                             error_buf, sizeof(error_buf));
    if (!module) {
        fprintf(stderr, "wasm_runtime_load failed: %s\n", error_buf);
        goto fail1;
    }

    wasm_module_inst_t inst = wasm_runtime_instantiate(module,
                                                       16 * 1024,  /* stack */
                                                       16 * 1024,  /* heap */
                                                       error_buf,
                                                       sizeof(error_buf));
    if (!inst) {
        fprintf(stderr, "wasm_runtime_instantiate failed: %s\n", error_buf);
        goto fail2;
    }

    if (!wasm_application_execute_main(inst, 0, NULL)) {
        fprintf(stderr, "wasm execution failed: %s\n",
                wasm_runtime_get_exception(inst));
        goto fail3;
    }

    printf("Wasm app executed successfully.\n");

fail3:
    wasm_runtime_deinstantiate(inst);
fail2:
    wasm_runtime_unload(module);
fail1:
    free(wasm_buf);
    wasm_runtime_destroy();
    return 0;
}
