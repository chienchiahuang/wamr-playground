#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "supervisor.h"
#include "wasm_runner.h"
#include "uart_loader.h"

#define SUPERVISOR_TASK_STACK_WORDS 512
#define WASM_BUF_SIZE 4096

static uint8_t wasm_buf[WASM_BUF_SIZE];

static void
supervisor_entry(void *param)
{
    (void)param;

    wasm_runner_init();

    if (uart_loader_init() != 0) {
        printf("[supervisor] UART init failed\n");
        vTaskDelete(NULL);
        return;
    }

    printf("[supervisor] loading default wasm\n");
    if (!wasm_runner_load_embedded()) {
        printf("[supervisor] failed to load default wasm\n");
        vTaskDelete(NULL);
        return;
    }
    wasm_runner_start();
    printf("[supervisor] wasm running, waiting for OTA uploads...\n");

    while (1) {
        uint32_t wasm_size = 0;
        int result = uart_loader_poll(wasm_buf, sizeof(wasm_buf),
                                      &wasm_size, 100);

        switch (result) {
        case LOADER_COMPLETE:
            printf("[supervisor] received %u bytes, swapping wasm\n",
                   (unsigned)wasm_size);

            wasm_runner_stop();
            vTaskDelay(pdMS_TO_TICKS(10));
            wasm_runner_unload();

            if (wasm_runner_load_buffer(wasm_buf, wasm_size)) {
                wasm_runner_start();
                uart_loader_respond("OK\n");
                printf("[supervisor] swap complete\n");
            } else {
                uart_loader_respond("ERR:LOAD\n");
                printf("[supervisor] load failed, falling back to default\n");
                if (wasm_runner_load_embedded()) {
                    wasm_runner_start();
                } else {
                    printf("[supervisor] fallback also failed!\n");
                }
            }
            break;

        case LOADER_ERR_CRC:
            uart_loader_respond("ERR:CRC\n");
            break;

        case LOADER_ERR_SIZE:
            uart_loader_respond("ERR:SIZE\n");
            break;

        case LOADER_STATUS:
            uart_loader_respond("RUNNING\n");
            break;

        default:
            break;
        }

        if (wasm_runner_exited()) {
            printf("[supervisor] wasm exited, restarting\n");
            wasm_runner_start();
        }
    }
}

void
supervisor_start(void)
{
    xTaskCreate(supervisor_entry, "super", SUPERVISOR_TASK_STACK_WORDS,
                NULL, 3, NULL);
}
