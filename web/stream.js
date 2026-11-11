export const MAGIC = 0x32534c41;
const crcTable = new Uint32Array(256);
for (let i=0;i<256;i++) {let x=i;for(let b=0;b<8;b++) x=(x>>>1)^((x&1)?0xedb88320:0);crcTable[i]=x;}
export function crc32(bytes) {let c=0xffffffff;for(const b of bytes)c=(c>>>8)^crcTable[(c^b)&255];return (c^0xffffffff)>>>0;}
export class StreamDecoder {
  constructor(onFrame) {this.onFrame=onFrame;this.pending=new Uint8Array(0);this.first=0;this.sequence=0;this.session=null;this.ended=false;}
  feed(bytes) {
    const data=new Uint8Array(this.pending.length+bytes.length);data.set(this.pending);data.set(bytes,this.pending.length);let at=0;
    while(data.length-at>=64) {
      const v=new DataView(data.buffer,at,64),u=i=>v.getUint32(i*4,true);
      const h={kind:u(2),session:u(3),sequence:u(4),flags:u(5),clock:u(6),divider:u(7),samples:u(8),first:u(9)+u(10)*4294967296,bytes:u(11),crc:u(12),ring:u(13),encoding:u(14),requestedRate:u(15)};
      if(this.ended || u(0)!==MAGIC || u(1)!==1 || (h.kind!==1 && h.kind!==2) || h.sequence!==this.sequence || h.first!==this.first || h.bytes>4096 || !h.clock || !h.divider || (this.session!==null && h.session!==this.session)) throw Error('流序号或样点位置不连续，已停止接收');
      if(data.length-at<64+h.bytes) break;
      const payload=data.subarray(at+64,at+64+h.bytes);
      if(crc32(payload)!==h.crc) throw Error('USB 数据块 CRC 校验失败');
      h.rate=h.clock*128/h.divider;
      if(h.kind===1) {
        if(h.samples!==4096 || h.encoding>1 || !h.bytes) throw Error('采样块格式错误');
        if(h.encoding===0 && h.bytes!==h.samples) throw Error('原始样点数量错误');
        if(h.encoding===1) {
          if(h.bytes%4) throw Error('游程编码长度错误');
          const p=new DataView(payload.buffer,payload.byteOffset,payload.byteLength);let sum=0;
          for(let i=0;i<h.bytes;i+=4) {const count=p.getUint32(i,true)>>>8;if(!count || count>h.samples-sum)throw Error('游程编码样点数量错误');sum+=count;}
          if(sum!==h.samples)throw Error('游程编码样点总数错误');
        }
        this.sequence++;this.first+=h.samples;
      } else {if(h.samples || h.bytes!==4)throw Error('结束帧格式错误');this.ended=true;}
      this.session=h.session;this.onFrame(h,payload);at+=64+h.bytes;
    }
    this.pending=data.slice(at);
    if(this.pending.length>8320)throw Error('流接收缓冲异常');
  }
  finish() {if(!this.ended || this.pending.length)throw Error('数据流未完整结束；保留已接收数据供检查');}
}
export class EdgeStore {
  constructor(capacity=1048576) {
    this.capacity=capacity;this.times=new Float64Array(capacity);this.values=new Uint8Array(capacity);
    this.start=0;this.length=0;this.total=0;this.rate=10000000;this.last=-1;this.rises=Array.from({length:8},()=>[-1,-1]);
  }
  time(i){return this.times[(this.start+i)%this.capacity];}
  value(i){return this.values[(this.start+i)%this.capacity];}
  add(at,value) {
    if(value===this.last)return;
    if(this.last>=0)for(let ch=0;ch<8;ch++)if(((this.last^value)&value)&(1<<ch)){this.rises[ch][0]=this.rises[ch][1];this.rises[ch][1]=at;}
    if(this.length===this.capacity){this.start=(this.start+1)%this.capacity;this.length--;}
    const slot=(this.start+this.length)%this.capacity;this.times[slot]=at;this.values[slot]=value;this.length++;this.last=value;
  }
  append(h,payload) {
    this.rate=h.rate;let at=h.first;
    if(h.encoding===1) {
      const v=new DataView(payload.buffer,payload.byteOffset,payload.byteLength);
      for(let i=0;i<payload.length;i+=4){const run=v.getUint32(i,true);this.add(at,run&255);at+=run>>>8;}
    } else {
      for(let i=0;i<payload.length;i++)this.add(at+i,payload[i]);
    }
    this.total=h.first+h.samples;
    const cutoff=this.total-this.rate*30;
    while(this.length>1 && this.time(1)<cutoff){this.start=(this.start+1)%this.capacity;this.length--;}
  }
  loadRaw(bytes,rate){this.rate=rate;for(let i=0;i<bytes.length;i++)this.add(i,bytes[i]);this.total=bytes.length;}
  get earliest(){return this.length?Math.max(this.time(0),this.total-this.rate*30,0):0;}
  find(sample){let a=0,b=this.length;while(a<b){const m=(a+b)>>>1;if(this.time(m)<=sample)a=m+1;else b=m;}return Math.max(0,a-1);}
  frequency(ch){const [a,b]=this.rises[ch];return a>=0 && b>a?this.rate/(b-a):0;}
}
