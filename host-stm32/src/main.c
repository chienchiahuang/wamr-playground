#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wasm_export.h"

/* UART register addresses for STM32L476RG */
#define RCC_BASE         0x40021000
#define RCC_AHB2ENR      (*(volatile uint32_t *)(RCC_BASE + 0x4C))
#define RCC_APB1ENR1     (*(volatile uint32_t *)(RCC_BASE + 0x58))

#define GPIOA_BASE       0x48000000
#define GPIOA_MODER      (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRL       (*(volatile uint32_t *)(GPIOA_BASE + 0x20))

#define USART2_BASE      0x40004400
#define USART2_CR1       (*(volatile uint32_t *)(USART2_BASE + 0x00))
#define USART2_BRR       (*(volatile uint32_t *)(USART2_BASE + 0x0C))
#define USART2_ISR       (*(volatile uint32_t *)(USART2_BASE + 0x1C))
#define USART2_TDR       (*(volatile uint32_t *)(USART2_BASE + 0x28))

#define SYS_CLK          4000000
#define BAUD_RATE         115200

static void uart_init(void)
{
    RCC_AHB2ENR  |= (1 << 0);
    RCC_APB1ENR1 |= (1 << 17);

    GPIOA_MODER &= ~((3 << (2 * 2)) | (3 << (3 * 2)));
    GPIOA_MODER |=  ((2 << (2 * 2)) | (2 << (3 * 2)));

    GPIOA_AFRL  &= ~((0xF << (2 * 4)) | (0xF << (3 * 4)));
    GPIOA_AFRL  |=  ((7   << (2 * 4)) | (7   << (3 * 4)));

    USART2_BRR = SYS_CLK / BAUD_RATE;
    USART2_CR1 = (1 << 0) | (1 << 3);
}

static void uart_putc(char c)
{
    while (!(USART2_ISR & (1 << 7)))
        ;
    USART2_TDR = (uint8_t)c;
}

int _write(int fd, const char *buf, int len)
{
    (void)fd;
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n')
            uart_putc('\r');
        uart_putc(buf[i]);
    }
    return len;
}

extern char end;
static char *heap_ptr = 0;

void *_sbrk(int incr)
{
    char *prev;
    if (heap_ptr == 0)
        heap_ptr = &end;
    prev = heap_ptr;
    heap_ptr += incr;
    return (void *)prev;
}

extern const uint8_t wasm_hello_bin[];
extern const uint32_t wasm_hello_bin_len;

/* WAMR pool in SRAM2 — keeps SRAM1 free for wasm linear memory */
static char wamr_heap[24 * 1024] __attribute__((section(".sram2")));

int main(void)
{
    uart_init();
    printf("\n--- WAMR on STM32L476RG ---\n");

    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(init_args));
    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf = wamr_heap;
    init_args.mem_alloc_option.pool.heap_size = sizeof(wamr_heap);

    if (!wasm_runtime_full_init(&init_args)) {
        printf("ERROR: wasm_runtime_full_init failed\n");
        while (1);
    }

    /* WAMR modifies the wasm buffer in-place — copy from flash to RAM */
    uint8_t *wasm_buf = malloc(wasm_hello_bin_len);
    if (!wasm_buf) {
        printf("ERROR: malloc wasm buf failed\n");
        while (1);
    }
    memcpy(wasm_buf, wasm_hello_bin, wasm_hello_bin_len);

    char error_buf[64];
    wasm_module_t module = wasm_runtime_load(wasm_buf, wasm_hello_bin_len,
                                             error_buf, sizeof(error_buf));
    if (!module) {
        printf("ERROR: load failed: %s\n", error_buf);
        while (1);
    }

    wasm_module_inst_t inst = wasm_runtime_instantiate(module,
                                                       4 * 1024,
                                                       8 * 1024,
                                                       error_buf,
                                                       sizeof(error_buf));
    if (!inst) {
        printf("ERROR: instantiate failed: %s\n", error_buf);
        while (1);
    }

    if (!wasm_application_execute_main(inst, 0, NULL)) {
        printf("ERROR: exec failed: %s\n", wasm_runtime_get_exception(inst));
    }
    else {
        printf("Wasm executed OK!\n");
    }

    wasm_runtime_deinstantiate(inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();

    printf("--- Done ---\n");
    while (1);
    return 0;
}
