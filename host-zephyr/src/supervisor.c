#include <zephyr/kernel.h>
#include "supervisor.h"
#include "wasm_runner.h"
#include "uart_loader.h"
#include "wasm_flash.h"

extern unsigned char wasm_hello[];
extern unsigned int wasm_hello_len;

#define SUPERVISOR_STACK_SIZE 2048

#ifndef WASM_OTA_BUF_SIZE
#define WASM_OTA_BUF_SIZE 8192
#endif

static uint8_t wasm_buf[WASM_OTA_BUF_SIZE];

static K_THREAD_STACK_DEFINE(supervisor_stack, SUPERVISOR_STACK_SIZE);
static struct k_thread supervisor_thread;

static bool
load_from_flash(void)
{
    bool a_valid = wasm_flash_valid(WASM_SLOT_A);
    bool b_valid = wasm_flash_valid(WASM_SLOT_B);
    int slot = -1;

    if (b_valid && a_valid) {
        slot = (wasm_flash_version(WASM_SLOT_B) > wasm_flash_version(WASM_SLOT_A))
               ? WASM_SLOT_B : WASM_SLOT_A;
    } else if (b_valid) {
        slot = WASM_SLOT_B;
    } else if (a_valid) {
        slot = WASM_SLOT_A;
    }

    if (slot < 0)
        return false;

    printk("[supervisor] loading from flash slot %c\n", slot == 0 ? 'A' : 'B');
    uint32_t size = wasm_flash_read(slot, wasm_buf, sizeof(wasm_buf));
    if (size == 0)
        return false;

    return wasm_runner_load_buffer(wasm_buf, size);
}

static void
supervisor_entry(void *p1, void *p2, void *p3)
{
    (void)p1; (void)p2; (void)p3;

    wasm_runner_init();

    if (wasm_flash_init() != 0) {
        printk("[supervisor] flash init failed\n");
        return;
    }

    if (uart_loader_init() != 0) {
        printk("[supervisor] UART init failed\n");
        return;
    }

    if (!load_from_flash()) {
        printk("[supervisor] no valid wasm in flash, writing default to slot A\n");
        wasm_flash_write(WASM_SLOT_A, wasm_hello, wasm_hello_len);

        uint32_t size = wasm_flash_read(WASM_SLOT_A, wasm_buf, sizeof(wasm_buf));
        if (size > 0) {
            wasm_runner_load_buffer(wasm_buf, size);
        } else {
            printk("[supervisor] flash readback failed, using embedded\n");
            wasm_runner_load_embedded();
        }
    }

    wasm_runner_start();
    printk("[supervisor] wasm running, waiting for OTA uploads...\n");

    while (1) {
        uint32_t wasm_size = 0;
        int result = uart_loader_poll(wasm_buf, sizeof(wasm_buf),
                                      &wasm_size, 100);

        switch (result) {
        case LOADER_COMPLETE:
            printk("[supervisor] received %u bytes, writing to flash slot B\n",
                   wasm_size);

            if (wasm_flash_write(WASM_SLOT_B, wasm_buf, wasm_size) != 0) {
                uart_loader_respond("ERR:FLASH\n");
                break;
            }

            wasm_runner_stop();
            wasm_runner_unload();

            uint32_t size = wasm_flash_read(WASM_SLOT_B, wasm_buf,
                                            sizeof(wasm_buf));
            if (size > 0 && wasm_runner_load_buffer(wasm_buf, size)) {
                wasm_runner_start();
                uart_loader_respond("OK\n");
                printk("[supervisor] swap complete (persisted to flash)\n");
            } else {
                uart_loader_respond("ERR:LOAD\n");
                printk("[supervisor] load failed, falling back\n");
                if (!load_from_flash()) {
                    wasm_runner_load_embedded();
                }
                wasm_runner_start();
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
