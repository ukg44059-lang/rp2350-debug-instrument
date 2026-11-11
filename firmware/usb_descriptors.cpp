#include <algorithm>
#include <cstring>
#include "tusb.h"
#include "pico/unique_id.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "pico/time.h"

extern bool boot_requested;
extern uint32_t suspend_count,packet_count,last_reply;
namespace {
// TinyUSB example VID/PID: private lab prototype only, not a production allocation.
const tusb_desc_device_t device = {
    sizeof(tusb_desc_device_t), TUSB_DESC_DEVICE, 0x0210,
    TUSB_CLASS_MISC,MISC_SUBCLASS_COMMON,MISC_PROTOCOL_IAD,64,0xcafe,0x4016,0x0200,1,2,3,1
};
constexpr uint8_t config[] = {
    TUD_CONFIG_DESCRIPTOR(1,4,0,TUD_CONFIG_DESC_LEN+2*TUD_VENDOR_DESC_LEN+TUD_CDC_DESC_LEN,0,100),
    TUD_VENDOR_DESCRIPTOR(0,4,0x01,0x81,64),
    TUD_CDC_DESCRIPTOR(1,5,0x82,8,0x03,0x83,64),
    TUD_VENDOR_DESCRIPTOR(3,6,0x04,0x84,64)
};
constexpr unsigned os_length=338;
constexpr uint8_t bos[] = {
    TUD_BOS_DESCRIPTOR(TUD_BOS_DESC_LEN+TUD_BOS_MICROSOFT_OS_DESC_LEN,1),
    TUD_BOS_MS_OS_20_DESCRIPTOR(os_length,0x20)
};
#pragma pack(push,1)
struct OsFunction {
    uint16_t function_length,function_type;
    uint8_t interface_number,function_reserved;
    uint16_t function_total;
    uint16_t compatible_length,compatible_type;
    char compatible[8],subcompatible[8];
    uint16_t property_length,property_type,property_data_type,property_name_length;
    char16_t property_name[21];
    uint16_t property_data_length;
    char16_t guid[40];
};
struct OsDescriptor {
    uint16_t header_length,header_type;
    uint32_t windows_version;
    uint16_t total_length;
    uint16_t config_length,config_type;
    uint8_t config_index,config_reserved;
    uint16_t config_total;
    OsFunction functions[2];
};
#pragma pack(pop)
static_assert(sizeof(OsDescriptor)==os_length);
constexpr OsDescriptor os = {
    10,0,0x06030000,os_length,
    8,1,0,0,os_length-10,
    {{8,2,0,0,160,20,3,"WINUSB","",
      132,4,7,42,u"DeviceInterfaceGUIDs",80,u"{CBB9952E-7BEA-4B51-9F19-9CE724E87357}\0"},
     {8,2,3,0,160,20,3,"WINUSB","",
      132,4,7,42,u"DeviceInterfaceGUIDs",80,u"{F6B88B84-3221-4B89-A912-730BD7F28AF1}\0"}}
};
}
extern "C" void probe_serial(char *s) { pico_get_unique_board_id_string(s,17); }
extern "C" const uint8_t *tud_descriptor_device_cb() { return reinterpret_cast<const uint8_t *>(&device); }
extern "C" const uint8_t *tud_descriptor_configuration_cb(uint8_t) { return config; }
extern "C" const uint8_t *tud_descriptor_bos_cb() { return bos; }
extern "C" const uint16_t *tud_descriptor_string_cb(uint8_t index,uint16_t) {
    static uint16_t result[64];
    static char serial[17];
    if (index==0) { result[0]=0x0304; result[1]=0x0409; return result; }
    probe_serial(serial);
    const char *strings[]={nullptr,"AI Debug Instrument","RP2350 Debug + Logic",serial,"CMSIS-DAP v2","Probe diagnostics","AI Logic Analyzer"};
    if (index>=sizeof(strings)/sizeof(strings[0])) return nullptr;
    const auto count=std::min(std::strlen(strings[index]),std::size_t{63});
    for (unsigned i=0;i<count;++i) result[1+i]=static_cast<uint8_t>(strings[index][i]);
    result[0]=(TUSB_DESC_STRING<<8) | (2*count+2);
    return result;
}
extern "C" bool tud_vendor_control_xfer_cb(uint8_t rhport,uint8_t stage,const tusb_control_request_t *request) {
    if (request->bmRequestType==0xc0 && request->bRequest==0x43 && request->wValue==0 && request->wIndex==0) {
        if (stage!=CONTROL_STAGE_SETUP) return true;
        static uint32_t status[7];
        status[0]=gpio_get_all();
        status[1]=(gpio_get_dir(2)<<2) | (gpio_get_dir(3)<<3);
        status[2]=suspend_count;status[3]=packet_count;status[4]=last_reply;
        status[5]=watchdog_hw->reason;status[6]=to_ms_since_boot(get_absolute_time());
        return tud_control_xfer(rhport,request,status,sizeof(status));
    }
    if (request->bmRequestType==0xc0 && request->bRequest==0x20 && request->wIndex==7) {
        if (stage!=CONTROL_STAGE_SETUP) return true;
        return tud_control_xfer(rhport,request,const_cast<OsDescriptor *>(&os),sizeof(os));
    }
    // Explicit local maintenance request, not triggered by a USB descriptor read.
    if (request->bmRequestType==0x40 && request->bRequest==0x42 && request->wValue==0xb007 && request->wIndex==0 && request->wLength==0) {
        if (stage==CONTROL_STAGE_SETUP) return tud_control_status(rhport,request);
        if (stage==CONTROL_STAGE_ACK) boot_requested=true;
        return true;
    }
    return false;
}
