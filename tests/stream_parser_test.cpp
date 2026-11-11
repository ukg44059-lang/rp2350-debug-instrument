#include "stream_parser.hpp"
#include <iostream>
void expect(bool ok){if(!ok)throw std::runtime_error("Native parser regression");}
int main(){
    std::vector<std::uint8_t> data(4096,0x55),bytes;
    logic::StreamHeader h{logic::stream_magic,1,logic::StreamData,42,0,0,300000000,3840,4096,0,0,4096,logic::crc32(data.data(),data.size()),4096,logic::Raw,10000000};
    auto append=[&](const auto& header,const auto& payload){auto p=reinterpret_cast<const std::uint8_t*>(&header);bytes.insert(bytes.end(),p,p+64);bytes.insert(bytes.end(),payload.begin(),payload.end());};
    append(h,data);std::vector<std::uint8_t> end(4);
    h.kind=logic::StreamEnd;h.sequence=1;h.samples=0;h.first_sample_lo=4096;h.payload_length=4;h.payload_crc=logic::crc32(end.data(),4);append(h,end);
    for(unsigned split:{1,63,64,65,4096,16384}){logic::StreamParser parser(42);unsigned count=0;for(unsigned i=0;i<bytes.size();i+=split)parser.feed(bytes.data()+i,std::min<std::size_t>(split,bytes.size()-i),[&](const auto&,const auto*){++count;});expect(parser.ended && count==2);}
    bytes[70]^=1;bool failed=false;try{logic::StreamParser p(42);p.feed(bytes.data(),bytes.size(),[](const auto&,const auto*){});}catch(const std::runtime_error&){failed=true;}expect(failed);
    std::cout<<"Native streaming fragmented reads, ordering and CRC: PASS\n";
}
