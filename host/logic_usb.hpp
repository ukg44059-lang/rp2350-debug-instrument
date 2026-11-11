#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include "logic_protocol.hpp"
#include <array>
#include <string>
#include <stdexcept>
#include <vector>
namespace logic_usb {
inline void require(bool ok,const char* what) {
    if (!ok) throw std::runtime_error(std::string(what)+" Windows="+std::to_string(GetLastError()));
}
inline std::vector<std::wstring> paths() {
    const GUID guid={0xf6b88b84,0x3221,0x4b89,{0xa9,0x12,0x73,0x0b,0xd7,0xf2,0x8a,0xf1}};
    const auto list=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    require(list!=INVALID_HANDLE_VALUE,"enumerate logic");
    std::vector<std::wstring> result;
    for (DWORD i=0;;++i) {
        SP_DEVICE_INTERFACE_DATA d{};d.cbSize=sizeof(d);
        if (!SetupDiEnumDeviceInterfaces(list,nullptr,&guid,i,&d)) break;
        DWORD bytes=0;SetupDiGetDeviceInterfaceDetailW(list,&d,nullptr,0,&bytes,nullptr);
        std::vector<UCHAR> storage(bytes);
        auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());detail->cbSize=sizeof(*detail);
        if (SetupDiGetDeviceInterfaceDetailW(list,&d,detail,bytes,nullptr,nullptr)) result.emplace_back(detail->DevicePath);
    }
    SetupDiDestroyDeviceInfoList(list);return result;
}
class Device {
public:
    HANDLE file=INVALID_HANDLE_VALUE;
    WINUSB_INTERFACE_HANDLE usb=nullptr;
    std::string serial;
    explicit Device(const std::wstring& path) {
        file=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
        require(file!=INVALID_HANDLE_VALUE,"open logic USB");
        if (!WinUsb_Initialize(file,&usb)) {CloseHandle(file);file=INVALID_HANDLE_VALUE;require(false,"initialize WinUSB");}
        ULONG timeout=1500;
        WinUsb_SetPipePolicy(usb,0x04,PIPE_TRANSFER_TIMEOUT,sizeof(timeout),&timeout);
        WinUsb_SetPipePolicy(usb,0x84,PIPE_TRANSFER_TIMEOUT,sizeof(timeout),&timeout);
        UCHAR descriptor[64]{};ULONG n=0;
        if (WinUsb_GetDescriptor(usb,USB_STRING_DESCRIPTOR_TYPE,3,0x409,descriptor,sizeof(descriptor),&n))
            for (ULONG i=2;i+1<n && i<descriptor[0];i+=2) serial+=char(descriptor[i]);
    }
    ~Device() {if (usb) WinUsb_Free(usb);if (file!=INVALID_HANDLE_VALUE) CloseHandle(file);}
    Device(const Device&)=delete;Device& operator=(const Device&)=delete;
    void write(logic::Request request) {
        request.magic_word=logic::magic;ULONG n=0;
        require(WinUsb_WritePipe(usb,0x04,reinterpret_cast<UCHAR*>(&request),64,&n,nullptr),"write logic command");
        require(n==64,"short command");
    }
    logic::Reply command(logic::Request request) {
        write(request);logic::Reply r{};ULONG n=0;
        require(WinUsb_ReadPipe(usb,0x84,reinterpret_cast<UCHAR*>(&r),64,&n,nullptr),"read logic reply");
        if (n!=64 || r.magic_word!=logic::magic || r.operation!=request.operation || r.protocol!=logic::version || r.error)
            throw std::runtime_error("Logic command rejected / incompatible firmware (status="+std::to_string(r.error)+")");
        return r;
    }
};
// Eight pending reads keep the USB host controller supplied with requests.
class Reader {
    struct Slot {OVERLAPPED io{};std::array<UCHAR,16384> data{};bool pending=false;};
    Device& d_;std::array<Slot,8> slots_{};unsigned next_=0;
    void submit(Slot& s) {
        ResetEvent(s.io.hEvent);ULONG ignored=0;
        if (WinUsb_ReadPipe(d_.usb,0x84,s.data.data(),s.data.size(),&ignored,&s.io)) SetEvent(s.io.hEvent);
        else require(GetLastError()==ERROR_IO_PENDING,"queue USB read");
        s.pending=true;
    }
public:
    explicit Reader(Device& device):d_(device) {
        ULONG timeout=35000;WinUsb_SetPipePolicy(d_.usb,0x84,PIPE_TRANSFER_TIMEOUT,sizeof(timeout),&timeout);
        ULONG maximum=0,bytes=sizeof(maximum);
        require(WinUsb_GetPipePolicy(d_.usb,0x84,MAXIMUM_TRANSFER_SIZE,&bytes,&maximum),"USB transfer limit");
        if(maximum<slots_[0].data.size()) throw std::runtime_error("USB transfer limit is smaller than queued read buffers");
        // Bypass WinUSB's software request queue so all eight reads reach the
        // host controller at once. Buffers are multiples of the 64-byte MPS.
        UCHAR raw=TRUE;
        require(WinUsb_SetPipePolicy(d_.usb,0x84,RAW_IO,sizeof(raw),&raw),"enable direct queued USB reads");
        for (auto& s:slots_) {s.io.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);require(s.io.hEvent!=nullptr,"USB read event");}
        try {for (auto& s:slots_) submit(s);} catch (...) {cancel();throw;}
    }
    void cancel() {
        WinUsb_AbortPipe(d_.usb,0x84);
        for (auto& s:slots_) {
            if (s.pending) {ULONG ignored=0;WinUsb_GetOverlappedResult(d_.usb,&s.io,&ignored,TRUE);s.pending=false;}
            if (s.io.hEvent) {CloseHandle(s.io.hEvent);s.io.hEvent=nullptr;}
        }
    }
    ~Reader() {cancel();}
    bool next(std::vector<std::uint8_t>& out) {
        auto& s=slots_[next_];const auto wait=WaitForSingleObject(s.io.hEvent,100);
        if (wait==WAIT_TIMEOUT) return false;
        require(wait==WAIT_OBJECT_0,"wait USB read");ULONG n=0;
        require(WinUsb_GetOverlappedResult(d_.usb,&s.io,&n,FALSE),"USB streaming read");
        out.assign(s.data.begin(),s.data.begin()+n);s.pending=false;submit(s);next_=(next_+1)%slots_.size();return true;
    }
};
}
