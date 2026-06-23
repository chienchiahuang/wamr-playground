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

#ifndef BH_PLATFORM_STM32
#define BH_PLATFORM_STM32
#endif

#define BH_APPLET_PRESERVED_STACK_SIZE (1 * 1024)
#define BH_THREAD_DEFAULT_PRIORITY 0

typedef int korp_tid;
typedef int korp_thread;

typedef struct {
    int dummy;
} korp_mutex;

typedef struct {
    int dummy;
} korp_cond;

typedef struct {
    int dummy;
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

#endif /* _PLATFORM_INTERNAL_H */
