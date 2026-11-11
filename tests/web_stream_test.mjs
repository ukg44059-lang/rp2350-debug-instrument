import assert from 'node:assert/strict';
import {StreamDecoder,EdgeStore,crc32,MAGIC} from '../web/stream.js';
assert.equal(crc32(new TextEncoder().encode('123456789')),0xcbf43926);
function frame(kind,seq,first,payload,encoding=0,flags=0){const data=new Uint8Array(64+payload.length),v=new DataView(data.buffer);const words=[MAGIC,1,kind,42,seq,flags,300000000,3840,kind===1?4096:0,first,0,payload.length,crc32(payload),8192,encoding,10000000];words.forEach((x,i)=>v.setUint32(i*4,x,true));data.set(payload,64);return data;}
const raw=new Uint8Array(4096);for(let i=0;i<4096;i++)raw[i]=i>>5;
const compressed=new Uint8Array(8);const cv=new DataView(compressed.buffer);cv.setUint32(0,(2048<<8)|0x55,true);cv.setUint32(4,(2048<<8)|0xaa,true);
const chunks=[frame(1,0,0,raw),frame(1,1,4096,compressed,1),frame(2,2,8192,new Uint8Array(4))];
const bytes=new Uint8Array(chunks.reduce((n,x)=>n+x.length,0));let at=0;for(const c of chunks){bytes.set(c,at);at+=c.length;}
for(const split of [1,7,63,64,65,512,4096,16384]){let frames=0;const store=new EdgeStore();const d=new StreamDecoder((h,p)=>{frames++;if(h.kind===1)store.append(h,p);});for(let i=0;i<bytes.length;i+=split)d.feed(bytes.subarray(i,i+split));d.finish();assert.equal(frames,3);assert.equal(store.total,8192);assert.equal(store.last,0xaa);}
const broken=bytes.slice();broken[70]^=1;assert.throws(()=>new StreamDecoder(()=>{}).feed(broken),/CRC/);
const missing=frame(1,1,4096,raw);assert.throws(()=>new StreamDecoder(()=>{}).feed(missing),/不连续/);
const zero=compressed.slice();zero.fill(0,0,4);assert.throws(()=>new StreamDecoder(()=>{}).feed(frame(1,0,0,zero,1)),/数量/);
const d=new StreamDecoder(()=>{});d.feed(chunks[0]);assert.throws(()=>d.finish(),/未完整/);
const small=new EdgeStore(8);small.loadRaw(new Uint8Array(Array.from({length:64},(_,i)=>i)),10000);assert.equal(small.length,8);assert.equal(small.time(0),56);assert.equal(small.find(60),4);
console.log('Browser CRC, fragmented stream, RLE, sequence, truncation and bounded history: PASS');
