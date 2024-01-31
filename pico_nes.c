
#include <stdint.h>
#include <stdio.h>

#include "bsp/board.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "usb_descriptors.h"

#define CONTROLLER_ADDRESS 0x52

#define BUTTON_A (1 << 12)
#define BUTTON_B (1 << 14)
#define BUTTON_START (1 << 2)
#define BUTTON_SELECT (1 << 4)
#define DPAD_UP (1 << 8)
#define DPAD_DOWN (1 << 6)
#define DPAD_LEFT (1 << 9)
#define DPAD_RIGHT (1 << 7)

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED = 1000,
    BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

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

uint16_t get_controller_input(i2c_inst_t *i2c) {

    static uint8_t information[6];
    const uint8_t button_reg[1] = {0x00};
    i2c_write_blocking(i2c_default, CONTROLLER_ADDRESS, button_reg, 1, false);
    i2c_read_blocking(i2c_default, CONTROLLER_ADDRESS, information, 6, false);

    // debug_print(i2c_default, *((uint16_t *)(buttons + 4)));
    return *((uint16_t *)(information + 4));
}

static void send_hid_report(uint16_t activated) {
    // skip if hid is not ready yet
    if (!tud_hid_ready())
        return;

    // use to avoid send multiple consecutive zero report for keyboard
    static bool has_gamepad_key = false;

    hid_gamepad_report_t report = {.x = 0,
                                   .y = 0,
                                   .z = 0,
                                   .rz = 0,
                                   .rx = 0,
                                   .ry = 0,
                                   .hat = 0,
                                   .buttons = 0};

    if (activated) {
        if (activated & BUTTON_A) {
            report.buttons |= GAMEPAD_BUTTON_A;
        }
        if (activated & BUTTON_B) {
            report.buttons |= GAMEPAD_BUTTON_B;
        }
        if (activated & BUTTON_START) {
            report.buttons |= GAMEPAD_BUTTON_START;
        }
        if (activated & BUTTON_SELECT) {
            report.buttons |= GAMEPAD_BUTTON_SELECT;
        }
        if (activated & DPAD_UP) {
            report.hat = GAMEPAD_HAT_UP;
        }
        if (activated & DPAD_DOWN) {
            report.hat = GAMEPAD_HAT_DOWN;
        }
        if (activated & DPAD_LEFT) {
            switch (report.hat) {
            case GAMEPAD_HAT_DOWN: {
                report.hat = GAMEPAD_HAT_DOWN_LEFT;
            } break;
            case GAMEPAD_HAT_UP: {
                report.hat = GAMEPAD_HAT_UP_LEFT;
            } break;
            default: {
                report.hat = GAMEPAD_HAT_LEFT;
            } break;
            }
        }
        if (activated & DPAD_RIGHT) {
            switch (report.hat) {
            case GAMEPAD_HAT_DOWN: {
                report.hat = GAMEPAD_HAT_DOWN_RIGHT;
            } break;
            case GAMEPAD_HAT_UP: {
                report.hat = GAMEPAD_HAT_UP_RIGHT;
            } break;
            default: {
                report.hat = GAMEPAD_HAT_RIGHT;
            } break;
            }
        }

        tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
        has_gamepad_key = true;
    } else {
        report.buttons = 0;
        if (has_gamepad_key)
            tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
        has_gamepad_key = false;
    }
}

// Every 10ms, we will sent 1 report for each HID profile (keyboard, mouse etc
// ..) tud_hid_report_complete_cb() is used to send the next report after
// previous one is complete
void hid_task(void) {
    // Poll every 10ms
    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;

    if (board_millis() - start_ms < interval_ms)
        return; // not enough time
    start_ms += interval_ms;

    uint16_t activated = ~get_controller_input(i2c_default);

    // Remote wakeup
    if (tud_suspended() && activated) {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    } else {
        // Send the 1st of report chain, the rest will be sent by
        // tud_hid_report_complete_cb()
        send_hid_report(activated);
    }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report,
                                uint16_t len) {
    (void)instance;
    (void)len;

    uint8_t next_report_id = report[0] + 1;

    uint16_t activated = ~get_controller_input(i2c_default);

    if (next_report_id < REPORT_ID_COUNT) {
        send_hid_report(activated);
    }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
    // TODO not Implemented
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
    // don't care, didnt'ask + bozo
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

// Invoked when device is mounted
void tud_mount_cb(void) { blink_interval_ms = BLINK_MOUNTED; }

// Invoked when device is unmounted
void tud_umount_cb(void) { blink_interval_ms = BLINK_NOT_MOUNTED; }

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) { blink_interval_ms = BLINK_MOUNTED; }

void led_blinking_task(void) {
    static uint32_t start_ms = 0;
    static bool led_state = false;

    // blink is disabled
    if (!blink_interval_ms)
        return;

    // Blink every interval ms
    if (board_millis() - start_ms < blink_interval_ms)
        return; // not enough time
    start_ms += blink_interval_ms;

    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
}

int main() {
    stdio_init_all();
#if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) ||             \
    !defined(PICO_DEFAULT_I2C_SCL_PIN)
#warning i2c/bus_scan example requires a board with I2C pins
    puts("Default I2C pins were not defined");
#else
    // init i2c
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN,
                               PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    // init tinyusb
    board_init();
    tusb_init();

    int retval;
    retval = init_no_encryption(i2c_default);
    while (retval < 0) {
        printf("error initializing! retval: %d\n", retval);
    }

    int dataformat = get_dataformat(i2c_default);

    while (true) {
        tud_task();
        led_blinking_task();
        hid_task();
    }
    return 0;
#endif
}
