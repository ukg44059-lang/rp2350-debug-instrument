#include "logic_protocol.hpp"
#include "stream_codec.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
void check(bool ok) {if (!ok) throw std::runtime_error("Stream codec/protocol test failed");}
int main() {
    alignas(4) std::array<std::uint8_t,4096> raw{};
    std::array<std::uint32_t,1024> encoded{};
    auto roundtrip=[&] {
        const auto n=logic::encode_runs(raw.data(),raw.size(),encoded.data());
        if (!n) return;
        check(n<raw.size());std::vector<std::uint8_t> decoded;
        for (unsigned j=0;j<n/4;++j) {
            const auto word=encoded[j];check((word>>8)>0);
            decoded.insert(decoded.end(),word>>8,std::uint8_t(word));
        }
        check(decoded.size()==raw.size() && !std::memcmp(decoded.data(),raw.data(),raw.size()));
    };
    check(logic::encode_runs(raw.data(),raw.size(),encoded.data())==4);roundtrip();
    for (unsigned i=0;i<raw.size();++i) raw[i]=i%2;
    check(!logic::encode_runs(raw.data(),raw.size(),encoded.data()));roundtrip();
    std::mt19937 rng(42);
    for (unsigned trial=0;trial<2000;++trial) {
        unsigned i=0;
        while (i<raw.size()) {auto value=std::uint8_t(rng());auto count=1+rng()%300;while(count-- && i<raw.size()) raw[i++]=value;}
        roundtrip();
    }
    logic::Request r{};r.rate=10000000;r.timeout_ms=5000;r.reserved[0]=logic::Rle;
    check(logic::valid_stream(r));r.samples=4095;check(!logic::valid_stream(r));
    r.samples=4096;check(logic::valid_stream(r));r.reserved[0]=2;check(!logic::valid_stream(r));
    std::cout<<"Streaming RLE roundtrips, fallback and limits: PASS\n";
}
