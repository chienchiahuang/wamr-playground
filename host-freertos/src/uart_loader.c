#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "uart_loader.h"

/* STM32L476 USART2 registers */
#define USART2_BASE      0x40004400
#define USART2_CR1       (*(volatile uint32_t *)(USART2_BASE + 0x00))
#define USART2_ISR       (*(volatile uint32_t *)(USART2_BASE + 0x1C))
#define USART2_RDR       (*(volatile uint32_t *)(USART2_BASE + 0x24))
#define USART2_TDR       (*(volatile uint32_t *)(USART2_BASE + 0x28))

/* NVIC for USART2 (IRQ 38) */
#define NVIC_ISER1       (*(volatile uint32_t *)0xE000E104)
#define NVIC_IPR38       (*(volatile uint8_t *)(0xE000E400 + 38))
#define NVIC_USART2_BIT  (1 << (38 - 32))

/* CRC-32 (IEEE) */
static uint32_t crc32_ieee(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

static const uint8_t MAGIC[4] = { 'W', 'A', 'S', 'M' };

typedef enum {
    RX_IDLE,
    RX_HEADER,
    RX_PAYLOAD,
    RX_CRC,
} rx_state_t;

static StreamBufferHandle_t rx_stream;

static rx_state_t rx_state;
static uint8_t magic_window[4];
static uint8_t header_buf[6];
static uint32_t header_pos;
static uint32_t payload_pos;
static uint32_t expected_size;
static uint8_t expected_cmd;
static uint8_t crc_buf[4];
static uint32_t crc_pos;

void USART2_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    while (USART2_ISR & (1 << 5)) {
        uint8_t b = (uint8_t)USART2_RDR;
        xStreamBufferSendFromISR(rx_stream, &b, 1, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

int
uart_loader_init(void)
{
    rx_stream = xStreamBufferCreate(1024, 1);
    if (!rx_stream) {
        printf("[loader] stream buffer create failed\n");
        return -1;
    }

    USART2_CR1 |= (1 << 2) | (1 << 5);
    NVIC_IPR38 = (6 << 4);
    NVIC_ISER1 |= NVIC_USART2_BIT;

    rx_state = RX_IDLE;
    memset(magic_window, 0, sizeof(magic_window));

    printf("[loader] UART RX ready\n");
    return 0;
}

void
uart_loader_respond(const char *msg)
{
    while (*msg) {
        while (!(USART2_ISR & (1 << 7)))
            ;
        USART2_TDR = (uint8_t)*msg++;
    }
}

static int
process_byte(uint8_t b, uint8_t *buf, uint32_t buf_size, uint32_t *out_size)
{
    switch (rx_state) {
    case RX_IDLE:
        memmove(magic_window, magic_window + 1, 3);
        magic_window[3] = b;
        if (memcmp(magic_window, MAGIC, 4) == 0) {
            rx_state = RX_HEADER;
            header_pos = 0;
            memset(magic_window, 0, sizeof(magic_window));
        }
        break;

    case RX_HEADER:
        header_buf[header_pos++] = b;
        if (header_pos == 6) {
            expected_cmd = header_buf[0];
            expected_size = header_buf[1]
                          | (header_buf[2] << 8)
                          | (header_buf[3] << 16)
                          | (header_buf[4] << 24);

            if (expected_cmd == CMD_STATUS) {
                rx_state = RX_IDLE;
                *out_size = 0;
                return LOADER_STATUS;
            }

            if (expected_size == 0 || expected_size > buf_size) {
                printf("[loader] size %lu exceeds buffer %lu\n",
                       (unsigned long)expected_size, (unsigned long)buf_size);
                rx_state = RX_IDLE;
                return LOADER_ERR_SIZE;
            }

            payload_pos = 0;
            rx_state = RX_PAYLOAD;
        }
        break;

    case RX_PAYLOAD:
        buf[payload_pos++] = b;
        if (payload_pos == expected_size) {
            crc_pos = 0;
            rx_state = RX_CRC;
        }
        break;

    case RX_CRC:
        crc_buf[crc_pos++] = b;
        if (crc_pos == 4) {
            uint32_t received_crc = crc_buf[0]
                                  | (crc_buf[1] << 8)
                                  | (crc_buf[2] << 16)
                                  | (crc_buf[3] << 24);
            uint32_t computed_crc = crc32_ieee(buf, expected_size);

            rx_state = RX_IDLE;

            if (received_crc != computed_crc) {
                printf("[loader] CRC mismatch: got 0x%08lx, expected 0x%08lx\n",
                       (unsigned long)received_crc, (unsigned long)computed_crc);
                return LOADER_ERR_CRC;
            }

            *out_size = expected_size;
            return LOADER_COMPLETE;
        }
        break;
    }

    return (rx_state == RX_IDLE) ? LOADER_IDLE : LOADER_RECEIVING;
}

int
uart_loader_poll(uint8_t *buf, uint32_t buf_size,
                 uint32_t *out_size, int32_t timeout_ms)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    int result = LOADER_IDLE;

    while (xTaskGetTickCount() < deadline) {
        uint8_t b;
        if (xStreamBufferReceive(rx_stream, &b, 1, pdMS_TO_TICKS(1)) == 1) {
            result = process_byte(b, buf, buf_size, out_size);
            if (result != LOADER_IDLE && result != LOADER_RECEIVING)
                return result;
        }
    }

    return result;
}
