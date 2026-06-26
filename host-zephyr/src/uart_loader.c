#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/crc.h>
#include "uart_loader.h"

#define UART_RING_BUF_SIZE 256

static const uint8_t MAGIC[4] = { 'W', 'A', 'S', 'M' };

typedef enum {
    RX_IDLE,
    RX_HEADER,
    RX_PAYLOAD,
    RX_CRC,
} rx_state_t;

static const struct device *uart_dev;
static uint8_t ring_buf_data[UART_RING_BUF_SIZE];
static struct ring_buf rx_ring;

static rx_state_t rx_state;
static uint8_t magic_window[4];
static uint8_t header_buf[6];
static uint32_t header_pos;
static uint32_t payload_pos;
static uint32_t expected_size;
static uint8_t expected_cmd;
static uint8_t crc_buf[4];
static uint32_t crc_pos;

static void
uart_isr_callback(const struct device *dev, void *user_data)
{
    (void)user_data;

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            uint8_t buf[32];
            int len = uart_fifo_read(dev, buf, sizeof(buf));
            if (len > 0)
                ring_buf_put(&rx_ring, buf, len);
        }
    }
}

int
uart_loader_init(void)
{
    uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (!device_is_ready(uart_dev)) {
        printk("[loader] UART device not ready\n");
        return -1;
    }

    ring_buf_init(&rx_ring, sizeof(ring_buf_data), ring_buf_data);

    uart_irq_callback_set(uart_dev, uart_isr_callback);
    uart_irq_rx_enable(uart_dev);

    rx_state = RX_IDLE;
    memset(magic_window, 0, sizeof(magic_window));

    return 0;
}

void
uart_loader_respond(const char *msg)
{
    while (*msg) {
        uart_poll_out(uart_dev, *msg++);
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
                printk("[loader] size %u exceeds buffer %u\n",
                       expected_size, buf_size);
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
                printk("[loader] CRC mismatch: got 0x%08x, expected 0x%08x\n",
                       received_crc, computed_crc);
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
    int64_t deadline = k_uptime_get() + timeout_ms;
    int result = LOADER_IDLE;

    while (k_uptime_get() < deadline) {
        uint8_t b;
        if (ring_buf_get(&rx_ring, &b, 1) == 1) {
            result = process_byte(b, buf, buf_size, out_size);
            if (result != LOADER_IDLE && result != LOADER_RECEIVING)
                return result;
        } else {
            k_sleep(K_MSEC(1));
        }
    }

    return result;
}
