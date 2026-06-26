#include "platform_api_vmcore.h"
#include "platform_api_extension.h"

int
bh_platform_init(void)
{
    return 0;
}

void
bh_platform_destroy(void)
{}

void *
os_malloc(unsigned size)
{
    return malloc(size);
}

void *
os_realloc(void *ptr, unsigned size)
{
    return realloc(ptr, size);
}

void
os_free(void *ptr)
{
    free(ptr);
}

int
os_dumps_proc_mem_info(char *out, unsigned int size)
{
    return -1;
}

int
os_printf(const char *format, ...)
{
    int ret;
    va_list ap;
    va_start(ap, format);
    ret = vprintf(format, ap);
    va_end(ap);
    return ret;
}

int
os_vprintf(const char *format, va_list ap)
{
    return vprintf(format, ap);
}

int
os_usleep(uint32 usec)
{
    vTaskDelay(pdMS_TO_TICKS(usec / 1000));
    return 0;
}

int
os_getpagesize(void)
{
    return 4096;
}

void *
os_mmap(void *hint, size_t size, int prot, int flags, os_file_handle file)
{
    (void)hint;
    (void)prot;
    (void)flags;
    (void)file;
    if (size > 0)
        return malloc(size);
    return NULL;
}

void
os_munmap(void *addr, size_t size)
{
    (void)size;
    free(addr);
}

int
os_mprotect(void *addr, size_t size, int prot)
{
    (void)addr;
    (void)size;
    (void)prot;
    return 0;
}

void
os_dcache_flush(void)
{}

void
os_icache_flush(void *start, size_t len)
{
    (void)start;
    (void)len;
}

void *
os_mremap(void *old_addr, size_t old_size, size_t new_size)
{
    void *new_addr = BH_MALLOC(new_size);
    if (new_addr) {
        memcpy(new_addr, old_addr, old_size < new_size ? old_size : new_size);
        BH_FREE(old_addr);
    }
    return new_addr;
}
