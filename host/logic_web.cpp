#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include "logic_usb.hpp"
#include "stream_parser.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
fs::path root;
int port=8765;
std::atomic<bool> running{false},stop_requested{false};
std::atomic<std::uint64_t> sample_count{0},wire_bytes{0},blocks{0},rle_blocks{0};
std::atomic<std::uint32_t> flags{0},max_ring{0},requested_rate{0};
std::mutex state_mutex;
std::string last_error,record_name,probe_serial;
Clock::time_point started;
std::string quote(const std::string& s) {
    std::string r="\"";
    for (unsigned char c:s) {
        if (c=='"' || c=='\\') {r+='\\';r+=c;}
        else if (c<32) r+=' ';
        else r+=c;
    }
    return r+'"';
}
std::string state_json() {
    std::lock_guard<std::mutex> lock(state_mutex);
    std::ostringstream s;
    s<<"{\"running\":"<<(running?"true":"false")<<",\"serial\":"<<quote(probe_serial)
     <<",\"samples\":"<<sample_count<<",\"wire_bytes\":"<<wire_bytes<<",\"blocks\":"<<blocks
     <<",\"rle_blocks\":"<<rle_blocks<<",\"flags\":"<<flags<<",\"max_ring_samples\":"<<max_ring
     <<",\"requested_rate_hz\":"<<requested_rate<<",\"record\":"<<quote(record_name)<<",\"error\":"<<quote(last_error)<<"}";
    return s.str();
}
void send_all(SOCKET socket,const void* data,std::size_t size) {
    auto* p=static_cast<const char*>(data);
    while(size) {const int n=send(socket,p,static_cast<int>(std::min<std::size_t>(size,1<<20)),0);if(n<=0) throw std::runtime_error("Browser connection closed / too slow");p+=n;size-=n;}
}
void response(SOCKET socket,int status,const std::string& type,const std::string& body) {
    const auto header="HTTP/1.1 "+std::to_string(status)+(status==200?" OK\r\n":" Error\r\n")+
        "Content-Type: "+type+"\r\nContent-Length: "+std::to_string(body.size())+
        "\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n";
    send_all(socket,header.data(),header.size());send_all(socket,body.data(),body.size());
}
std::string read_file(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);if(!in) throw std::runtime_error("File unavailable");
    return std::string(std::istreambuf_iterator<char>(in),{});
}
void file_response(SOCKET socket,const fs::path& path) {
    std::ifstream in(path,std::ios::binary);if(!in) throw std::runtime_error("File unavailable");
    const auto size=fs::file_size(path);
    const auto header="HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: "+std::to_string(size)+
        "\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n";
    send_all(socket,header.data(),header.size());
    std::array<char,65536> buffer{};
    auto remaining=size;
    while(remaining) {
        const auto count=static_cast<std::streamsize>(std::min<std::uintmax_t>(remaining,buffer.size()));
        if(!in.read(buffer.data(),count)) throw std::runtime_error("Capture file read failed");
        send_all(socket,buffer.data(),static_cast<std::size_t>(count));remaining-=count;
    }
}
struct Request {std::string method,path;std::map<std::string,std::string> query,headers;};
Request read_request(SOCKET socket) {
    std::string text;char buffer[4096];
    while(text.find("\r\n\r\n")==std::string::npos) {
        int n=recv(socket,buffer,sizeof(buffer),0);if(n<=0 || text.size()+n>16384) throw std::runtime_error("Invalid HTTP request");text.append(buffer,n);
    }
    Request r;std::istringstream in(text);std::string url,version,line;in>>r.method>>url>>version;std::getline(in,line);
    const auto q=url.find('?');r.path=url.substr(0,q);
    if(q!=std::string::npos) {
        std::istringstream fields(url.substr(q+1));
        while(std::getline(fields,line,'&')) {auto e=line.find('=');if(e!=std::string::npos) r.query[line.substr(0,e)]=line.substr(e+1);}
    }
    while(std::getline(in,line) && line!="\r") {
        auto c=line.find(':');if(c==std::string::npos) continue;
        auto name=line.substr(0,c);for(auto& ch:name) ch=static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        auto value=line.substr(c+1);while(!value.empty() && value.front()==' ') value.erase(0,1);if(!value.empty() && value.back()=='\r') value.pop_back();r.headers[name]=value;
    }
    const auto origin=r.headers.find("origin");
    const auto host=r.headers["host"];
    if(host!="127.0.0.1:"+std::to_string(port) && host!="localhost:"+std::to_string(port)) throw std::runtime_error("Invalid local service host");
    if(origin!=r.headers.end() && origin->second!="http://127.0.0.1:"+std::to_string(port) && origin->second!="http://localhost:"+std::to_string(port)) throw std::runtime_error("Cross-origin request rejected");
    if(r.headers["sec-fetch-site"]=="cross-site") throw std::runtime_error("Cross-site request rejected");
    return r;
}
std::uint32_t number(const Request& r,const std::string& name,std::uint32_t fallback) {
    const auto it=r.query.find(name);if(it==r.query.end()) return fallback;
    std::size_t n=0;auto value=std::stoull(it->second,&n);if(n!=it->second.size() || value>0xffffffffULL) throw std::runtime_error("Invalid numeric parameter");return static_cast<std::uint32_t>(value);
}
void chunk(SOCKET socket,const void* header,std::size_t header_size,const void* payload,std::size_t payload_size) {
    std::ostringstream s;s<<std::hex<<(header_size+payload_size)<<"\r\n";auto line=s.str();
    send_all(socket,line.data(),line.size());send_all(socket,header,header_size);send_all(socket,payload,payload_size);send_all(socket,"\r\n",2);
}
void capture(SOCKET socket,const Request& request) {
    logic::Request r{};r.operation=logic::StreamStart;r.session=GetTickCount()^GetCurrentProcessId();if(!r.session) r.session=1;
    r.rate=number(request,"rate",10000000);r.samples=number(request,"samples",0);
    r.trigger=number(request,"trigger",0);r.channel=number(request,"channel",0);r.timeout_ms=number(request,"timeout",5000);
    r.reserved[0]=number(request,"encoding",1);const auto seconds=number(request,"seconds",0);
    if(!logic::valid_stream(r) || seconds>3600) throw std::runtime_error("Invalid acquisition settings");
    if(running.exchange(true)) {response(socket,409,"application/json","{\"error\":\"Another acquisition is active\"}");return;}
    bool headers_sent=false,armed=false;
    std::unique_ptr<logic_usb::Device> device;
    try {
        stop_requested=false;sample_count=wire_bytes=blocks=rle_blocks=flags=max_ring=0;requested_rate=r.rate;
        started=Clock::now();
        {std::lock_guard<std::mutex> lock(state_mutex);last_error.clear();record_name.clear();}
        const auto devices=logic_usb::paths();if(devices.empty()) throw std::runtime_error("RP2350 logic interface not detected");
        // Match the known probe rather than silently choosing another board.
        for(const auto& path:devices) {auto d=std::make_unique<logic_usb::Device>(path);if(d->serial=="0C36968AD31958CA") {device=std::move(d);break;}}
        if(!device) throw std::runtime_error("Configured probe serial not detected");
        {std::lock_guard<std::mutex> lock(state_mutex);probe_serial=device->serial;}
        logic::Request info{};info.operation=logic::Info;const auto capabilities=device->command(info);
        if(!(capabilities.capabilities&8)) throw std::runtime_error("Streaming firmware 0.3.0 or newer is required");
        device->command(r);armed=true;
        fs::create_directories(root/"artifacts/web-captures");
        const auto id=std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        const auto path=root/"artifacts/web-captures"/(id+".las");
        std::ofstream file(path,std::ios::binary);if(!file) throw std::runtime_error("Cannot create capture file on G drive");
        {std::lock_guard<std::mutex> lock(state_mutex);record_name=id;}
        const std::string header="HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nTransfer-Encoding: chunked\r\nCache-Control: no-store\r\nConnection: close\r\nX-Accel-Buffering: no\r\n\r\n";
        send_all(socket,header.data(),header.size());headers_sent=true;
        logic_usb::Reader reader(*device);logic::StreamParser parser(r.session);
        std::vector<std::uint8_t> data;bool stop_sent=false;auto last_data=Clock::now();
        while(!parser.ended) {
            if((stop_requested || (seconds && Clock::now()-started>std::chrono::seconds(seconds))) && !stop_sent) {
                logic::Request stop{};stop.operation=logic::StreamStop;stop.session=r.session;device->write(stop);stop_sent=true;
            }
            if(!reader.next(data)) {
                if(Clock::now()-last_data>std::chrono::seconds(35)) throw std::runtime_error("No streaming data / trigger timed out");
                continue;
            }
            if(data.empty()) continue;
            last_data=Clock::now();
            parser.feed(data.data(),data.size(),[&](const logic::StreamHeader& h,const std::uint8_t* payload) {
                file.write(reinterpret_cast<const char*>(&h),64);file.write(reinterpret_cast<const char*>(payload),h.payload_length);
                if(!file) throw std::runtime_error("Capture file write failed");
                wire_bytes+=64+h.payload_length;flags=h.flags;
                if(h.ring_used>max_ring) max_ring=h.ring_used;
                if(h.kind==logic::StreamData) {sample_count+=h.samples;++blocks;if(h.encoding==logic::Rle) ++rle_blocks;}
                chunk(socket,&h,64,payload,h.payload_length);
            });
        }
        file.close();armed=false;send_all(socket,"0\r\n\r\n",5);
        {std::lock_guard<std::mutex> lock(state_mutex);if(flags) last_error="Device ended acquisition with flags="+std::to_string(flags.load());}
    } catch(const std::exception& e) {
        if(armed && device) {
            try {
                logic::Request stop{};stop.operation=logic::StreamStop;stop.session=r.session;device->write(stop);
                // Every small compressed batch may end with a short packet;
                // that does not mean the stream's End frame has been drained.
                // Continue until two quiet intervals, including all batches.
                ULONG drain_timeout=150;
                WinUsb_SetPipePolicy(device->usb,0x84,PIPE_TRANSFER_TIMEOUT,sizeof(drain_timeout),&drain_timeout);
                std::array<UCHAR,16384> sink{};ULONG n=0;
                unsigned quiet=0;const auto deadline=Clock::now()+std::chrono::seconds(3);
                while(quiet<2 && Clock::now()<deadline) {
                    if(WinUsb_ReadPipe(device->usb,0x84,sink.data(),sink.size(),&n,nullptr)) quiet=0;
                    else ++quiet;
                }
            } catch(...) {}
        }
        {std::lock_guard<std::mutex> lock(state_mutex);last_error=e.what();}
        if(!headers_sent) response(socket,503,"application/json","{\"error\":"+quote(e.what())+"}");
        std::cerr<<"capture: "<<e.what()<<std::endl;
    }
    auto state=state_json();
    state.replace(state.find("\"running\":true"),14,"\"running\":false");
    std::string id;{std::lock_guard<std::mutex> lock(state_mutex);id=record_name;}
    if(!id.empty()) {std::ofstream meta(root/"artifacts/web-captures"/(id+".json"));meta<<state;}
    running=false;
}
bool safe_id(const std::string& id) {return !id.empty() && id.size()<40 && id.find_first_not_of("0123456789")==std::string::npos;}
void handle(SOCKET socket) {
    bool answered=false;
    try {
        auto r=read_request(socket);
        if(r.path=="/api/stream" && r.method=="POST") {answered=true;capture(socket,r);}
        else if(r.path=="/api/stop" && r.method=="POST") {stop_requested=true;response(socket,200,"application/json","{\"ok\":true}");}
        else if(r.path=="/api/state") response(socket,200,"application/json",state_json());
        else if(r.path=="/api/devices") {
            std::string body="{\"devices\":[";bool first=true;
            if(!running) for(const auto& path:logic_usb::paths()) {
                try {logic_usb::Device d(path);logic::Request q{};q.operation=logic::Info;auto v=d.command(q);
                    if(!first) body+=',';
                    first=false;body+="{\"serial\":"+quote(d.serial)+",\"capabilities\":"+std::to_string(v.capabilities)+"}";
                } catch(...) {}
            }
            response(socket,200,"application/json",body+"]}");
        }
        else if(r.path=="/api/records") {
            std::string body="[";bool first=true;const auto dir=root/"artifacts/web-captures";
            if(fs::exists(dir)) for(const auto& entry:fs::directory_iterator(dir)) if(entry.path().extension()==".json" && safe_id(entry.path().stem().string())) {
                if(!first) body+=',';
                first=false;body+="{\"id\":"+quote(entry.path().stem().string())+",\"metadata\":"+read_file(entry.path())+"}";
            }
            response(socket,200,"application/json",body+"]");
        }
        else if(r.path=="/api/record" && safe_id(r.query["id"])) {answered=true;file_response(socket,root/"artifacts/web-captures"/(r.query["id"]+".las"));}
        else if(r.path=="/api/legacy") response(socket,200,"application/octet-stream",read_file(root/"artifacts/overclock-300/first-10M.bin"));
        else {
            const std::map<std::string,std::pair<std::string,std::string>> files={
                {"/",{"index.html","text/html; charset=utf-8"}},{"/app.js",{"app.js","text/javascript; charset=utf-8"}},
                {"/style.css",{"style.css","text/css; charset=utf-8"}},{"/stream.js",{"stream.js","text/javascript; charset=utf-8"}}};
            auto it=files.find(r.path);
            if(it==files.end()) response(socket,404,"text/plain","Not found");
            else response(socket,200,it->second.second,read_file(root/"web"/it->second.first));
        }
    } catch(const std::exception& e) {if(!answered) try {response(socket,400,"application/json","{\"error\":"+quote(e.what())+"}");} catch(...) {}}
    shutdown(socket,SD_BOTH);closesocket(socket);
}
int main(int argc,char** argv) {
    try {
        root=fs::current_path();
        for(int i=1;i<argc;++i) {std::string a=argv[i];if(a=="--port" && i+1<argc) port=std::stoi(argv[++i]);else if(a=="--root" && i+1<argc) root=fs::absolute(argv[++i]);else throw std::runtime_error("Usage: logic_web [--port 8765] [--root PROJECT]");}
        if(port<1024 || port>65535 || !fs::exists(root/"web/index.html")) throw std::runtime_error("Invalid port or project root");
        WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)) throw std::runtime_error("Winsock startup failed");
        SOCKET server=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(server==INVALID_SOCKET) throw std::runtime_error("Socket failed");
        BOOL exclusive=TRUE;setsockopt(server,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<char*>(&exclusive),sizeof(exclusive));
        sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(static_cast<u_short>(port));inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
        if(bind(server,reinterpret_cast<sockaddr*>(&addr),sizeof(addr)) || listen(server,16)) throw std::runtime_error("Local port is already in use");
        std::cout<<"Logic analyzer: http://127.0.0.1:"<<port<<"/\nProject: "<<root.string()<<std::endl;
        for(;;) {
            SOCKET client=accept(server,nullptr,nullptr);if(client==INVALID_SOCKET) continue;
            DWORD timeout=3000;setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout));setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout));
            BOOL nodelay=TRUE;setsockopt(client,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<char*>(&nodelay),sizeof(nodelay));
            std::thread(handle,client).detach();
        }
    } catch(const std::exception& e) {std::cerr<<e.what()<<std::endl;return 1;}
}
