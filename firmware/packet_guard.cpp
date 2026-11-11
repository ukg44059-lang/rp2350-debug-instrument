#include "packet_guard.hpp"

namespace probe {
bool valid_packet(const std::uint8_t *p, std::size_t size) {
    if (!p || size == 0 || size > 64) return false;
    auto has = [size](std::size_t n) { return n <= size; };
    switch (p[0]) {
    case 0x00: return has(2); // Info
    case 0x01: return has(3); // HostStatus
    case 0x02: return has(2) && p[1] <= 1; // Connect: SWD or default
    case 0x03: case 0x07: case 0x0a: return true;
    case 0x04: // Keep CMSIS-DAP retries bounded while accepting pyOCD defaults.
        return has(6) && (p[2] | (p[3]<<8)) <= 150 && (p[4] | (p[5]<<8)) <= 16;
    case 0x05: { // Transfer: DP/AP requests may include match data or timestamps.
        if (!has(3)) return false;
        std::size_t pos=3, response=3;
        for (unsigned i=0; i<p[2]; ++i) {
            if (!has(pos+1)) return false;
            const auto req=p[pos++];
            if (req & 0xc0) return false; // timestamp / reserved
            if ((req & 0x02) && !(req & 0x10)) response+=4;
            if (!(req & 0x02) || (req & 0x10)) pos+=4;
            if (!has(pos) || response>64) return false;
        }
        return true;
    }
    case 0x06: { // TransferBlock
        if (!has(5)) return false;
        const unsigned count=p[2] | (p[3]<<8);
        if (p[4] & ~0x0f) return false;
        return (p[4]&2) ? 4+4*count<=64 : has(5+4*count);
    }
    case 0x08: return has(6); // WriteABORT
    case 0x09: return has(3) && (p[1] | (p[2]<<8)) <= 10000; // <=10ms
    case 0x10: { // SWJ Pins: no physical nRESET/nTRST available.
        if (!has(7) || (p[2] & ~0x83)) return false;
        if ((p[2]&0x80) && !(p[1]&0x80)) return false; // Allow harmless reset release only.
        const std::uint32_t wait=p[3] | (p[4]<<8) | (p[5]<<16) | (std::uint32_t(p[6])<<24);
        return wait==0; // No timestamp timer, so do not enter upstream pin-wait loop.
    }
    case 0x11: { // Clock: limited bring-up range, actual GPIO rate is approximate.
        if (!has(5)) return false;
        const std::uint32_t hz=p[1] | (p[2]<<8) | (p[3]<<16) | (std::uint32_t(p[4])<<24);
        return hz>=50000 && hz<=1000000;
    }
    case 0x12: return has(2) && has(2+(((p[1]?p[1]:256)+7)/8));
    case 0x13: return has(2);
    case 0x1d: { // SWD raw sequences
        if (!has(2)) return false;
        std::size_t pos=2,response=2;
        for (unsigned i=0;i<p[1];++i) {
            if (!has(pos+1)) return false;
            const auto info=p[pos++];
            const auto bits=(info&63)?(info&63):64;
            if (info&0x40) return false;
            if (info&0x80) response+=(bits+7)/8; else pos+=(bits+7)/8;
            if (!has(pos) || response>64) return false;
        }
        return true;
    }
    default: return false; // No batch, UART, SWO, JTAG or vendor DAP commands.
    }
}
}
