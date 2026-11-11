// Native Windows maintenance/identification client. No target Flash operations.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
const GUID interface_guid={0xcbb9952e,0x7bea,0x4b51,{0x9f,0x19,0x9c,0xe7,0x24,0xe8,0x73,0x57}};
void require(bool ok,const char *operation) {
    if (!ok) throw std::runtime_error(std::string(operation)+" (Windows error "+std::to_string(GetLastError())+")");
}
class DeviceList {
    HDEVINFO handle_=SetupDiGetClassDevsW(&interface_guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
public:
    DeviceList() { require(handle_!=INVALID_HANDLE_VALUE,"enumerate interfaces"); }
    ~DeviceList() { SetupDiDestroyDeviceInfoList(handle_); }
    std::vector<std::wstring> paths() {
        std::vector<std::wstring> result;
        for (DWORD i=0;;++i) {
            SP_DEVICE_INTERFACE_DATA data{};data.cbSize=sizeof(data);
            if (!SetupDiEnumDeviceInterfaces(handle_,nullptr,&interface_guid,i,&data)) {
                require(GetLastError()==ERROR_NO_MORE_ITEMS,"enumerate interface");break;
            }
            DWORD required=0;
            SetupDiGetDeviceInterfaceDetailW(handle_,&data,nullptr,0,&required,nullptr);
            std::vector<uint8_t> bytes(required);
            auto *detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(bytes.data());
            detail->cbSize=sizeof(*detail);
            require(SetupDiGetDeviceInterfaceDetailW(handle_,&data,detail,required,nullptr,nullptr),"interface detail");
            result.emplace_back(detail->DevicePath);
        }
        return result;
    }
};
class Probe {
    HANDLE file_=INVALID_HANDLE_VALUE;
    WINUSB_INTERFACE_HANDLE usb_=nullptr;
public:
    explicit Probe(const std::wstring &path) {
        file_=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
        require(file_!=INVALID_HANDLE_VALUE,"open probe");
        if (!WinUsb_Initialize(file_,&usb_)) { CloseHandle(file_);throw std::runtime_error("initialize WinUSB"); }
        ULONG timeout=3000;
        WinUsb_SetPipePolicy(usb_,0x01,PIPE_TRANSFER_TIMEOUT,sizeof(timeout),&timeout);
        WinUsb_SetPipePolicy(usb_,0x81,PIPE_TRANSFER_TIMEOUT,sizeof(timeout),&timeout);
    }
    ~Probe() { if (usb_) WinUsb_Free(usb_);if (file_!=INVALID_HANDLE_VALUE) CloseHandle(file_); }
    Probe(const Probe&)=delete;
    Probe& operator=(const Probe&)=delete;
    std::vector<uint8_t> command(const std::vector<uint8_t> &request) {
        ULONG n=0;
        require(WinUsb_WritePipe(usb_,0x01,const_cast<uint8_t *>(request.data()),static_cast<ULONG>(request.size()),&n,nullptr),"write command");
        require(n==request.size(),"short command write");
        std::array<uint8_t,64> response{};
        require(WinUsb_ReadPipe(usb_,0x81,response.data(),response.size(),&n,nullptr),"read response");
        return {response.begin(),response.begin()+n};
    }
    std::string info(uint8_t id) {
        auto r=command({0,id});
        if (r.size()<2 || r[0]!=0 || r[1]>r.size()-2) throw std::runtime_error("invalid DAP_Info reply");
        if (r[1]==0) return {};
        std::size_t count=r[1];
        if (count && r[count+1]==0) --count;
        return std::string(reinterpret_cast<char *>(r.data()+2),count);
    }
    void bootloader() {
        WINUSB_SETUP_PACKET setup{};setup.RequestType=0x40;setup.Request=0x42;setup.Value=0xb007;
        ULONG n=0;
        require(WinUsb_ControlTransfer(usb_,setup,nullptr,0,&n,nullptr),"request BOOTSEL");
    }
    void status() {
        WINUSB_SETUP_PACKET setup{};setup.RequestType=0xc0;setup.Request=0x43;setup.Length=28;
        std::array<uint32_t,7> data{};ULONG n=0;
        require(WinUsb_ControlTransfer(usb_,setup,reinterpret_cast<uint8_t *>(data.data()),sizeof(data),&n,nullptr),"read probe status");
        require(n==20 || n==sizeof(data),"status length");
        std::cout<<"GPIO levels=0x"<<std::hex<<data[0]<<" SWD output-enable=0x"<<data[1]
                 <<" last-reply=0x"<<data[4]<<std::dec<<" suspends="<<data[2]<<" packets="<<data[3];
        if (n>=28) std::cout<<" watchdog_reason="<<data[5]<<" uptime_ms="<<data[6];
        std::cout<<"\n";
    }
    void swdcheck() {
        unsigned successful=0;
        for (const uint32_t hz:{50000U,100000U,500000U}) {
            command({2,1});
            command({0x11,uint8_t(hz),uint8_t(hz>>8),uint8_t(hz>>16),uint8_t(hz>>24)});
            command({4,0,50,0,0,0});command({0x13,0});
            command({0x12,56,255,255,255,255,255,255,255});
            command({0x12,16,0x9e,0xe7});
            command({0x12,56,255,255,255,255,255,255,255});
            command({0x12,8,0});
            auto r=command({5,0,1,2});
            std::cout<<"SWD "<<hz<<"Hz DPIDR reply:";
            for (auto b:r) std::cout<<" "<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(b);
            std::cout<<std::dec<<"\n";status();
            if (r.size()==7 && r[0]==5 && r[1]==1 && r[2]==1) ++successful;
            command({3});
        }
        if (successful!=3) throw std::runtime_error("SWD ID read failed at one or more diagnostic clock settings");
    }
    void selftest() {
        const std::vector<std::vector<uint8_t>> bad={
            {0x05,0,1,0}, {0x06,0,0xff,0xff,2}, {0x11,0,0,0,0},
            {0x7f,0xff}, {0x10,0,0x80,0,0,0,0}, {0x1d,1,8}
        };
        for (const auto &p:bad) {
            auto r=command(p);
            if (r!=std::vector<uint8_t>{0xff}) throw std::runtime_error("malformed packet was not rejected");
        }
        for (unsigned i=0;i<100;++i) if (info(3).empty()) throw std::runtime_error("missing serial");
        std::cout<<"USB selftest: malformed requests rejected; 100 serial queries passed.\n";
    }
};
}
int main(int argc,char **argv) {
    try {
        const std::string operation=argc>1?argv[1]:"info";
        if (operation!="info" && operation!="selftest" && operation!="bootloader" && operation!="status" && operation!="swdcheck") {
            std::cerr<<"Usage: probe_cli [info|selftest|bootloader|status|swdcheck] [serial]\n";return 2;
        }
        if (operation!="info" && argc!=3) throw std::runtime_error("explicit probe serial required");
        DeviceList list;
        unsigned matched=0;
        for (const auto &path:list.paths()) {
            Probe probe(path);
            const auto serial=probe.info(3);
            if (argc>2 && serial!=argv[2]) continue;
            ++matched;
            std::cout<<"Serial: "<<serial<<"\nVendor: "<<probe.info(1)<<"\nProduct: "<<probe.info(2)<<"\nDAP version: "<<probe.info(4)<<"\nFirmware: "<<probe.info(9)<<"\n";
            if (operation=="selftest") probe.selftest();
            if (operation=="status") probe.status();
            if (operation=="swdcheck") probe.swdcheck();
            if (operation=="bootloader") { probe.bootloader();std::cout<<"BOOTSEL requested.\n"; }
        }
        if (!matched) throw std::runtime_error("no matching AI probe found");
        return 0;
    } catch (const std::exception &e) { std::cerr<<"Error: "<<e.what()<<"\n";return 1; }
}
