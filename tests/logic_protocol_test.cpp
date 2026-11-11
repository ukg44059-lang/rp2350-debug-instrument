#include "logic_protocol.hpp"
#include <iostream>
#include <stdexcept>
void expect(bool value) {if (!value) throw std::runtime_error("logic protocol regression");}
int main() {
    const auto *text=reinterpret_cast<const std::uint8_t *>("123456789");
    expect(logic::crc32(text,9)==0xcbf43926);
    expect(logic::crc32(text,0)==0);
    logic::Request r{};r.rate=100000;r.samples=262144;r.timeout_ms=5000;
    expect(logic::valid_arm(r));
    r.samples=262148;expect(!logic::valid_arm(r));
    r.samples=3;expect(!logic::valid_arm(r));
    r.samples=4;r.rate=10000000;expect(logic::valid_arm(r));
    r.rate=10000001;expect(!logic::valid_arm(r));
    r.rate=10000;r.trigger=2;r.channel=7;expect(logic::valid_arm(r));
    r.channel=8;expect(!logic::valid_arm(r));
    r.channel=0;r.trigger=3;expect(!logic::valid_arm(r));
    r.trigger=0;r.timeout_ms=0;expect(!logic::valid_arm(r));
    r.timeout_ms=30001;expect(!logic::valid_arm(r));
    std::cout<<"Logic protocol limits and standard CRC-32 vector: PASS\n";
}
