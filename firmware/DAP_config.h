// RP2350 GPIO adaptation for Arm CMSIS-DAP (upstream files retain Apache-2.0).
#ifndef AI_DAP_CONFIG_H
#define AI_DAP_CONFIG_H
#include <stdint.h>
#include <string.h>
#include "cmsis_compiler.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#define CPU_CLOCK PROBE_CPU_CLOCK_HZ
#define IO_PORT_WRITE_CYCLES 2U
#define DAP_SWD 1
#define DAP_JTAG 0
#define DAP_JTAG_DEV_CNT 0U
#define DAP_DEFAULT_PORT 1U
#define DAP_DEFAULT_SWJ_CLOCK 100000U
#define DAP_PACKET_SIZE 64U
#define DAP_PACKET_COUNT 1U
#define SWO_UART 0
#define SWO_MANCHESTER 0
#define SWO_STREAM 0
#define TIMESTAMP_CLOCK 0U
#define DAP_UART 0
#define DAP_UART_USB_COM_PORT 0
#define TARGET_FIXED 0

// Strings include their terminating NUL, as required by CMSIS-DAP.
__STATIC_INLINE uint8_t DAP_GetVendorString(char *s) { strcpy(s,"AI Debug Instrument"); return sizeof("AI Debug Instrument"); }
__STATIC_INLINE uint8_t DAP_GetProductString(char *s) { strcpy(s,"RP2350 CMSIS-DAP"); return sizeof("RP2350 CMSIS-DAP"); }
void probe_serial(char *s);
__STATIC_INLINE uint8_t DAP_GetSerNumString(char *s) { probe_serial(s); return 17; }
__STATIC_INLINE uint8_t DAP_GetProductFirmwareVersionString(char *s) { strcpy(s,PROBE_FIRMWARE_VERSION); return sizeof(PROBE_FIRMWARE_VERSION); }
__STATIC_INLINE uint8_t DAP_GetTargetDeviceVendorString(char *s) { (void)s; return 0; }
__STATIC_INLINE uint8_t DAP_GetTargetDeviceNameString(char *s) { (void)s; return 0; }
__STATIC_INLINE uint8_t DAP_GetTargetBoardVendorString(char *s) { (void)s; return 0; }
__STATIC_INLINE uint8_t DAP_GetTargetBoardNameString(char *s) { (void)s; return 0; }

__STATIC_INLINE void PORT_OFF(void) {
    for (unsigned p=2; p<=7; ++p) { gpio_set_dir(p,GPIO_IN); gpio_disable_pulls(p); }
}
__STATIC_INLINE void PORT_JTAG_SETUP(void) { /* Not advertised or enabled. */ }
__STATIC_INLINE void PORT_SWD_SETUP(void) {
    PORT_OFF();
    gpio_put(2,1); gpio_put(3,1);
    gpio_set_dir(2,GPIO_OUT); gpio_set_dir(3,GPIO_OUT);
}
__STATIC_FORCEINLINE uint32_t PIN_SWCLK_TCK_IN(void) { return gpio_get(2); }
__STATIC_FORCEINLINE void PIN_SWCLK_TCK_SET(void) { gpio_put(2,1); }
__STATIC_FORCEINLINE void PIN_SWCLK_TCK_CLR(void) { gpio_put(2,0); }
__STATIC_FORCEINLINE uint32_t PIN_SWDIO_TMS_IN(void) { return gpio_get(3); }
__STATIC_FORCEINLINE void PIN_SWDIO_TMS_SET(void) { gpio_put(3,1); }
__STATIC_FORCEINLINE void PIN_SWDIO_TMS_CLR(void) { gpio_put(3,0); }
__STATIC_FORCEINLINE uint32_t PIN_SWDIO_IN(void) { return gpio_get(3); }
__STATIC_FORCEINLINE void PIN_SWDIO_OUT(uint32_t bit) { gpio_put(3,bit & 1); }
__STATIC_FORCEINLINE void PIN_SWDIO_OUT_ENABLE(void) { gpio_set_dir(3,GPIO_OUT); }
__STATIC_FORCEINLINE void PIN_SWDIO_OUT_DISABLE(void) { gpio_set_dir(3,GPIO_IN); }
__STATIC_FORCEINLINE uint32_t PIN_TDI_IN(void) { return 0; }
__STATIC_FORCEINLINE void PIN_TDI_OUT(uint32_t bit) { (void)bit; }
__STATIC_FORCEINLINE uint32_t PIN_TDO_IN(void) { return 0; }
__STATIC_FORCEINLINE uint32_t PIN_nTRST_IN(void) { return 1; }
__STATIC_FORCEINLINE void PIN_nTRST_OUT(uint32_t bit) { (void)bit; }
// NRST is physically unconnected. Never drive GP4 or claim a physical reading.
__STATIC_FORCEINLINE uint32_t PIN_nRESET_IN(void) { return 1; }
__STATIC_FORCEINLINE void PIN_nRESET_OUT(uint32_t bit) { (void)bit; }
__STATIC_INLINE void LED_CONNECTED_OUT(uint32_t bit) { gpio_put(23,bit != 0); }
__STATIC_INLINE void LED_RUNNING_OUT(uint32_t bit) { (void)bit; }
__STATIC_INLINE uint32_t TIMESTAMP_GET(void) { return 0; }
__STATIC_INLINE void DAP_SETUP(void) { PORT_OFF(); }
__STATIC_INLINE uint8_t RESET_TARGET(void) { return 0; }
#endif
