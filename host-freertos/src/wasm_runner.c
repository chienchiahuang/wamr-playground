#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "wasm_export.h"
#include "wasm_runner.h"

#define WASM_STACK_SIZE 4096
#define WASM_HEAP_SIZE  4096
#define RUNNER_TASK_STACK_WORDS 1024
#define RUNNER_STOP_TIMEOUT_MS 5000

extern const uint8_t wasm_hello_bin[];
extern const uint32_t wasm_hello_bin_len;

bool
wasm_application_execute_main(wasm_module_inst_t module_inst, int32_t argc,
                              char *argv[]);

static wasm_module_t current_module;
static wasm_module_inst_t current_inst;
static uint8_t *embedded_copy;

static TaskHandle_t runner_task_handle;
static SemaphoreHandle_t runner_done;
static volatile bool runner_active;

void
wasm_runner_init(void)
{
    runner_done = xSemaphoreCreateBinary();
    current_module = NULL;
    current_inst = NULL;
    runner_task_handle = NULL;
    runner_active = false;
    embedded_copy = NULL;
}

static void
runner_entry(void *param)
{
    (void)param;

    runner_active = true;
    TickType_t start = xTaskGetTickCount();

    if (wasm_runtime_lookup_function(current_inst, "main")
        || wasm_runtime_lookup_function(current_inst, "__main_argc_argv")) {
        wasm_application_execute_main(current_inst, 0, NULL);
    } else {
        printf("[runner] no main function found\n");
    }

    const char *exc = wasm_runtime_get_exception(current_inst);
    if (exc)
        printf("[runner] exception: %s\n", exc);

    TickType_t elapsed = xTaskGetTickCount() - start;
    printf("[runner] elapsed: %lu ms\n", (unsigned long)elapsed);

    runner_active = false;
    xSemaphoreGive(runner_done);
    vTaskDelete(NULL);
}

bool
wasm_runner_load_embedded(void)
{
    wasm_runner_unload();

    embedded_copy = malloc(wasm_hello_bin_len);
    if (!embedded_copy) {
        printf("[runner] malloc embedded copy failed\n");
        return false;
    }
    memcpy(embedded_copy, wasm_hello_bin, wasm_hello_bin_len);

    char error_buf[64];
    current_module = wasm_runtime_load(
        embedded_copy, wasm_hello_bin_len, error_buf, sizeof(error_buf));
    if (!current_module) {
        printf("[runner] load embedded failed: %s\n", error_buf);
        free(embedded_copy);
        embedded_copy = NULL;
        return false;
    }

    current_inst = wasm_runtime_instantiate(
        current_module, WASM_STACK_SIZE, WASM_HEAP_SIZE,
        error_buf, sizeof(error_buf));
    if (!current_inst) {
        printf("[runner] instantiate embedded failed: %s\n", error_buf);
        wasm_runtime_unload(current_module);
        current_module = NULL;
        free(embedded_copy);
        embedded_copy = NULL;
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
        printf("[runner] load failed: %s\n", error_buf);
        return false;
    }

    current_inst = wasm_runtime_instantiate(
        current_module, WASM_STACK_SIZE, WASM_HEAP_SIZE,
        error_buf, sizeof(error_buf));
    if (!current_inst) {
        printf("[runner] instantiate failed: %s\n", error_buf);
        wasm_runtime_unload(current_module);
        current_module = NULL;
        return false;
    }

    return true;
}

void
wasm_runner_start(void)
{
    xSemaphoreTake(runner_done, 0);
    xTaskCreate(runner_entry, "wasm_run", RUNNER_TASK_STACK_WORDS,
                NULL, 2, &runner_task_handle);
}

void
wasm_runner_stop(void)
{
    if (!runner_active)
        return;

    wasm_runtime_terminate(current_inst);

    if (xSemaphoreTake(runner_done, pdMS_TO_TICKS(RUNNER_STOP_TIMEOUT_MS)) != pdPASS) {
        printf("[runner] stop timeout, deleting task\n");
        if (runner_task_handle) {
            vTaskDelete(runner_task_handle);
            runner_task_handle = NULL;
        }
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
    if (embedded_copy) {
        free(embedded_copy);
        embedded_copy = NULL;
    }
}

bool
wasm_runner_exited(void)
{
    return !runner_active && current_inst != NULL;
}
