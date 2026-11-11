#pragma once
#include "hardware/gpio.h"

namespace board {
inline constexpr unsigned swclk = 2, swdio = 3, nreset = 4;
inline constexpr unsigned tdi = 5, tdo = 6, ntrst = 7, led = 23;
inline constexpr bool has_reset = false;
inline void release() {
    for (unsigned pin = swclk; pin <= ntrst; ++pin) {
        gpio_set_dir(pin, GPIO_IN);
        gpio_disable_pulls(pin);
    }
}
inline void init() {
    for (unsigned pin = swclk; pin <= ntrst; ++pin) gpio_init(pin);
    release();
    gpio_init(led);
    gpio_set_dir(led, GPIO_OUT);
    gpio_put(led, false);
}
}
