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

uint64
os_time_get_boot_us(void)
{
    return 0;
}

uint64
os_time_thread_cputime_us(void)
{
    return 0;
}

int
os_thread_sys_init(void)
{
    return BHT_OK;
}

void
os_thread_sys_destroy(void)
{}

korp_tid
os_self_thread(void)
{
    return 0;
}

int
os_mutex_init(korp_mutex *mutex)
{
    (void)mutex;
    return BHT_OK;
}

int
os_recursive_mutex_init(korp_mutex *mutex)
{
    (void)mutex;
    return BHT_OK;
}

int
os_mutex_destroy(korp_mutex *mutex)
{
    (void)mutex;
    return BHT_OK;
}

int
os_mutex_lock(korp_mutex *mutex)
{
    (void)mutex;
    return BHT_OK;
}

int
os_mutex_unlock(korp_mutex *mutex)
{
    (void)mutex;
    return BHT_OK;
}

int
os_cond_init(korp_cond *cond)
{
    (void)cond;
    return BHT_OK;
}

int
os_cond_destroy(korp_cond *cond)
{
    (void)cond;
    return BHT_OK;
}

int
os_cond_wait(korp_cond *cond, korp_mutex *mutex)
{
    (void)cond;
    (void)mutex;
    return BHT_OK;
}

int
os_cond_reltimedwait(korp_cond *cond, korp_mutex *mutex, uint64 useconds)
{
    (void)cond;
    (void)mutex;
    (void)useconds;
    return BHT_OK;
}

int
os_cond_signal(korp_cond *cond)
{
    (void)cond;
    return BHT_OK;
}

int
os_cond_broadcast(korp_cond *cond)
{
    (void)cond;
    return BHT_OK;
}

int
os_thread_create(korp_tid *p_tid, void *(*start)(void *), void *arg,
                 unsigned int stack_size)
{
    (void)p_tid;
    (void)start;
    (void)arg;
    (void)stack_size;
    return BHT_ERROR;
}

int
os_thread_create_with_prio(korp_tid *p_tid, void *(*start)(void *), void *arg,
                           unsigned int stack_size, int prio)
{
    (void)p_tid;
    (void)start;
    (void)arg;
    (void)stack_size;
    (void)prio;
    return BHT_ERROR;
}

int
os_thread_join(korp_tid tid, void **retval)
{
    (void)tid;
    (void)retval;
    return BHT_OK;
}

int
os_thread_detach(korp_tid tid)
{
    (void)tid;
    return BHT_OK;
}

void
os_thread_exit(void *retval)
{
    (void)retval;
    while (1)
        ;
}

uint8 *
os_thread_get_stack_boundary(void)
{
    return NULL;
}

void
os_thread_jit_write_protect_np(bool enabled)
{
    (void)enabled;
}

int
os_usleep(uint32 usec)
{
    (void)usec;
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
