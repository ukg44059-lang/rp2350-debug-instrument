// STM32F407 PB2 blink, C++17, no external oscillator or vendor runtime required.
#include <cstdint>

namespace {
volatile std::uint32_t &reg(std::uintptr_t address) {
    return *reinterpret_cast<volatile std::uint32_t *>(address);
}
constexpr std::uintptr_t rcc=0x40023800, gpiob=0x40020400, systick=0xe000e010;
constexpr std::uint32_t pin=1U<<2;
constexpr std::uint32_t core_hz=16000000, half_period_ms=200;

void setup_hsi_clock() {
    reg(rcc)|=1U; // HSION
    while (!(reg(rcc)&(1U<<1))) {} // HSIRDY
    reg(rcc+0x08)&=~(3U | (15U<<4) | (7U<<10) | (7U<<13));
    while (reg(rcc+0x08)&(3U<<2)) {} // SWS=HSI; AHB/APB prescalers=1
}
void setup_pb2() {
    reg(rcc+0x30)|=1U<<1; // RCC_AHB1ENR.GPIOBEN
    const std::uint32_t enabled=reg(rcc+0x30); // force volatile peripheral read-back
    (void)enabled;
    reg(gpiob+0x18)=pin<<16; // preload output low before changing mode
    reg(gpiob+0x04)&=~pin; // push-pull
    reg(gpiob+0x08)&=~(3U<<4); // low slew rate
    reg(gpiob+0x0c)&=~(3U<<4); // no pulls
    reg(gpiob)=(reg(gpiob)&~(3U<<4)) | (1U<<4); // MODER2=01: output
}
void setup_systick() {
    reg(systick)=0;
    reg(systick+4)=(core_hz/1000)*half_period_ms-1;
    reg(systick+8)=0;
    reg(systick)=5; // ENABLE + CLKSOURCE, interrupt disabled
}
}

extern "C" {
extern std::uint32_t _sidata,_sdata,_edata,_sbss,_ebss,_stack_top;
volatile std::uint32_t led_level=0;
volatile std::uint32_t toggle_count=0;
volatile std::uint32_t demo_signature=0x407b0002;
void Reset_Handler();
void Default_Handler() { for (;;) asm volatile("nop"); }
using Handler=void (*)();
__attribute__((section(".vectors"),used)) Handler const vectors[16]={
    reinterpret_cast<Handler>(&_stack_top),Reset_Handler,
    Default_Handler,Default_Handler,Default_Handler,Default_Handler,Default_Handler,
    nullptr,nullptr,nullptr,nullptr,Default_Handler,Default_Handler,nullptr,Default_Handler,Default_Handler
};
__attribute__((noinline)) void led_toggle() {
    led_level^=1U;
    reg(gpiob+0x18)=led_level ? pin : pin<<16;
    ++toggle_count;
}
void Reset_Handler() {
    auto *src=&_sidata;
    for (auto *dst=&_sdata;dst<&_edata;) *dst++=*src++;
    for (auto *dst=&_sbss;dst<&_ebss;) *dst++=0;
    setup_hsi_clock();
    setup_pb2();
    setup_systick();
    // Preserve an identifiable initialized-data symbol for live verification.
    if (demo_signature!=0x407b0002) Default_Handler();
    for (;;) {
        while (!(reg(systick)&(1U<<16))) {} // COUNTFLAG every 200 ms
        led_toggle();
    }
}
}
