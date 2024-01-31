
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include <stdio.h>

#define CONTROLLER_ADDRESS 0x52

#define BUTTON_A (1 << 12)
#define BUTTON_B (1 << 14)
#define BUTTON_START (1 << 2)
#define BUTTON_SELECT (1 << 4)
#define DPAD_UP (1 << 8)
#define DPAD_DOWN (1 << 6)
#define DPAD_LEFT (1 << 9)
#define DPAD_RIGHT (1 << 7)

int init_no_encryption(i2c_inst_t *i2c) {
    int retval;

    const uint8_t init_buf_first[2] = {0xf0, 0x55};
    retval =
        i2c_write_blocking(i2c, CONTROLLER_ADDRESS, init_buf_first, 2, false);
    if (retval < 0)
        return retval;

    const uint8_t init_buf_second[2] = {0xfb, 0x00};
    retval =
        i2c_write_blocking(i2c, CONTROLLER_ADDRESS, init_buf_second, 2, false);
    return retval;
}

int get_dataformat(i2c_inst_t *i2c) {
    uint8_t dataformat;
    const uint8_t dataformat_reg[1] = {0xfe};

    i2c_write_blocking(i2c, CONTROLLER_ADDRESS, dataformat_reg, 1, false);
    i2c_read_blocking(i2c, CONTROLLER_ADDRESS, &dataformat, 1, false);

    return dataformat;
}

void debug_print(i2c_inst_t *i2c, uint16_t buttons) {
    uint16_t activated = ~buttons;

    if (activated & BUTTON_A) {
        printf("A! ");
    }
    if (activated & BUTTON_B) {
        printf("B! ");
    }
    if (activated & BUTTON_START) {
        printf("START! ");
    }
    if (activated & BUTTON_SELECT) {
        printf("SELECT! ");
    }
    if (activated & DPAD_UP) {
        printf("UP! ");
    }
    if (activated & DPAD_DOWN) {
        printf("DOWN! ");
    }
    if (activated & DPAD_LEFT) {
        printf("LEFT! ");
    }
    if (activated & DPAD_RIGHT) {
        printf("RIGHT! ");
    }
    printf("\n");
}

int main() {
    // Enable UART so we can print status output
    stdio_init_all();
#if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) ||             \
    !defined(PICO_DEFAULT_I2C_SCL_PIN)
#warning i2c/bus_scan example requires a board with I2C pins
    puts("Default I2C pins were not defined");
#else
    // This example will use I2C0 on the default SDA and SCL pins (GP4, GP5 on a
    // Pico)
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN,
                               PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    int retval;
    retval = init_no_encryption(i2c_default);
    while (retval < 0) {
        printf("error initializing! retval: %d\n", retval);
    }

    int dataformat = get_dataformat(i2c_default);

    static uint8_t buttons[6];
    while (true) {
        const uint8_t button_reg[1] = {0x00};
        retval = i2c_write_blocking(i2c_default, CONTROLLER_ADDRESS, button_reg,
                                    1, false);
        retval = i2c_read_blocking(i2c_default, CONTROLLER_ADDRESS, buttons, 6,
                                   false);

        // printf("%04x\n", *((uint16_t *)(buttons + 4)));
        debug_print(i2c_default, *((uint16_t *)(buttons + 4)));
        sleep_ms(1000);
    }
    return 0;
#endif
}
