#pragma once
#include <cstdint>
#include <cstddef>
namespace logic {
// Zero means raw is smaller. Input word-aligned; output capacity >= input size.
inline std::size_t encode_runs(const std::uint8_t* in,std::size_t size,std::uint32_t* out) {
    if (!size) return 0;
    std::size_t i=1,words=0,count=1;
    auto value=in[0];
    while (i<size) {
        if (!(i&3) && i+4<=size &&
            *reinterpret_cast<const std::uint32_t*>(in+i)==std::uint32_t(value)*0x01010101U) {
            count+=4;i+=4;continue;
        }
        if (in[i]==value) {++count;++i;continue;}
        if ((words+1)*4>=size) return 0;
        out[words++]=(std::uint32_t(count)<<8)|value;
        value=in[i++];count=1;
    }
    if ((words+1)*4>=size) return 0;
    out[words++]=(std::uint32_t(count)<<8)|value;
    return words*4;
}
}
