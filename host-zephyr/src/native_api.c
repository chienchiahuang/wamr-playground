#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "wasm_export.h"
#include "native_api.h"

#define NUM_BUTTONS 4
#define NUM_LEDS    4
#define NUM_DATA    16

static const struct gpio_dt_spec buttons[NUM_BUTTONS] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios),
};

static const struct gpio_dt_spec leds[NUM_LEDS] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
};

static int32_t shared_data[NUM_DATA];

static int32_t
native_read_button(wasm_exec_env_t exec_env, int32_t n)
{
    (void)exec_env;
    if (n < 0 || n >= NUM_BUTTONS)
        return -1;
    return gpio_pin_get_dt(&buttons[n]);
}

static void
native_set_led(wasm_exec_env_t exec_env, int32_t n, int32_t on)
{
    (void)exec_env;
    if (n >= 0 && n < NUM_LEDS)
        gpio_pin_set_dt(&leds[n], on ? 1 : 0);
}

static void
native_set_data(wasm_exec_env_t exec_env, int32_t key, int32_t value)
{
    (void)exec_env;
    if (key >= 0 && key < NUM_DATA)
        shared_data[key] = value;
}

static int32_t
native_get_data(wasm_exec_env_t exec_env, int32_t key)
{
    (void)exec_env;
    if (key < 0 || key >= NUM_DATA)
        return 0;
    return shared_data[key];
}

static NativeSymbol native_symbols[] = {
    { "read_button", native_read_button, "(i)i", NULL },
    { "set_led",     native_set_led,     "(ii)", NULL },
    { "set_data",    native_set_data,    "(ii)", NULL },
    { "get_data",    native_get_data,    "(i)i", NULL },
};

int
native_api_init(void)
{
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (!gpio_is_ready_dt(&buttons[i])) {
            printk("[native] button %d not ready\n", i);
            return -1;
        }
        gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
    }

    for (int i = 0; i < NUM_LEDS; i++) {
        if (!gpio_is_ready_dt(&leds[i])) {
            printk("[native] LED %d not ready\n", i);
            return -1;
        }
        gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
    }

    memset(shared_data, 0, sizeof(shared_data));
    printk("[native] GPIO ready: %d buttons, %d LEDs\n", NUM_BUTTONS, NUM_LEDS);
    return 0;
}

void
native_api_register(void)
{
    wasm_runtime_register_natives("env", native_symbols,
                                 sizeof(native_symbols) / sizeof(NativeSymbol));
}
