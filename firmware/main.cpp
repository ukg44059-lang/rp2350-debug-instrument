#include <array>
#include <cstring>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/structs/qmi.h"
#include "hardware/sync.h"
#include "tusb.h"
#include "board.hpp"
#include "packet_guard.hpp"
#include "logic_analyzer.hpp"
#include "pico/multicore.h"
extern "C" {
#include "DAP_config.h"
#include "DAP.h"
}

bool boot_requested=false;
uint32_t suspend_count=0;
uint32_t packet_count=0;
uint32_t last_reply=0;
extern "C" void tud_umount_cb() { PORT_OFF(); logic::disconnected(); gpio_put(board::led,false); }
extern "C" void tud_suspend_cb(bool) { ++suspend_count; PORT_OFF(); logic::disconnected(); gpio_put(board::led,false); }
extern "C" void tud_cdc_line_coding_cb(uint8_t,const cdc_line_coding_t *coding) {
    if (coding->bit_rate==1200) boot_requested=true;
}

namespace {
#if PROBE_CPU_CLOCK_HZ == 300000000U
// The RP2350 ROM configures XIP; the board's boot2 divider macro alone does
// not change it when PICO_EMBED_XIP_SETUP=0. Preserve the ROM's flash format.
// Run in SRAM before core1/DMA start, without instruction fetches from flash.
void __no_inline_not_in_flash_func(limit_flash_clock)() {
    const auto irq_state=save_and_disable_interrupts();
    const auto divider=qmi_hw->m[0].timing & QMI_M0_TIMING_CLKDIV_BITS;
    if (divider<4) {
        hw_write_masked(&qmi_hw->m[0].timing,4U << QMI_M0_TIMING_CLKDIV_LSB,
                        QMI_M0_TIMING_CLKDIV_BITS);
        __dsb();
        __isb();
    }
    restore_interrupts(irq_state);
}
#endif
void configure_system_clock() {
#if PROBE_CPU_CLOCK_HZ == 300000000U
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    limit_flash_clock(); // at most 75 MHz flash SCK at 300 MHz clk_sys
#endif
    set_sys_clock_khz(PROBE_CPU_CLOCK_HZ/1000U,true);
    // FC0 checks the physical PLL output against the crystal reference.
    // Keep the maintenance bootloader reachable if clock setup is wrong.
    const auto measured=frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    if (measured<PROBE_CPU_CLOCK_HZ/1000U-1000U ||
        measured>PROBE_CPU_CLOCK_HZ/1000U+1000U) reset_usb_boot(0,0);
}
class ProbeApplication {
    alignas(4) std::array<uint8_t,64> request_{};
    alignas(4) std::array<uint8_t,64> response_{};
public:
    void run() {
        configure_system_clock();
        board::init();
        DAP_Setup();
        logic::init();
        multicore_launch_core1(logic::core1_entry);
        tusb_init();
        watchdog_enable(8000,true);
        while (true) {
            watchdog_update();
            tud_task();
            while (tud_cdc_available()) { char ignored; tud_cdc_read(&ignored,1); }
            if (boot_requested) { PORT_OFF(); reset_usb_boot(0,0); }
            logic::usb_task(); // at most one analysis packet per loop; DAP remains independently serviced
            if (!tud_mounted() || tud_vendor_n_write_available(0)<64 || !tud_vendor_available()) continue;
            request_.fill(0);
            const auto length=tud_vendor_read(request_.data(),request_.size());
            ++packet_count;
            response_.fill(0);
            unsigned response_size=1;
            if (probe::valid_packet(request_.data(),length)) {
                response_size=DAP_ExecuteCommand(request_.data(),response_.data()) & 0xffff;
                // Upstream advertises ExecuteCommands unconditionally; our guard rejects batches.
                if (request_[0]==0 && request_[1]==0xf0 && response_size>=3) response_[2]&=~0x10;
            } else {
                response_[0]=0xff; // DAP_Invalid, no GPIO side effects.
            }
            if (response_size==0 || response_size>response_.size()) {
                PORT_OFF(); response_[0]=0xff; response_size=1;
            }
            tud_vendor_write(response_.data(),response_size);
            last_reply=response_[0] | (uint32_t(response_[1])<<8) | (uint32_t(response_[2])<<16);
            tud_vendor_write_flush();
        }
    }
};
}
int main() { ProbeApplication app; app.run(); }
