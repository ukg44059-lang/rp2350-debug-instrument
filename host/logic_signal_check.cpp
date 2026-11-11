// Check real RT1064 binary-counter captures, using measured PIO sampling rate.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

double field(const std::string &text, const std::string &key) {
    const auto at = text.find('"' + key + '"');
    if (at == std::string::npos) throw std::runtime_error("Missing metadata field: " + key);
    const auto colon = text.find(':', at);
    return std::stod(text.substr(colon+1));
}
int main(int argc, char **argv) {
    try {
        if (argc < 3) throw std::runtime_error("Usage: logic_signal_check PREFIX STEP_HZ [TRIGGER CHANNEL]");
        const std::string prefix = argv[1];
        const double step_hz = std::stod(argv[2]);
        std::ifstream meta(prefix+".json"), bin(prefix+".bin",std::ios::binary);
        if (!meta || !bin || !std::isfinite(step_hz) || step_hz<=0)
            throw std::runtime_error("Invalid files or step frequency");
        const std::string json((std::istreambuf_iterator<char>(meta)),{});
        const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(bin)),{});
        const double fs=field(json,"actual_rate_hz");
        if (data.empty() || field(json,"flags") || field(json,"samples")!=data.size())
            throw std::runtime_error("Capture flags/length failure");
        if (argc>=5) {
            const int trigger=std::stoi(argv[3]), ch=std::stoi(argv[4]);
            if ((trigger!=1 && trigger!=2) || ch<0 || ch>7 ||
                bool(data[0]&(1U<<ch)) != (trigger==1))
                throw std::runtime_error("Trigger first-sample level mismatch");
        }
        // Asynchronous GPIO edges can be sampled between individual pads switching.
        // Accept only a single sample mixing the exact old/new carry bits, followed
        // immediately by the next counter. Do not filter data used for edge checks.
        struct Run {std::size_t index,length;std::uint8_t value;};
        std::vector<Run> runs;
        for (std::size_t i=0;i<data.size();++i) {
            if (runs.empty() || data[i]!=runs.back().value) runs.push_back({i,1,data[i]});
            else ++runs.back().length;
        }
        unsigned changes=0, mixed_samples=0;
        for (std::size_t k=1;k<runs.size();++k) {
            const auto old=runs[k-1].value, next=std::uint8_t(old+1);
            if (runs[k].value!=next) {
                const auto changed=old^next;
                if (runs[k].length!=1 || k+1>=runs.size() || runs[k+1].value!=next ||
                    ((runs[k].value^old)&~changed))
                    throw std::runtime_error("Counter jump/glitch at sample "+std::to_string(runs[k].index));
                ++k;++mixed_samples;
            }
            ++changes;
        }
        if (changes<8) throw std::runtime_error("Insufficient counter transitions");
        std::cout<<std::setprecision(10)<<"prefix,channel,expected_hz,measured_hz,edges,min_half_samples,max_half_samples\n";
        for (unsigned ch=0;ch<8;++ch) {
            std::vector<std::size_t> edges;
            for (std::size_t i=1;i<data.size();++i)
                if ((data[i]^data[i-1])&(1U<<ch)) edges.push_back(i);
            const double expected=step_hz/double(2U<<ch);
            if (edges.size()<3) throw std::runtime_error("Insufficient edges on CH"+std::to_string(ch));
            const double half=fs/step_hz*double(1U<<ch);
            const double tolerance=2.0+0.02*half;
            std::size_t low=data.size(),high=0;
            for (std::size_t n=1;n<edges.size();++n) {
                const auto width=edges[n]-edges[n-1];
                low=std::min(low,width);high=std::max(high,width);
                if (std::abs(double(width)-half)>tolerance)
                    throw std::runtime_error("Bad half-period on CH"+std::to_string(ch));
            }
            const double measured=fs*double(edges.size()-1)/(2.0*double(edges.back()-edges.front()));
            // Mean over all captured edges; allow sampling quantization for short captures.
            if (std::abs(measured/expected-1)>0.005+2.0/double(edges.back()-edges.front()))
                throw std::runtime_error("Frequency mismatch on CH"+std::to_string(ch));
            std::cout<<prefix<<','<<ch<<','<<expected<<','<<measured<<','<<edges.size()<<','<<low<<','<<high<<'\n';
        }
        std::cout<<"SIGNAL_CAPTURE_PASSED changes="<<changes<<" mixed_bus_samples="<<mixed_samples<<" samples="<<data.size()<<" flags=0\n";
        return 0;
    } catch (const std::exception &e) { std::cerr<<"FAIL: "<<e.what()<<'\n';return 1; }
}
