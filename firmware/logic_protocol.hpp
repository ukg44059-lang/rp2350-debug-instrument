#pragma once
#include <cstdint>
#include <cstddef>

namespace logic {
constexpr std::uint32_t magic=0x31414c41, version=1, capacity=256*1024;
enum Operation : std::uint32_t { Info=1, Arm=2, Status=3, Fetch=4, Cancel=5, StreamStart=6, StreamStop=7 };
enum State : std::uint32_t { Idle=0, Armed=1, Capturing=2, Complete=3, Cancelled=4, Failed=5 };
enum Error : std::uint32_t { Ok=0, Invalid=1, Busy=2, NotReady=3, Stale=4 };
enum Flag : std::uint32_t { FifoStall=1, DmaError=2, TimedOut=4, Overflow=8 };
constexpr std::uint32_t stream_magic=0x32534c41, stream_version=1, stream_block=4096;
enum StreamKind : std::uint32_t { StreamData=1, StreamEnd=2 };
enum Encoding : std::uint32_t { Raw=0, Rle=1 };
// Each RLE word: low byte = eight input levels; high 24 bits = repeat count.
// Samples are exact, with no decimation or transition filtering.
struct StreamHeader {
    std::uint32_t magic_word,protocol,kind,session,sequence,flags,system_clock,divider256;
    std::uint32_t samples,first_sample_lo,first_sample_hi,payload_length,payload_crc,ring_used,encoding,requested_rate;
};
static_assert(sizeof(StreamHeader)==64);
// All fields little-endian. One 64-byte request, one 64-byte reply; Fetch adds raw payload.
struct Request {
    std::uint32_t magic_word,operation,session,rate,samples,trigger,channel,timeout_ms;
    std::uint32_t reserved[8];
};
struct Reply {
    std::uint32_t magic_word,operation,error,state,session,requested_rate,system_clock,divider256;
    std::uint32_t samples,flags,trigger_index,payload_length,payload_crc,offset,capabilities,protocol;
};
static_assert(sizeof(Request)==64 && sizeof(Reply)==64);
inline std::uint32_t crc32(const std::uint8_t *data,std::size_t n) {
    std::uint32_t crc=0xffffffffU;
    while (n--) { crc^=*data++; for (unsigned i=0;i<8;++i) crc=(crc>>1)^(0xedb88320U & (0U-(crc&1U))); }
    return ~crc;
}
inline bool valid_arm(const Request &r) {
    return r.rate>=10000 && r.rate<=10000000 && r.samples>=4 && r.samples<=capacity &&
        r.samples%4==0 && r.trigger<=2 && r.channel<8 && r.timeout_ms>=100 && r.timeout_ms<=30000;
}
inline bool valid_stream(const Request &r) {
    return r.rate>=10000 && r.rate<=10000000 &&
        (!r.samples || (r.samples%stream_block==0 && r.samples<=128U*1024U*1024U)) &&
        r.trigger<=2 && r.channel<8 && r.timeout_ms>=100 && r.timeout_ms<=30000 &&
        r.reserved[0]<=Rle;
}
}
