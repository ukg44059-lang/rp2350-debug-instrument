#include "logic_analyzer.hpp"
#include "logic_protocol.hpp"
#include "stream_codec.hpp"
#include "pico/stdlib.h"
#include "pico/critical_section.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "tusb.h"
#include <cstring>
#include <algorithm>

namespace logic {
namespace {
alignas(stream_block) std::uint32_t samples[capacity/4];
critical_section_t guard;
Reply shared{};
Request pending{};
bool has_command=false;
bool ready=false;
PIO const pio=pio1;
int sm=-1,dma=-1,dma_next=-1;
unsigned offset=0;
bool active=false;
std::uint64_t armed_at=0,capture_at=0;
Request current{};
Reply tx_header{};
bool header_pending=false;
std::uint32_t tx_offset=0,tx_remaining=0;
constexpr unsigned stream_slots=capacity/stream_block;
bool stream_mode=false,stream_capture=false,stream_done=false,stream_stop_requested=false;
bool stream_header_pending=false,stream_end_pending=false;
std::uint32_t produced=0,consumed=0,stream_flags=0;
Request stream_request{};
std::uint32_t stream_clock=0,stream_divider=0,stream_payload_size=0;
alignas(4) std::uint32_t stream_payload[stream_block/4];
alignas(4) std::uint8_t stream_frame[64+stream_block];
std::uint32_t stream_frame_used=0;
std::uint64_t stream_frame_at=0;
StreamHeader stream_header{};
std::uint64_t stream_first_sample=0;

Reply snapshot() {
    critical_section_enter_blocking(&guard); Reply r=shared; critical_section_exit(&guard); return r;
}
void publish(State state,std::uint32_t flags=0) {
    critical_section_enter_blocking(&guard); shared.state=state;shared.flags=flags;critical_section_exit(&guard);
}
void stop() {
    pio_sm_set_enabled(pio,sm,false);
    dma_channel_hw_addr(dma_next)->al1_ctrl &= ~DMA_CH0_CTRL_TRIG_EN_BITS;
    dma_channel_hw_addr(dma)->al1_ctrl &= ~DMA_CH0_CTRL_TRIG_EN_BITS;
    dma_channel_abort(dma);
    dma_channel_abort(dma_next);
    dma_hw->intr=(1U<<dma)|(1U<<dma_next);
    pio_interrupt_clear(pio,0);
    pio_sm_clear_fifos(pio,sm);
    active=false;
}
void finish_stream(std::uint32_t flags) {
    stop();stream_capture=false;
    critical_section_enter_blocking(&guard);
    stream_done=true;stream_flags=flags;shared.flags=flags;shared.state=flags?Failed:Complete;
    critical_section_exit(&guard);
}
void arm_stream_dma(int channel,int chain,std::uint32_t block,bool initial) {
    auto dc=dma_channel_get_default_config(channel);
    channel_config_set_transfer_data_size(&dc,DMA_SIZE_32);
    channel_config_set_read_increment(&dc,false);
    channel_config_set_write_increment(&dc,true);
    // A missed service deadline can only overwrite its own slot, never run
    // outside the allocation. Both pending completion bits detect that case.
    channel_config_set_ring(&dc,true,12);
    channel_config_set_chain_to(&dc,chain);
    channel_config_set_dreq(&dc,pio_get_dreq(pio,sm,false));
    dma_channel_configure(channel,&dc,samples+(block%stream_slots)*stream_block/4,
                          &pio->rxf[sm],stream_block/4,initial);
}
void start(const Request &r) {
    stop(); current=r;
    const auto clock=clock_get_hz(clk_sys);
    const auto div=static_cast<std::uint32_t>((std::uint64_t(clock)*256+r.rate)/(2*r.rate));
    std::uint32_t code[]={
        pio_encode_pull(false,true), pio_encode_mov(pio_x,pio_osr),
        pio_encode_nop(),pio_encode_nop(),
        pio_encode_in(pio_pins,8),pio_encode_jmp_x_dec(offset+4),
        pio_encode_irq_set(false,0),pio_encode_wait_irq(false,false,0)
    };
    if (r.trigger) {
        code[2]=pio_encode_wait_pin(r.trigger==2,r.channel);
        code[3]=pio_encode_wait_pin(r.trigger==1,r.channel);
    }
    for (unsigned i=0;i<8;++i) pio->instr_mem[offset+i]=code[i];
    auto config=pio_get_default_sm_config();
    sm_config_set_wrap(&config,offset,offset+7);
    sm_config_set_in_pins(&config,8);
    sm_config_set_in_shift(&config,true,true,32); // four chronological samples per word, oldest byte first
    // Keep TX available for the initial sample counter pull. RX is serviced by DMA.
    sm_config_set_fifo_join(&config,PIO_FIFO_JOIN_NONE);
    sm_config_set_clkdiv_int_frac8(&config,div/256,div%256);
    pio_sm_init(pio,sm,offset,&config);
    pio->fdebug=1U<<sm; // clear sticky RXSTALL, monitor before the finite program finishes
    const bool streaming=r.operation==StreamStart;
    stream_capture=streaming;
    pio_sm_put(pio,sm,streaming?0xffffffffU:r.samples-1);
    if (streaming) pio->instr_mem[offset+5]=pio_encode_jmp(offset+4);
    auto dc=dma_channel_get_default_config(dma);
    channel_config_set_transfer_data_size(&dc,DMA_SIZE_32);
    channel_config_set_read_increment(&dc,false);
    channel_config_set_write_increment(&dc,true);
    channel_config_set_dreq(&dc,pio_get_dreq(pio,sm,false));
    if (streaming) {
        stream_capture=true;
        arm_stream_dma(dma_next,dma,1,false);
        arm_stream_dma(dma,dma_next,0,true);
    } else dma_channel_configure(dma,&dc,samples,&pio->rxf[sm],r.samples/4,true);
    critical_section_enter_blocking(&guard);
    shared.session=r.session;shared.requested_rate=r.rate;shared.system_clock=clock;shared.divider256=div;
    shared.samples=r.samples;shared.flags=0;shared.trigger_index=0;shared.state=r.trigger?Armed:Capturing;
    critical_section_exit(&guard);
    armed_at=time_us_64();capture_at=r.trigger?0:armed_at;active=true;
    pio_sm_set_enabled(pio,sm,true);
}
}

void init() {
    critical_section_init(&guard);
    shared.magic_word=magic;shared.protocol=version;shared.capabilities=0x1f; // edges + streaming/RLE
    shared.samples=capacity;
}
void core1_entry() {
    sm=pio_claim_unused_sm(pio,true);dma=dma_claim_unused_channel(true);dma_next=dma_claim_unused_channel(true);
    const std::uint16_t blank[8]={};
    pio_program program{};program.instructions=blank;program.length=8;program.origin=-1;
    offset=pio_add_program(pio,&program);
    for (unsigned pin=8;pin<16;++pin) {gpio_init(pin);gpio_set_dir(pin,GPIO_IN);gpio_disable_pulls(pin);}
    critical_section_enter_blocking(&guard);ready=true;critical_section_exit(&guard);
    for (;;) {
        // Hardware captures independently. Avoid continually contesting the cross-core spinlock.
        busy_wait_us_32(10);
        Request command{};bool received=false;
        critical_section_enter_blocking(&guard);
        if (has_command) {command=pending;has_command=false;received=true;}
        critical_section_exit(&guard);
        if (received) {
            if (command.operation==Arm || command.operation==StreamStart) start(command);
            else {stop();stream_capture=false;publish(Cancelled);}
        }
        if (!active) {tight_loop_contents();continue;}
        const auto now=time_us_64();
        if (!capture_at && pio_sm_get_pc(pio,sm)>=offset+4) {capture_at=now;publish(Capturing);}
        if (stream_capture) {
            critical_section_enter_blocking(&guard);
            const bool requested_stop=stream_stop_requested;
            const auto next=produced,used=produced-consumed;
            critical_section_exit(&guard);
            if (requested_stop) {finish_stream(0);continue;}
            if (!capture_at && now-armed_at>std::uint64_t(current.timeout_ms)*1000) {
                finish_stream(TimedOut);continue;
            }
            const auto mask=(1U<<dma)|(1U<<dma_next);
            const auto completed=dma_hw->intr & mask;
            if (completed==mask || (pio->fdebug&(1U<<sm))) {
                finish_stream(FifoStall);continue;
            }
            const int channel=(next&1)?dma_next:dma;
            if (!(completed&(1U<<channel))) continue;
            dma_hw->intr=1U<<channel;
            const auto err=DMA_CH0_CTRL_TRIG_READ_ERROR_BITS|DMA_CH0_CTRL_TRIG_WRITE_ERROR_BITS;
            if (dma_channel_hw_addr(channel)->ctrl_trig&err) {finish_stream(DmaError);continue;}
            __dmb();
            critical_section_enter_blocking(&guard);++produced;critical_section_exit(&guard);
            if (current.samples && next+1>=current.samples/stream_block) {finish_stream(0);continue;}
            if (used+2>=stream_slots) {finish_stream(Overflow);continue;}
            arm_stream_dma(channel,channel==dma?dma_next:dma,next+2,false);
            continue;
        }
        if (pio_interrupt_get(pio,0) && !dma_channel_is_busy(dma)) {
            const auto flags=((pio->fdebug&(1U<<sm))?std::uint32_t(FifoStall):0U) |
                ((dma_channel_hw_addr(dma)->ctrl_trig & (DMA_CH0_CTRL_TRIG_READ_ERROR_BITS|DMA_CH0_CTRL_TRIG_WRITE_ERROR_BITS))?std::uint32_t(DmaError):0U);
            stop();publish(flags?Failed:Complete,flags);
        } else if ((!capture_at && now-armed_at>std::uint64_t(current.timeout_ms)*1000) ||
                   (capture_at && now-capture_at>std::uint64_t(current.samples)*1000000/current.rate+1000000)) {
            stop();publish(Failed,TimedOut);
        }
    }
}
void disconnected() {
    critical_section_enter_blocking(&guard);
    stream_stop_requested=true;
    pending={};pending.operation=Cancel;has_command=true;
    critical_section_exit(&guard);
    header_pending=false;tx_remaining=0;stream_mode=false;
}

namespace {
void stream_usb_task() {
    // StreamStop has no ordinary Reply: completion is an ordered End frame.
    if (tud_vendor_n_available(1)) {
        Request r{};const auto n=tud_vendor_n_read(1,&r,sizeof(r));
        if (n==64 && r.magic_word==magic && (r.operation==StreamStop || r.operation==Cancel) &&
            (!r.session || r.session==stream_request.session)) {
            critical_section_enter_blocking(&guard);stream_stop_requested=true;critical_section_exit(&guard);
        }
    }
    const bool end_staged=stream_end_pending && !stream_header_pending;
    const auto next_size=64+stream_payload_size;
    // Coalesce small compressed blocks for at most 2 ms. Avoid thousands of
    // short-packet host completions per second; raw blocks still go directly.
    // Keep End separate to guarantee its final four-byte short packet.
    const bool must_flush=stream_frame_used && (stream_frame_used>=2048 ||
        time_us_64()-stream_frame_at>=2000 || end_staged ||
        (stream_header_pending && (stream_end_pending || stream_frame_used+next_size>sizeof(stream_frame))));
    if(must_flush) {
        if(tud_vendor_n_write_available(1)>=stream_frame_used &&
           tud_vendor_n_write(1,stream_frame,stream_frame_used)==stream_frame_used) {
            stream_frame_used=0;
            if(end_staged) {stream_mode=false;stream_end_pending=false;}
        }
        return;
    }
    if (stream_header_pending) {
        if(!stream_frame_used) stream_frame_at=time_us_64();
        std::memcpy(stream_frame+stream_frame_used,&stream_header,64);
        std::memcpy(stream_frame+stream_frame_used+64,stream_payload,stream_payload_size);
        stream_frame_used+=next_size;
        stream_header_pending=false;stream_payload_size=0;
        return;
    }
    if(end_staged) return;
    critical_section_enter_blocking(&guard);
    const auto head=produced,tail=consumed,flags=stream_flags;
    const bool done=stream_done;
    critical_section_exit(&guard);
    if (head==tail && !done) return;
    stream_header={stream_magic,stream_version,head==tail?StreamEnd:StreamData,
        stream_request.session,tail,flags,stream_clock,stream_divider,0,
        std::uint32_t(stream_first_sample),std::uint32_t(stream_first_sample>>32),0,0,
        (head-tail)*stream_block,Raw,stream_request.rate};
    if (head==tail) {
        // A four-byte trailer makes the end frame terminate in a short USB
        // packet, completing already queued large host reads without a timeout.
        stream_payload[0]=0;stream_payload_size=4;stream_header.payload_length=4;
        stream_header.payload_crc=crc32(reinterpret_cast<const std::uint8_t*>(stream_payload),4);
        stream_end_pending=true;stream_header_pending=true;return;
    }
    const auto *raw=reinterpret_cast<const std::uint8_t*>(samples)+(tail%stream_slots)*stream_block;
    const auto compressed=stream_request.reserved[0]==Rle?encode_runs(raw,stream_block,stream_payload):0;
    if (compressed) {stream_payload_size=compressed;stream_header.encoding=Rle;}
    else {std::memcpy(stream_payload,raw,stream_block);stream_payload_size=stream_block;}
    stream_header.samples=stream_block;stream_header.payload_length=stream_payload_size;
    stream_header.payload_crc=crc32(reinterpret_cast<const std::uint8_t*>(stream_payload),stream_payload_size);
    stream_first_sample+=stream_block;
    // Only release a DMA slot after its exact data has been staged elsewhere.
    __dmb();
    critical_section_enter_blocking(&guard);++consumed;critical_section_exit(&guard);
    stream_header_pending=true;
}
}
void usb_task() {
    if (!tud_vendor_n_mounted(1)) return;
    if (header_pending) {
        if (tud_vendor_n_write_available(1)>=64 && tud_vendor_n_write(1,&tx_header,64)==64) {
            tud_vendor_n_write_flush(1);header_pending=false;
        }
        return;
    }
    if (tx_remaining) {
        const auto n=std::min<std::uint32_t>(tx_remaining,tud_vendor_n_write_available(1));
        if (n) {
            const auto sent=tud_vendor_n_write(1,reinterpret_cast<const std::uint8_t *>(samples)+tx_offset,n);
            tx_offset+=sent;tx_remaining-=sent;tud_vendor_n_write_flush(1);
        }
        return;
    }
    if (stream_mode) {stream_usb_task();return;}
    if (!tud_vendor_n_available(1)) return;
    Request r{};
    const auto length=tud_vendor_n_read(1,&r,sizeof(r));
    tx_header=snapshot();tx_header.operation=r.operation;tx_header.error=Ok;
    tx_header.payload_length=0;tx_header.payload_crc=0;tx_header.offset=0;
    critical_section_enter_blocking(&guard);
    if (length!=64 || r.magic_word!=magic || r.operation<Info || r.operation>StreamStop) tx_header.error=Invalid;
    else if (!ready || (has_command && r.operation!=Info && r.operation!=Status)) tx_header.error=Busy;
    else if (r.operation==Arm || r.operation==StreamStart) {
        if (!(r.operation==Arm?valid_arm(r):valid_stream(r)) || !r.session) tx_header.error=Invalid;
        else if (shared.state==Armed || shared.state==Capturing) tx_header.error=Busy;
        else {
            pending=r;has_command=true;shared.state=Armed;shared.session=r.session;
            shared.requested_rate=r.rate;shared.samples=r.samples;shared.flags=0;shared.divider256=0;
            tx_header.state=Armed;tx_header.session=r.session;tx_header.requested_rate=r.rate;
            tx_header.samples=r.samples;tx_header.flags=0;tx_header.divider256=0;
            if (r.operation==StreamStart) {
                stream_mode=true;stream_request=r;produced=consumed=0;stream_done=false;
                stream_flags=0;stream_stop_requested=false;stream_first_sample=0;
                stream_header_pending=stream_end_pending=false;stream_payload_size=0;
                stream_frame_used=0;stream_frame_at=0;
                stream_clock=clock_get_hz(clk_sys);
                stream_divider=(std::uint64_t(stream_clock)*256+r.rate)/(2*r.rate);
            }
        }
    } else if (r.operation==Cancel) {pending=r;has_command=true;}
    else if (r.operation==StreamStop) tx_header.error=NotReady;
    else if (r.operation==Fetch) {
        // rate/samples are offset/length for Fetch, max one 4-KiB block per request.
        if (r.session!=shared.session) tx_header.error=Stale;
        else if (shared.state!=Complete) tx_header.error=NotReady;
        else if (!r.samples || r.samples>4096 || r.rate>shared.samples || r.samples>shared.samples-r.rate) tx_header.error=Invalid;
        else {tx_offset=r.rate;tx_remaining=r.samples;tx_header.offset=r.rate;tx_header.payload_length=r.samples;}
    }
    critical_section_exit(&guard);
    if (tx_header.payload_length) tx_header.payload_crc=crc32(reinterpret_cast<const std::uint8_t *>(samples)+tx_offset,tx_remaining);
    header_pending=true;
}
}
