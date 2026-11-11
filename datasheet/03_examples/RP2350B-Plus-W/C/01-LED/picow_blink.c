/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#define LED2_PIN 23

int main() {
    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
        return -1;
    }
    gpio_init(LED2_PIN);
    gpio_set_dir(LED2_PIN, GPIO_OUT);

    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        gpio_put(LED2_PIN, 1);
        sleep_ms(250);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        gpio_put(LED2_PIN, 0);
        sleep_ms(250);
    }
}