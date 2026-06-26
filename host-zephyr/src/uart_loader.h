#ifndef UART_LOADER_H
#define UART_LOADER_H

#include <stdint.h>

#define LOADER_IDLE       0
#define LOADER_RECEIVING  1
#define LOADER_COMPLETE   2
#define LOADER_STATUS     3
#define LOADER_ERR_CRC   -1
#define LOADER_ERR_SIZE  -2

#define CMD_UPLOAD  0x01
#define CMD_STATUS  0x02

int uart_loader_init(void);

int uart_loader_poll(uint8_t *buf, uint32_t buf_size,
                     uint32_t *out_size, int32_t timeout_ms);

void uart_loader_respond(const char *msg);

#endif
