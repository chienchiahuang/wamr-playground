#include <stdint.h>
#include <string.h>

extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata;
extern uint32_t _sbss, _ebss;

extern int main(void);

/* FreeRTOS handlers — defined via FreeRTOSConfig.h macros */
extern void SVC_Handler(void);
extern void PendSV_Handler(void);
extern void SysTick_Handler(void);
extern void USART2_IRQHandler(void);

__attribute__((naked))
void Reset_Handler(void)
{
    __asm volatile(
        "ldr r0, =0xE000ED88  \n"
        "ldr r1, [r0]          \n"
        "orr r1, r1, #(0xF << 20) \n"
        "str r1, [r0]          \n"
        "dsb                   \n"
        "isb                   \n"
        "bl Reset_Handler_Main \n"
    );
}

/* RCC registers */
#define RCC_CR           (*(volatile uint32_t *)0x40021000)
#define RCC_CFGR         (*(volatile uint32_t *)0x40021008)
#define RCC_PLLCFGR      (*(volatile uint32_t *)0x4002100C)
#define FLASH_ACR        (*(volatile uint32_t *)0x40022000)

static void clock_init_80mhz(void)
{
    /* MSI is already running at 4 MHz (default after reset).
     * Configure PLL: MSI (4 MHz) → PLL → 80 MHz SYSCLK
     * PLLM=/1, PLLN=×40, PLLR=/2 → 4 × 40 / 2 = 80 MHz */

    /* Set flash wait states for 80 MHz (4 WS) */
    FLASH_ACR = (FLASH_ACR & ~0x7) | 4;
    while ((FLASH_ACR & 0x7) != 4)
        ;

    /* Configure PLL: source=MSI, M=1, N=40, R=2 */
    RCC_CR &= ~(1 << 24);           /* PLL OFF */
    while (RCC_CR & (1 << 25))      /* Wait PLL unlocked */
        ;

    RCC_PLLCFGR = (1 << 0)          /* PLLSRC = MSI */
                | (0 << 4)          /* PLLM = /1 (0 = div1) */
                | (40 << 8)         /* PLLN = ×40 */
                | (0 << 25)         /* PLLR = /2 (0 = div2) */
                | (1 << 24);        /* PLLREN = 1 (enable PLLR output) */

    RCC_CR |= (1 << 24);            /* PLL ON */
    while (!(RCC_CR & (1 << 25)))    /* Wait PLL locked */
        ;

    /* Switch SYSCLK to PLL */
    RCC_CFGR = (RCC_CFGR & ~0x3) | 0x3;  /* SW = PLL */
    while ((RCC_CFGR & 0xC) != 0xC)      /* Wait SWS = PLL */
        ;
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

    clock_init_80mhz();

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

__attribute__((section(".isr_vector"), used))
void (*const vector_table[])(void) = {
    (void (*)(void))&_estack,  /*  0: Initial SP */
    Reset_Handler,             /*  1: Reset */
    Default_Handler,           /*  2: NMI */
    HardFault_Handler,         /*  3: HardFault */
    Default_Handler,           /*  4: MemManage */
    Default_Handler,           /*  5: BusFault */
    Default_Handler,           /*  6: UsageFault */
    0, 0, 0, 0,               /*  7-10: Reserved */
    SVC_Handler,               /* 11: SVCall — FreeRTOS */
    Default_Handler,           /* 12: DebugMon */
    0,                         /* 13: Reserved */
    PendSV_Handler,            /* 14: PendSV — FreeRTOS */
    SysTick_Handler,           /* 15: SysTick — FreeRTOS */
    /* IRQ 0-37: unused peripherals */
    [16 ... 53] = Default_Handler,
    /* IRQ 38: USART2 */
    [54] = USART2_IRQHandler,
};
