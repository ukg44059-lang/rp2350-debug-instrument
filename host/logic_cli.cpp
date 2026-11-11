#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include "logic_protocol.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
const GUID guid={0xf6b88b84,0x3221,0x4b89,{0xa9,0x12,0x73,0x0b,0xd7,0xf2,0x8a,0xf1}};
void check(bool ok,const char *what) {if (!ok) throw std::runtime_error(std::string(what)+" Windows="+std::to_string(GetLastError()));}
class Device {
    HANDLE file=INVALID_HANDLE_VALUE;
    WINUSB_INTERFACE_HANDLE usb=nullptr;
public:
    std::string serial;
    explicit Device(const std::wstring &path) {
        file=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
        check(file!=INVALID_HANDLE_VALUE,"open logic interface");
        if (!WinUsb_Initialize(file,&usb)) {CloseHandle(file);file=INVALID_HANDLE_VALUE;throw std::runtime_error("initialize logic WinUSB");}
        ULONG timeout=3000;
        WinUsb_SetPipePolicy(usb,0x04,PIPE_TRANSFER_TIMEOUT,sizeof(timeout),&timeout);
        WinUsb_SetPipePolicy(usb,0x84,PIPE_TRANSFER_TIMEOUT,sizeof(timeout),&timeout);
        UCHAR desc[64]{};ULONG n=0;
        check(WinUsb_GetDescriptor(usb,USB_STRING_DESCRIPTOR_TYPE,3,0x409,desc,sizeof(desc),&n),"read serial");
        for (ULONG i=2;i+1<n && i<desc[0];i+=2) serial+=char(desc[i]);
    }
    ~Device() {if (usb) WinUsb_Free(usb);if (file!=INVALID_HANDLE_VALUE) CloseHandle(file);}
    Device(const Device&)=delete;Device& operator=(const Device&)=delete;
    void read(void *data,std::uint32_t size) {
        auto *p=static_cast<UCHAR *>(data);
        while (size) {ULONG n=0;check(WinUsb_ReadPipe(usb,0x84,p,size,&n,nullptr),"read logic response");if (!n || n>size) throw std::runtime_error("short/invalid USB read");p+=n;size-=n;}
    }
    logic::Reply command(logic::Request r) {
        r.magic_word=logic::magic;ULONG n=0;
        check(WinUsb_WritePipe(usb,0x04,reinterpret_cast<UCHAR *>(&r),sizeof(r),&n,nullptr),"write logic command");
        if (n!=sizeof(r)) throw std::runtime_error("short logic command");
        logic::Reply reply{};read(&reply,sizeof(reply));
        if (reply.magic_word!=logic::magic || reply.protocol!=logic::version || reply.operation!=r.operation) throw std::runtime_error("logic protocol mismatch");
        return reply;
    }
};
std::vector<std::wstring> paths() {
    const auto list=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    check(list!=INVALID_HANDLE_VALUE,"enumerate logic devices");
    std::vector<std::wstring> result;
    for (DWORD i=0;;++i) {
        SP_DEVICE_INTERFACE_DATA d{};d.cbSize=sizeof(d);
        if (!SetupDiEnumDeviceInterfaces(list,nullptr,&guid,i,&d)) {const auto error=GetLastError();SetupDiDestroyDeviceInfoList(list);check(error==ERROR_NO_MORE_ITEMS,"enumerate logic interface");break;}
        DWORD size=0;SetupDiGetDeviceInterfaceDetailW(list,&d,nullptr,0,&size,nullptr);
        std::vector<UCHAR> storage(size);
        auto *detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(storage.data());detail->cbSize=sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(list,&d,detail,size,nullptr,nullptr)) {SetupDiDestroyDeviceInfoList(list);throw std::runtime_error("logic interface details");}
        result.emplace_back(detail->DevicePath);
    }
    return result;
}
void require_ok(const logic::Reply &r) {if (r.error) throw std::runtime_error("device rejected operation, status="+std::to_string(r.error));}
double rate(const logic::Reply &r) {return r.divider256?double(r.system_clock)*128/r.divider256:0;}
void print(const logic::Reply &r) {
    std::cout<<"state="<<r.state<<" session="<<r.session<<" requested_hz="<<r.requested_rate<<" actual_hz="<<std::setprecision(12)<<rate(r)
             <<" samples="<<r.samples<<" flags="<<r.flags<<" trigger_index="<<r.trigger_index<<"\n";
}
std::uint32_t number(const char *s) {
    std::size_t pos=0;auto value=std::stoull(s,&pos,0);
    if (pos!=std::strlen(s) || value>0xffffffffULL) throw std::runtime_error("invalid numeric argument");
    return static_cast<std::uint32_t>(value);
}
logic::Reply arm(Device &d,int argc,char **argv,bool capture) {
    logic::Request request{};request.operation=logic::Arm;
    request.session=GetTickCount()^GetCurrentProcessId();if (!request.session) request.session=1;
    request.rate=number(argv[3]);request.samples=number(argv[4]);
    // arm arguments begin at argv[5], capture at argv[6] after prefix.
    if (!capture) {
        request.trigger=argc>5?number(argv[5]):0;request.channel=argc>6?number(argv[6]):0;request.timeout_ms=argc>7?number(argv[7]):5000;
    } else {
        request.trigger=argc>6?number(argv[6]):0;request.channel=argc>7?number(argv[7]):0;request.timeout_ms=argc>8?number(argv[8]):5000;
    }
    if (!logic::valid_arm(request)) throw std::runtime_error("invalid acquisition: 10k..10M Hz, 4..262144 samples multiple of 4, trigger 0/1/2, channel 0..7, timeout 100..30000 ms");
    auto reply=d.command(request);require_ok(reply);return reply;
}
void fetch(Device &d,const std::string &prefix) {
    logic::Request request{};request.operation=logic::Status;
    auto status=d.command(request);require_ok(status);print(status);
    if (status.state!=logic::Complete || status.flags || !status.samples || status.samples>logic::capacity || !status.divider256) throw std::runtime_error("capture not complete/valid");
    std::vector<std::uint8_t> data(status.samples);
    const auto started=std::chrono::steady_clock::now();
    for (std::uint32_t offset=0;offset<status.samples;) {
        request={};request.operation=logic::Fetch;request.session=status.session;request.rate=offset;
        request.samples=std::min<std::uint32_t>(4096,status.samples-offset);
        const auto reply=d.command(request);require_ok(reply);
        if (reply.session!=status.session || reply.offset!=offset || reply.payload_length!=request.samples) throw std::runtime_error("capture block header mismatch");
        d.read(data.data()+offset,reply.payload_length);
        if (logic::crc32(data.data()+offset,reply.payload_length)!=reply.payload_crc) throw std::runtime_error("capture block CRC mismatch");
        offset+=reply.payload_length;
    }
    const auto upload_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    const auto path=std::filesystem::path(prefix);if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::ofstream bin(prefix+".bin",std::ios::binary);bin.write(reinterpret_cast<const char *>(data.data()),data.size());bin.close();
    std::ofstream json(prefix+".json"),vcd(prefix+".vcd"),csv(prefix+".edges.csv");
    for (auto *out:{&json,&vcd,&csv}) out->exceptions(std::ios::badbit|std::ios::failbit);
    const auto fs=rate(status);
    json<<std::setprecision(15)<<"{\n  \"protocol\": 1,\n  \"serial\": \""<<d.serial<<"\",\n  \"session\": "<<status.session
        <<",\n  \"channels\": 8,\n  \"gpio_base\": 8,\n  \"samples\": "<<data.size()<<",\n  \"requested_rate_hz\": "<<status.requested_rate
        <<",\n  \"actual_rate_hz\": "<<fs<<",\n  \"system_clock_hz\": "<<status.system_clock<<",\n  \"divider256\": "<<status.divider256
        <<",\n  \"duration_s\": "<<double(data.size())/fs<<",\n  \"trigger_index\": "<<status.trigger_index<<",\n  \"flags\": "<<status.flags
        <<",\n  \"crc32\": "<<logic::crc32(data.data(),data.size())<<",\n  \"upload_ms\": "<<upload_ms<<",\n  \"channel_stats\": [\n";
    vcd<<"$timescale 1 ns $end\n$scope module logic $end\n";
    for (unsigned ch=0;ch<8;++ch) vcd<<"$var wire 1 "<<char('!'+ch)<<" CH"<<ch<<" $end\n";
    vcd<<"$upscope $end\n$enddefinitions $end\n";
    csv<<"sample,time_s,value_hex\n"<<std::setprecision(15);
    for (std::size_t i=0;i<data.size();++i) {
        if (i && data[i]==data[i-1]) continue;
        vcd<<'#'<<static_cast<std::uint64_t>(static_cast<long double>(i)*1000000000.0L/fs+0.5L)<<'\n';
        for (unsigned ch=0;ch<8;++ch) if (!i || ((data[i]^data[i-1])&(1U<<ch))) vcd<<((data[i]>>ch)&1)<<char('!'+ch)<<'\n';
        csv<<i<<','<<double(i)/fs<<",0x"<<std::hex<<unsigned(data[i])<<std::dec<<'\n';
    }
    vcd<<'#'<<static_cast<std::uint64_t>(static_cast<long double>(data.size())*1000000000.0L/fs+0.5L)<<'\n';
    for (unsigned ch=0;ch<8;++ch) {
        std::size_t edges=0,previous=0,intervals=0;std::uint64_t total=0;std::size_t minimum=data.size(),maximum=0;
        for (std::size_t i=1;i<data.size();++i) if ((data[i]^data[i-1])&(1U<<ch)) {
            if (edges) {const auto width=i-previous;minimum=std::min(minimum,width);maximum=std::max(maximum,width);total+=width;++intervals;}
            previous=i;++edges;
        }
        const double mean=intervals?double(total)/intervals/fs:0;
        json<<"    {\"channel\":"<<ch<<",\"edges\":"<<edges<<",\"mean_edge_interval_s\":"<<mean
            <<",\"min_edge_interval_s\":"<<(intervals?double(minimum)/fs:0)<<",\"max_edge_interval_s\":"<<(intervals?double(maximum)/fs:0)<<"}"<<(ch==7?"\n":",\n");
        std::cout<<"CH"<<ch<<" edges="<<edges<<" mean_edge_interval_s="<<mean<<"\n";
    }
    json<<"  ]\n}\n";
    if (!bin) throw std::runtime_error("BIN write failed");
    std::cout<<"CAPTURE_SAVED prefix="<<prefix<<" bytes="<<data.size()<<" upload_ms="<<upload_ms<<" CRC32="<<logic::crc32(data.data(),data.size())<<"\n";
}
}
int main(int argc,char **argv) {
    try {
        const std::string op=argc>1?argv[1]:"info";
        if (op!="info" && op!="capture" && op!="arm" && op!="status" && op!="fetch" && op!="cancel" && op!="selftest") throw std::runtime_error("unknown command");
        if (op!="info" && argc<3) throw std::runtime_error("explicit serial required");
        if ((op=="capture" && argc<6) || (op=="arm" && argc<5) || (op=="fetch" && argc!=4)) throw std::runtime_error("Usage: logic_cli capture SERIAL RATE SAMPLES PREFIX [TRIGGER CHANNEL TIMEOUT_MS] | arm SERIAL RATE SAMPLES [TRIGGER CHANNEL TIMEOUT_MS] | fetch SERIAL PREFIX | info/status/cancel/selftest SERIAL");
        unsigned matched=0;
        for (const auto &path:paths()) {
            Device d(path);if (argc>2 && d.serial!=argv[2]) continue;++matched;
            std::cout<<"Logic serial="<<d.serial<<"\n";
            logic::Request request{};request.operation=logic::Status;
            if (op=="arm" || op=="capture") {
                const auto start=arm(d,argc,argv,op=="capture");print(start);
                if (op=="capture") {
                    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
                    for (;;) {
                        auto s=d.command(request);require_ok(s);
                        if (s.session!=start.session) throw std::runtime_error("session replaced");
                        if (s.state==logic::Complete) break;
                        if (s.state==logic::Failed || s.state==logic::Cancelled || std::chrono::steady_clock::now()>deadline) {print(s);throw std::runtime_error("capture failed/cancelled/timed out");}
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    }
                    fetch(d,argv[5]);
                }
            } else if (op=="fetch") fetch(d,argv[3]);
            else if (op=="selftest") {
                request.operation=logic::Arm;request.session=1;request.rate=0;request.samples=0;
                if (d.command(request).error!=logic::Invalid) throw std::runtime_error("invalid arm accepted");
                request={};request.operation=logic::Fetch;request.session=0xffffffff;
                if (d.command(request).error!=logic::Stale) throw std::runtime_error("stale session accepted");
                std::cout<<"LOGIC_PROTOCOL_SELFTEST_PASSED\n";
            } else {
                request.operation=op=="cancel"?logic::Cancel:op=="info"?logic::Info:logic::Status;
                auto reply=d.command(request);require_ok(reply);print(reply);
            }
        }
        if (!matched) throw std::runtime_error("no matching logic analyzer interface");
        return 0;
    } catch (const std::exception &e) {std::cerr<<"Error: "<<e.what()<<"\n";return 1;}
}
