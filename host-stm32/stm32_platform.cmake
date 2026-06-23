# Custom platform configuration for STM32 bare metal
# Points WAMR to our platform_internal.h and platform source

set(PLATFORM_SHARED_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
set(PLATFORM_SHARED_SOURCE
    ${PLATFORM_SHARED_DIR}/platform_stm32.c
)

include_directories(
    ${PLATFORM_SHARED_DIR}
    ${WAMR_ROOT_DIR}/core/shared/platform/include
)
