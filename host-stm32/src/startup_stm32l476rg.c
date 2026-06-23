#include <stdint.h>
#include <string.h>

extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata;
extern uint32_t _sbss, _ebss;

extern int main(void);

__attribute__((naked))
void Reset_Handler(void)
{
    /* Enable FPU before any C code that might use VFP instructions */
    __asm volatile(
        "ldr r0, =0xE000ED88  \n"  /* CPACR address */
        "ldr r1, [r0]          \n"
        "orr r1, r1, #(0xF << 20) \n"  /* CP10 + CP11 full access */
        "str r1, [r0]          \n"
        "dsb                   \n"
        "isb                   \n"
        "bl Reset_Handler_Main \n"
    );
}

void Reset_Handler_Main(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata)
        *dst++ = *src++;

    dst = &_sbss;
    while (dst < &_ebss)
        *dst++ = 0;

    main();
    while (1);
}

static void uart_fault_putc(char c)
{
    volatile uint32_t *isr = (volatile uint32_t *)0x4000441C;
    volatile uint32_t *tdr = (volatile uint32_t *)0x40004428;
    while (!(*isr & (1 << 7)));
    *tdr = (uint8_t)c;
}

static void uart_fault_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') uart_fault_putc('\r');
        uart_fault_putc(*s++);
    }
}

static void uart_fault_hex(uint32_t v)
{
    for (int i = 28; i >= 0; i -= 4) {
        int d = (v >> i) & 0xF;
        uart_fault_putc(d < 10 ? '0' + d : 'a' + d - 10);
    }
}

void HardFault_Handler_C(uint32_t *frame)
{
    uart_fault_puts("\n!!! HardFault !!!\n");
    uart_fault_puts("  R0=0x");  uart_fault_hex(frame[0]);
    uart_fault_puts("  R1=0x");  uart_fault_hex(frame[1]);
    uart_fault_puts("\n  R2=0x");  uart_fault_hex(frame[2]);
    uart_fault_puts("  R3=0x");  uart_fault_hex(frame[3]);
    uart_fault_puts("\n  LR=0x");  uart_fault_hex(frame[5]);
    uart_fault_puts("  PC=0x");  uart_fault_hex(frame[6]);
    uart_fault_puts("\n  PSR=0x"); uart_fault_hex(frame[7]);
    uart_fault_puts("\n");
    while (1);
}

__attribute__((naked))
void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4       \n"
        "ite eq            \n"
        "mrseq r0, msp     \n"
        "mrsne r0, psp     \n"
        "b HardFault_Handler_C \n"
    );
}

void Default_Handler(void)
{
    uart_fault_puts("\n!!! Default_Handler !!!\n");
    while (1);
}

/* Minimal vector table — only reset + faults needed */
__attribute__((section(".isr_vector"), used))
void (*const vector_table[])(void) = {
    (void (*)(void))&_estack,  /* Initial SP */
    Reset_Handler,             /* Reset */
    Default_Handler,           /* NMI */
    HardFault_Handler,         /* HardFault */
    Default_Handler,           /* MemManage */
    Default_Handler,           /* BusFault */
    Default_Handler,           /* UsageFault */
    0, 0, 0, 0,               /* Reserved */
    Default_Handler,           /* SVCall */
    Default_Handler,           /* DebugMon */
    0,                         /* Reserved */
    Default_Handler,           /* PendSV */
    Default_Handler,           /* SysTick */
};
