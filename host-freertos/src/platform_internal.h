#ifndef _PLATFORM_INTERNAL_H
#define _PLATFORM_INTERNAL_H

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <inttypes.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#ifndef BH_PLATFORM_FREERTOS
#define BH_PLATFORM_FREERTOS
#endif

#define BH_APPLET_PRESERVED_STACK_SIZE (1 * 1024)
#define BH_THREAD_DEFAULT_PRIORITY 4

typedef TaskHandle_t korp_tid;
typedef TaskHandle_t korp_thread;

struct os_thread_data;
typedef struct os_thread_wait_node *os_thread_wait_list;

typedef struct {
    SemaphoreHandle_t sem;
    bool is_recursive;
} korp_mutex;

typedef struct {
    SemaphoreHandle_t wait_list_lock;
    os_thread_wait_list thread_wait_list;
} korp_cond;

typedef struct {
    SemaphoreHandle_t sem;
} korp_sem;

typedef struct {
    int dummy;
} korp_rwlock;

typedef int os_file_handle;
typedef void *os_dir_stream;
typedef int os_raw_file_handle;
typedef int os_poll_file_handle;
typedef unsigned int os_nfds_t;
typedef int os_timespec;

static inline os_file_handle
os_get_invalid_handle(void)
{
    return -1;
}

int os_printf(const char *format, ...);
int os_vprintf(const char *format, va_list ap);

int os_getpagesize(void);

#endif
