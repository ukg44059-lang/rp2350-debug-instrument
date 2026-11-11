#include "packet_guard.hpp"
#include <array>
#include <cassert>
#include <initializer_list>
#include <iostream>
#include <random>

bool valid(std::initializer_list<uint8_t> p) { return probe::valid_packet(p.begin(),p.size()); }
int main() {
    assert(!probe::valid_packet(nullptr,0));
    assert(!valid({0x00}));
    assert(valid({0x00,0xf0}));
    assert(valid({0x02,1}));
    assert(!valid({0x02,2})); // JTAG not advertised
    assert(valid({0x04,2,150,0,0,0})); // pyOCD default WAIT retry count
    assert(!valid({0x04,2,151,0,0,0})); // retries remain bounded
    assert(valid({0x05,0,1,2})); // DPIDR read
    assert(!valid({0x05,0,1,0})); // truncated write
    assert(valid({0x05,0,1,0,0,0,0,0}));
    assert(valid({0x05,0,1,0x20,0xff,0xff,0xff,0xff})); // match mask
    assert(!valid({0x05,0,1,0x82})); // unsupported timestamp
    assert(valid({0x06,0,15,0,2})); // full 64-byte response
    assert(!valid({0x06,0,16,0,2}));
    assert(!valid({0x06,0,0xff,0xff,2}));
    assert(!valid({0x06,0,1,0,0})); // truncated block write
    assert(valid({0x10,0x80,0x80,0,0,0,0})); // NRST release is no-op
    assert(!valid({0x10,0,0x80,0,0,0,0})); // unsupported physical reset
    assert(!valid({0x10,0,0,1,0,0,0})); // no pin wait
    assert(!valid({0x11,0,0,0,0})); // divide-by-zero clock
    assert(valid({0x11,0xa0,0x86,1,0})); // 100kHz
    assert(!valid({0x12,0})); // truncated 256-bit SWJ sequence
    assert(valid({0x12,8,0x9e}));
    assert(valid({0x1d,1,0x88})); // eight input bits
    assert(!valid({0x1d,1,8})); // missing output bits
    assert(!valid({0x7f,0})); // no unvalidated nested execute commands
    std::mt19937 random(407);
    for (unsigned n=0;n<100000;++n) {
        const auto size=random()%66;
        auto *data=new uint8_t[size];
        for (unsigned i=0;i<size;++i) data[i]=static_cast<uint8_t>(random());
        (void)probe::valid_packet(data,size);
        delete[] data;
    }
    std::cout<<"Packet boundary and protocol tests passed; 100000 fuzz packets checked.\n";
}
