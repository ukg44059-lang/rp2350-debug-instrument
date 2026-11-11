import fs from 'node:fs';
import {StreamDecoder} from '../web/stream.js';
const results=[];
for(const [name,rate,encoding,seconds,extra] of [
 ['raw-500k',500000,0,3,{}],['raw-750k',750000,0,3,{}],['raw-1M',1000000,0,3,{}],
 ['raw-10M-overflow',10000000,0,3,{}],['rle-10M',10000000,1,5,{}],
 ['rle-10M-rising',10000000,1,2,{trigger:1,channel:0}],
 ['rle-10M-falling',10000000,1,2,{trigger:2,channel:7}],
 ['finite-1M-points',10000000,1,0,{samples:1048576}]
]) {
 const at=performance.now();let firstAt=0,lastAt=0,samples=0,wire=0,blocks=0,rle=0,endFlags=null,maxRing=0;
 const params=new URLSearchParams({rate,encoding,seconds,timeout:5000,...extra});
 const response=await fetch('http://127.0.0.1:8765/api/stream?'+params,{method:'POST'});
 if(!response.ok)throw Error(await response.text());
 const decoder=new StreamDecoder((h)=>{const now=performance.now();if(!firstAt)firstAt=now;lastAt=now;wire+=64+h.bytes;maxRing=Math.max(maxRing,h.ring);if(h.kind===1){samples+=h.samples;blocks++;rle+=h.encoding;}else endFlags=h.flags;});
 for await(const bytes of response.body)decoder.feed(bytes);
 decoder.finish();
 const wall=(performance.now()-at)/1000;
 const state=await(await fetch('http://127.0.0.1:8765/api/state')).json();
 const result={name,rate,encoding,wall_seconds:wall,sampled_seconds:samples/rate,samples,wire_bytes:wire,blocks,rle_blocks:rle,flags:endFlags,max_ring_samples:maxRing,wall_MBps:wire/1e6/wall,receive_MBps:wire/1e6/Math.max((lastAt-firstAt)/1000,.001),record:state.record};
 results.push(result);console.log(JSON.stringify(result));
 await new Promise(r=>setTimeout(r,150));
}
fs.writeFileSync('artifacts/streaming/benchmark.json',JSON.stringify(results,null,2));
