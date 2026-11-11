#pragma once
#include "logic_protocol.hpp"
#include <cstring>
#include <functional>
#include <stdexcept>
#include <vector>
namespace logic {
class StreamParser {
    std::vector<std::uint8_t> pending_;
    std::uint64_t first_=0;
    std::uint32_t sequence_=0,session_;
public:
    bool ended=false;
    explicit StreamParser(std::uint32_t session):session_(session) {}
    void feed(const std::uint8_t* bytes,std::size_t size,
              const std::function<void(const StreamHeader&,const std::uint8_t*)>& accept) {
        pending_.insert(pending_.end(),bytes,bytes+size);std::size_t at=0;
        while (pending_.size()-at>=64) {
            StreamHeader h{};std::memcpy(&h,pending_.data()+at,64);
            const auto first=std::uint64_t(h.first_sample_lo)|(std::uint64_t(h.first_sample_hi)<<32);
            if (ended || h.magic_word!=stream_magic || h.protocol!=stream_version || h.session!=session_ ||
                h.sequence!=sequence_ || first!=first_ || h.payload_length>stream_block || !h.divider256 ||
                !h.system_clock || (h.kind!=StreamData && h.kind!=StreamEnd))
                throw std::runtime_error("Invalid stream header / missing or reordered samples");
            if (pending_.size()-at<64+h.payload_length) break;
            const auto* payload=pending_.data()+at+64;
            if (crc32(payload,h.payload_length)!=h.payload_crc) throw std::runtime_error("Streaming CRC mismatch");
            if (h.kind==StreamData) {
                if (h.samples!=stream_block || h.encoding>Rle || !h.payload_length) throw std::runtime_error("Invalid sample block");
                if (h.encoding==Raw && h.payload_length!=h.samples) throw std::runtime_error("Invalid raw length");
                if (h.encoding==Rle) {
                    if (h.payload_length%4) throw std::runtime_error("Invalid RLE length");
                    std::uint32_t total=0;
                    for (unsigned i=0;i<h.payload_length;i+=4) {
                        std::uint32_t word;std::memcpy(&word,payload+i,4);
                        if (!(word>>8) || (word>>8)>h.samples-total) throw std::runtime_error("Invalid RLE run");
                        total+=word>>8;
                    }
                    if (total!=h.samples) throw std::runtime_error("RLE sample count mismatch");
                }
                ++sequence_;first_+=h.samples;
            } else {
                if (h.samples || h.payload_length!=4) throw std::runtime_error("Invalid end frame");
                ended=true;
            }
            accept(h,payload);at+=64+h.payload_length;
        }
        if (at) pending_.erase(pending_.begin(),pending_.begin()+at);
        if (pending_.size()>2*stream_block+128) throw std::runtime_error("Unbounded stream input");
    }
};
}
