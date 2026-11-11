// Standalone STM32F407 download/debug test, intentionally does not touch GPIO.
#include <cstdint>
extern "C" {
extern std::uint32_t _sidata,_sdata,_edata,_sbss,_ebss,_stack_top;
volatile std::uint32_t heartbeat=0;
volatile std::uint32_t test_signature=0x407c0017;
void Reset_Handler();
void Default_Handler() { for (;;) asm volatile("nop"); }
using Handler=void (*)();
__attribute__((section(".vectors"),used)) Handler const vectors[16]={
    reinterpret_cast<Handler>(&_stack_top),Reset_Handler,
    Default_Handler,Default_Handler,Default_Handler,Default_Handler,Default_Handler,
    nullptr,nullptr,nullptr,nullptr,Default_Handler,Default_Handler,nullptr,Default_Handler,Default_Handler
};
__attribute__((noinline)) void heartbeat_tick() { heartbeat += test_signature==0x407c0017 ? 1U : 2U; }
void Reset_Handler() {
    auto *src=&_sidata;
    for (auto *dst=&_sdata;dst<&_edata;) *dst++=*src++;
    for (auto *dst=&_sbss;dst<&_ebss;) *dst++=0;
    for (;;) {
        heartbeat_tick();
        for (volatile unsigned i=0;i<100000;++i) asm volatile("nop");
    }
}
}
