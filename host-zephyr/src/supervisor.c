#include <zephyr/kernel.h>
#include "supervisor.h"
#include "wasm_runner.h"
#include "uart_loader.h"

#define SUPERVISOR_STACK_SIZE 1536

#ifndef WASM_OTA_BUF_SIZE
#define WASM_OTA_BUF_SIZE 8192
#endif

static uint8_t wasm_buf[WASM_OTA_BUF_SIZE];

static K_THREAD_STACK_DEFINE(supervisor_stack, SUPERVISOR_STACK_SIZE);
static struct k_thread supervisor_thread;

static void
supervisor_entry(void *p1, void *p2, void *p3)
{
    (void)p1; (void)p2; (void)p3;

    wasm_runner_init();

    if (uart_loader_init() != 0) {
        printk("[supervisor] UART init failed\n");
        return;
    }

    printk("[supervisor] loading default wasm\n");
    if (!wasm_runner_load_embedded()) {
        printk("[supervisor] failed to load default wasm\n");
        return;
    }
    wasm_runner_start();
    printk("[supervisor] wasm running, waiting for OTA uploads...\n");

    while (1) {
        uint32_t wasm_size = 0;
        int result = uart_loader_poll(wasm_buf, sizeof(wasm_buf),
                                      &wasm_size, 100);

        switch (result) {
        case LOADER_COMPLETE:
            printk("[supervisor] received %u bytes, swapping wasm\n",
                   wasm_size);

            wasm_runner_stop();
            wasm_runner_unload();

            if (wasm_runner_load_buffer(wasm_buf, wasm_size)) {
                wasm_runner_start();
                uart_loader_respond("OK\n");
                printk("[supervisor] swap complete\n");
            } else {
                uart_loader_respond("ERR:LOAD\n");
                printk("[supervisor] load failed, falling back to default\n");
                if (wasm_runner_load_embedded()) {
                    wasm_runner_start();
                } else {
                    printk("[supervisor] fallback also failed!\n");
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
            printk("[supervisor] wasm exited, restarting\n");
            wasm_runner_start();
        }
    }
}

void
supervisor_start(void)
{
    k_thread_create(&supervisor_thread, supervisor_stack,
                    SUPERVISOR_STACK_SIZE, supervisor_entry,
                    NULL, NULL, NULL, 3, 0, K_NO_WAIT);
    k_thread_name_set(&supervisor_thread, "supervisor");
}
