// SeekFree RT1064 onboard LED: B9 = GPIO_AD_B0_09, GPIO1_IO09, active low.
// This image does not enable external SDRAM.
#include <cstdint>
#include "MIMXRT1064.h"

namespace {
constexpr std::uint32_t pin = 1U << 9;
constexpr std::uint32_t half_period_ticks = 24000000U / 5U;
#ifdef RT1064_SIGNAL_TEST
constexpr unsigned signal_pins[8] = {2, 13, 12, 17, 19, 21, 23, 25};
constexpr std::uint32_t signal_mask = (1U<<2)|(1U<<13)|(1U<<12)|(1U<<17)|
                                      (1U<<19)|(1U<<21)|(1U<<23)|(1U<<25);
void setup_signals() {
    CCM->CCGR1 |= CCM_CCGR1_CG13(3);
    IOMUXC_GPR->GPR26 &= ~signal_mask;
    GPIO1->DR_CLEAR = signal_mask;
    GPIO1->IMR &= ~signal_mask;
    for (auto bit : signal_pins) {
        *reinterpret_cast<volatile std::uint32_t *>(0x401F82ACU + 4*bit) = 0x10B0U;
        *reinterpret_cast<volatile std::uint32_t *>(0x401F80BCU + 4*bit) = 0x15U;
    }
    GPIO1->GDIR |= signal_mask;
    __DSB();
}
void write_signals(std::uint32_t count) {
    std::uint32_t levels = 0;
    for (unsigned ch = 0; ch < 8; ++ch)
        if (count & (1U<<ch)) levels |= 1U<<signal_pins[ch];
    GPIO1->DR = (GPIO1->DR & ~signal_mask) | levels;
}
#endif
void disable_watchdogs() {
    WDOG1->WMCR &= ~WDOG_WMCR_PDE_MASK;
    WDOG2->WMCR &= ~WDOG_WMCR_PDE_MASK;
    WDOG1->WCR &= ~WDOG_WCR_WDE_MASK;
    WDOG2->WCR &= ~WDOG_WCR_WDE_MASK;
    if (RTWDOG->CS & RTWDOG_CS_CMD32EN_MASK) RTWDOG->CNT = 0xD928C520U;
    else { RTWDOG->CNT = 0xC520U; RTWDOG->CNT = 0xD928U; }
    RTWDOG->TOVAL = 0xFFFFU;
    RTWDOG->CS = (RTWDOG->CS & ~RTWDOG_CS_EN_MASK) | RTWDOG_CS_UPDATE_MASK;
}
void setup_gpio() {
    CCM->CCGR4 |= CCM_CCGR4_CG1(3) | CCM_CCGR4_CG2(3);
    CCM->CCGR1 |= CCM_CCGR1_CG13(3);
    IOMUXC_GPR->GPR26 &= ~pin; // Route slow GPIO1, rather than fast GPIO6.
    GPIO1->DR_SET = pin; // Preload off (active low).
    GPIO1->IMR &= ~pin;
    *reinterpret_cast<volatile std::uint32_t *>(0x401F82D0U) = 0x10B0U;
    *reinterpret_cast<volatile std::uint32_t *>(0x401F80E0U) = 0x15U;
    GPIO1->GDIR |= pin;
    __DSB();
}
void setup_timer() {
    CCM->CCGR1 |= CCM_CCGR1_CG10(3) | CCM_CCGR1_CG11(3);
    GPT1->CR = GPT_CR_SWR_MASK;
    while (GPT1->CR & GPT_CR_SWR_MASK) {}
    GPT1->PR = 0; // Crystal and GPT dividers both /1.
    GPT1->IR = 0;
    GPT1->SR = 0x3FU;
    GPT1->CR = GPT_CR_CLKSRC(5) | GPT_CR_EN_24M_MASK |
               GPT_CR_FRR_MASK | GPT_CR_ENMOD_MASK | GPT_CR_EN_MASK;
}
}

extern "C" {
extern std::uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _stack_top;
void Reset_Handler();
volatile std::uint32_t toggle_count;
volatile std::uint32_t led_level;
#ifdef RT1064_SIGNAL_TEST
volatile std::uint32_t demo_signature = 0x1064A008U;
// Adjustable from debugger: 24000=1 kstep/s, 480=50 kstep/s.
volatile std::uint32_t signal_step_ticks = 24000;
volatile std::uint32_t signal_count;
#else
volatile std::uint32_t demo_signature = 0x1064B009U;
#endif
void Default_Handler() { for (;;) __NOP(); }
using Handler = void (*)();
__attribute__((section(".vectors"), used, aligned(1024)))
Handler const vectors[16] = {
    reinterpret_cast<Handler>(&_stack_top), Reset_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    nullptr, nullptr, nullptr, nullptr, Default_Handler,
    Default_Handler, nullptr, Default_Handler, Default_Handler
};
// ROM boot uses IVT.entry as the vector-table address, not Reset_Handler.
__attribute__((section(".boot_ivt"), used))
const std::uint32_t boot_ivt[8] = {
    0x412000D1U, 0x70002000U, 0, 0, 0x70001020U, 0x70001000U, 0, 0
};
__attribute__((section(".boot_data"), used))
const std::uint32_t boot_data[4] = {0x70000000U, 0x10000U, 0, 0xFFFFFFFFU};
__attribute__((noinline)) void led_toggle() {
    GPIO1->DR_TOGGLE = pin;
    led_level = (GPIO1->DR >> 9) & 1U;
    ++toggle_count;
}
void Reset_Handler() {
    __disable_irq();
    SCB->VTOR = 0x70002000U;
    SysTick->CTRL = 0;
    MPU->CTRL = 0;
    __DSB(); __ISB();
    auto *src = &_sidata;
    for (auto *dst = &_sdata; dst < &_edata;) *dst++ = *src++;
    for (auto *dst = &_sbss; dst < &_ebss;) *dst++ = 0;
    disable_watchdogs();
    setup_gpio();
#ifdef RT1064_SIGNAL_TEST
    setup_signals();
#endif
    setup_timer();
#ifdef RT1064_SIGNAL_TEST
    if (demo_signature != 0x1064A008U) Default_Handler();
    auto signal_previous = GPT1->CNT;
#else
    if (demo_signature != 0x1064B009U) Default_Handler();
#endif
    auto previous = GPT1->CNT;
    for (;;) {
        const auto now = GPT1->CNT;
#ifdef RT1064_SIGNAL_TEST
        if (std::uint32_t(now - signal_previous) >= signal_step_ticks) {
            signal_previous += signal_step_ticks;
            write_signals(++signal_count);
        }
#endif
        if (std::uint32_t(now - previous) >= half_period_ticks) {
            previous += half_period_ticks;
            led_toggle();
        }
    }
}
}
