# Platform configuration for STM32 + FreeRTOS
# Provides platform_freertos.c (init, printf, memory, mmap) and
# uses WAMR's built-in FreeRTOS thread/time support

set(PLATFORM_SHARED_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

set(WAMR_FREERTOS_DIR ${WAMR_ROOT_DIR}/core/shared/platform/common/freertos)

set(PLATFORM_SHARED_SOURCE
    ${PLATFORM_SHARED_DIR}/platform_freertos.c
    ${WAMR_FREERTOS_DIR}/freertos_thread.c
    ${WAMR_FREERTOS_DIR}/freertos_time.c
)

include_directories(
    ${PLATFORM_SHARED_DIR}
    ${WAMR_ROOT_DIR}/core/shared/platform/include
)
