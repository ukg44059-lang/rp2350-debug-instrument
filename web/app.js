import {StreamDecoder,EdgeStore} from './stream.js';
const $=id=>document.getElementById(id);
const colors=['#82e7ba','#8bb9ff','#eab879','#c7a0f5','#70d4e1','#ee9ab3','#d8d47e','#a7bdde'];
const targets=['B2','B13','B12','B17','B19','B21','B23','B25'];
let model=new EdgeStore(),active=false,paused=false,follow=true,needsPaint=true,span=.1,left=0,record='',cursorA=null,cursorB=null,lastFlags=0;
let visible=new Array(8).fill(true),viewEnd=0,wireTotal=0,renderAt=0,statsAt=-Infinity,modeLabel='就绪',streamAbort=null,usbWindow={at:performance.now(),bytes:0},pollBusy=false;
const canvas=$('wave'),ctx=canvas.getContext('2d');
function formatTime(seconds){const a=Math.abs(seconds);return a>=1?seconds.toFixed(3)+' s':a>=.001?(seconds*1e3).toFixed(a>=.1?1:3)+' ms':a>=.000001?(seconds*1e6).toFixed(2)+' µs':(seconds*1e9).toFixed(1)+' ns';}
function formatRate(rate){return rate>=1e6?(rate/1e6).toFixed(rate%1e6?2:0)+' MS/s':(rate/1000).toFixed(rate%1000?2:0)+' kS/s';}
function message(text,type='info'){$('message').className='message '+type;$('message').querySelector('div').textContent=text;}
function status(text,type=''){$('liveTag').textContent=text;$('liveTag').className='live-tag '+type;modeLabel=text;}
function enabled(){for(const id of ['rate','mode','encoding','trigger','triggerChannel','timeout','seconds','legacy','import','records'])$(id).disabled=active;$('start').disabled=active;$('stop').disabled=!active;$('refresh').disabled=active;$('exportVcd').disabled=!model.length;$('exportCsv').disabled=!model.length;$('exportLas').disabled=!record || active;}
function setFollow(value){follow=value;$('follow').classList.toggle('selected',value);needsPaint=true;}
for(let ch=0;ch<8;ch++) {
  const pin=document.createElement('div');pin.innerHTML=`<i style="background:${colors[ch]}"></i><b>CH${ch}</b> GP${ch+8} <span>${targets[ch]}</span>`;$('pinmap').append(pin);
  const row=document.createElement('div');row.className='channel-row';row.style.setProperty('--ch',colors[ch]);
  row.innerHTML=`<input type="checkbox" checked aria-label="显示 CH${ch}"><div class="ch-name">CH${ch}<span>GP${ch+8} → ${targets[ch]}</span></div><span class="value" id="level${ch}">—</span>`;
  row.querySelector('input').onchange=e=>{visible[ch]=e.target.checked;needsPaint=true;};$('channels').append(row);
  const option=document.createElement('option');option.value=ch;option.textContent='CH'+ch;$('triggerChannel').append(option);
}
async function detect(){try{const r=await fetch('/api/devices');if(!r.ok)throw Error('设备枚举失败');const {devices}=await r.json();const connected=devices.some(x=>x.serial==='0C36968AD31958CA');$('connection').className='connection '+(connected?'online':'');$('connection').innerHTML='<i></i>'+(connected?'RP2350 已连接':'未检测到 RP2350');if(!connected)message('未检测到 RP2350。可以先导入已有波形；连接 USB 后点击“重新检测”。');return connected;}catch(e){$('connection').textContent='本地服务未连接';message(e.message,'error');return false;}}
function resetData(){model=new EdgeStore();record='';wireTotal=0;lastFlags=0;cursorA=cursorB=null;left=0;viewEnd=0;paused=false;setFollow(true);$('pause').textContent='暂停显示';$('empty').hidden=true;$('integrity').textContent='接收并校验中';$('recordName').textContent='创建本地记录…';$('statBuffer').innerHTML='0 <em>%</em>';$('bufferBar').style.width='0%';$('sessionNote').textContent='无显示降采样、无毛刺过滤；缩小视图时显示像素内的高低电平范围。';usbWindow={at:performance.now(),bytes:0};needsPaint=true;enabled();}
function received(h,payload){wireTotal+=64+h.bytes;lastFlags=h.flags;
  if(h.kind===1)model.append(h,payload);
  const p=performance.now();
  // Store every sample/transition; limit DOM statistics work to ten updates/s.
  if(p-statsAt>=100 || h.kind===2 || model.total===4096){statsAt=p;$('clockChip').textContent=(h.clock/1e6).toFixed(0)+' MHz 系统时钟';$('statRate').textContent=formatRate(h.rate);$('statInterval').textContent='采样间隔 '+formatTime(1/h.rate);$('statSamples').textContent=model.total.toLocaleString();$('statDuration').textContent=(model.total/model.rate).toFixed(3)+' s';const used=Math.min(100,h.ring/262144*100);$('statBuffer').innerHTML=used.toFixed(1)+' <em>%</em>';$('bufferBar').style.width=used+'%';$('bufferBar').style.background=used>80?'#f0878c':'#7eebbb';}
  if(p-usbWindow.at>350){const bps=(wireTotal-usbWindow.bytes)/((p-usbWindow.at)/1000);$('statUsb').innerHTML=(bps/1e6).toFixed(2)+' <em>MB/s</em>';$('statRatio').textContent='原始等效 '+(h.rate/1e6).toFixed(2)+' MB/s · '+(h.encoding?'无损游程':'原始块');usbWindow={at:p,bytes:wireTotal};}
  if(!paused)needsPaint=true;
}
function flagMessage(flags){const parts=[];if(flags&1)parts.push('采样 FIFO 停顿 / DMA 服务延迟');if(flags&2)parts.push('DMA 错误');if(flags&4)parts.push('触发等待超时');if(flags&8)parts.push('USB 发送跟不上采样，设备环形缓冲已满');return parts.join('；')||'未知采集异常';}
async function start(){if(active)return;active=true;enabled();resetData();status('正在连接','active');$('sessionBadge').textContent='正在建立数据流';message('正在启动 PIO / DMA 采样，USB 将持续传输已完成的数据块。');
  streamAbort=new AbortController();const params=new URLSearchParams({rate:$('rate').value,samples:$('mode').value,encoding:$('encoding').value,trigger:$('trigger').value,channel:$('triggerChannel').value,timeout:$('timeout').value,seconds:$('seconds').value});
  try {
    const r=await fetch('/api/stream?'+params,{method:'POST',headers:{'Content-Type':'application/json'},body:'{}',signal:streamAbort.signal});
    if(!r.ok){const j=await r.json();throw Error(j.error||'设备启动失败');}
    status('采集中','active');$('sessionBadge').textContent='实时传输';message('采样与 USB 传输同时进行。网页显示最近的波形，完整无损记录持续保存到项目目录。','success');
    const decoder=new StreamDecoder(received),reader=r.body.getReader();
    while(true){const {done,value}=await reader.read();if(done)break;decoder.feed(value);}
    decoder.finish();
    if(lastFlags){status('异常停止','error');message(flagMessage(lastFlags)+'。已收到的原始数据仍保留；这次采集未通过完整性验收。','error');$('integrity').textContent='异常 · flags='+lastFlags;}
    else{status('采集完成');message(`采集结束，已接收 ${model.total.toLocaleString()} 点。数据块 CRC、序号和样点位置校验通过；这不替代输入波形质量验收。`,'success');$('integrity').textContent='传输校验通过';}
  }catch(e){status('连接异常','error');message(e.message,'error');$('integrity').textContent='未完整结束';try{await fetch('/api/stop',{method:'POST'});}catch{}}
  finally{active=false;streamAbort=null;$('sessionBadge').textContent=modeLabel;needsPaint=true;enabled();setTimeout(updateState,250);}
}
async function stop(){if(!active)return;$('stop').disabled=true;status('正在停止','active');try{const r=await fetch('/api/stop',{method:'POST'});if(!r.ok)throw Error('停止请求失败');}catch(e){message(e.message,'error');streamAbort?.abort();}}
async function updateState(){if(pollBusy)return;pollBusy=true;try{const r=await fetch('/api/state');const s=await r.json();$('serverStatus').textContent=location.host;if(s.record){record=s.record;$('recordName').textContent=s.record+'.las';}if(!active && s.error && s.record===record){$('sessionNote').textContent='最近采集状态：'+s.error;}enabled();}catch{$('serverStatus').textContent='连接中断';}finally{pollBusy=false;}}
$('start').onclick=start;$('stop').onclick=stop;$('refresh').onclick=detect;
$('rate').onchange=()=>{$('intervalHint').textContent='每 '+formatTime(1/Number($('rate').value))+' 采样一次';};
$('follow').onclick=()=>setFollow(!follow);$('pause').onclick=()=>{paused=!paused;if(paused)setFollow(false);$('pause').textContent=paused?'恢复显示':'暂停显示';needsPaint=true;};
$('windowSize').onchange=()=>{span=Number($('windowSize').value);needsPaint=true;};
function zoom(factor,ratio=.5){const width=span;span=Math.max(8/model.rate,Math.min(30,span*factor));left+=(width-span)*ratio;setFollow(false);needsPaint=true;}
$('zoomIn').onclick=()=>zoom(.5);$('zoomOut').onclick=()=>zoom(2);
$('fit').onclick=()=>{if(!model.total)return;span=Math.max(8/model.rate,(model.total-model.earliest)/model.rate);left=model.earliest/model.rate;setFollow(false);needsPaint=true;};
$('toggleSettings').onclick=()=>{$('settings').classList.toggle('visible');};
document.addEventListener('pointerdown',e=>{if(innerWidth<=700 && !$('settings').contains(e.target) && !$('toggleSettings').contains(e.target))$('settings').classList.remove('visible');});
new ResizeObserver(()=>{needsPaint=true;}).observe(canvas);
canvas.addEventListener('wheel',e=>{e.preventDefault();zoom(e.deltaY>0?1.25:.8,e.offsetX/canvas.clientWidth);},{passive:false});
let drag=null;
canvas.onpointerdown=e=>{drag={x:e.clientX,left,shift:e.shiftKey,moved:false};canvas.setPointerCapture(e.pointerId);};
canvas.onpointermove=e=>{if(!drag)return;const dx=e.clientX-drag.x;if(Math.abs(dx)>3)drag.moved=true;if(drag.moved){left=drag.left-dx/canvas.clientWidth*span;setFollow(false);needsPaint=true;}};
canvas.onpointerup=e=>{if(drag && !drag.moved && model.length){const at=left+Math.max(0,Math.min(1,e.offsetX/canvas.clientWidth))*span;if(drag.shift)cursorB=at;else cursorA=at;needsPaint=true;}drag=null;};
function niceStep(value){const base=10**Math.floor(Math.log10(value)),scaled=value/base;return(scaled>5?10:scaled>2?5:scaled>1?2:1)*base;}
function paint(){
  const width=Math.max(1,Math.round(canvas.clientWidth)),height=532,dpr=Math.min(devicePixelRatio||1,2);
  if(canvas.width!==Math.round(width*dpr)||canvas.height!==Math.round(height*dpr)){canvas.width=Math.round(width*dpr);canvas.height=Math.round(height*dpr);}
  ctx.setTransform(dpr,0,0,dpr,0,0);ctx.fillStyle='#0e1620';ctx.fillRect(0,0,width,height);
  if(follow && model.total)left=Math.max(model.earliest/model.rate,model.total/model.rate-span);
  const earliest=model.earliest/model.rate,latest=Math.max(earliest,model.total/model.rate);
  left=Math.max(earliest,Math.min(left,Math.max(earliest,latest-8/model.rate)));viewEnd=left+span;
  ctx.font='10px Consolas, monospace';ctx.textBaseline='middle';ctx.lineWidth=1;
  const tick=niceStep(span/(width/110));
  for(let t=Math.ceil(left/tick)*tick;t<viewEnd;t+=tick){const x=Math.round((t-left)/span*width)+.5;ctx.strokeStyle='#203042';ctx.beginPath();ctx.moveTo(x,36);ctx.lineTo(x,height);ctx.stroke();ctx.fillStyle='#6f879f';ctx.fillText(formatTime(t),x+5,18);}
  for(let ch=0;ch<8;ch++){const y=36+ch*62;ctx.fillStyle=ch%2?'#121d2a66':'#101a2566';ctx.fillRect(0,y,width,62);ctx.strokeStyle='#1d2b3c';ctx.beginPath();ctx.moveTo(0,y+61.5);ctx.lineTo(width,y+61.5);ctx.stroke();}
  let count=0;
  if(model.length){
    const lo=new Uint8Array(width);lo.fill(255);const hi=new Uint8Array(width);const filled=new Uint8Array(width);
    const startSample=left*model.rate,endSample=Math.min(viewEnd*model.rate,model.total);
    let i=model.find(startSample),value=model.value(i),at=startSample;
    while(at<endSample){
      const end=i+1<model.length?Math.min(model.time(i+1),endSample):endSample;
      const x0=Math.max(0,Math.min(width-1,Math.floor((at/model.rate-left)/span*width)));
      const x1=Math.max(x0,Math.min(width-1,Math.floor((end/model.rate-left)/span*width)));
      lo[x0]&=value;hi[x0]|=value;filled[x0]=1;
      if(x1>x0){lo.fill(value,x0+1,x1+1);hi.fill(value,x0+1,x1+1);filled.fill(1,x0+1,x1+1);}
      if(end>=endSample)break;i++;count++;value=model.value(i);at=end;
    }
    for(let ch=0;ch<8;ch++){
      $('level'+ch).textContent=(model.last>>ch)&1;const hz=model.frequency(ch);$('level'+ch).title=hz?(hz>=1000?(hz/1000).toFixed(3)+' kHz':hz.toFixed(3)+' Hz'):'周期不足';
      if(!visible[ch])continue;const high=36+ch*62+16,low=36+ch*62+45,bit=1<<ch;
      ctx.strokeStyle=colors[ch];ctx.globalAlpha=1;ctx.lineWidth=1.3;ctx.beginPath();let previous=null;
      for(let x=0;x<width;x++)if(filled[x]){const a=(hi[x]&bit)?high:low,b=(lo[x]&bit)?high:low;if(previous!==null)ctx.lineTo(x+.5,a);else ctx.moveTo(x+.5,a);if(a!==b)ctx.lineTo(x+.5,b);previous=b;}
      ctx.stroke();
    }
  }
  for(const [name,time,color] of [['A',cursorA,'#a7c8f7'],['B',cursorB,'#edba82']])if(time!==null && time>=left && time<=viewEnd){const x=(time-left)/span*width;ctx.strokeStyle=color;ctx.setLineDash([4,4]);ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,height);ctx.stroke();ctx.setLineDash([]);ctx.fillStyle=color;ctx.fillRect(x,0,18,17);ctx.fillStyle='#101820';ctx.fillText(name,x+5,9);}
  $('viewRange').textContent=formatTime(left)+' — '+formatTime(viewEnd)+' · '+formatTime(span)+'/屏';
  const preset=[...$('windowSize').options].find(o=>o.value!=='custom' && Math.abs(Number(o.value)-span)<span*1e-8);
  if(preset)$('windowSize').value=preset.value;else{$('windowSize').querySelector('[value="custom"]').textContent=formatTime(span)+' / 屏';$('windowSize').value='custom';}
  $('visibleCount').textContent=count.toLocaleString()+' 个跳变';
  if(cursorA!==null || cursorB!==null){const parts=[];if(cursorA!==null)parts.push('A '+formatTime(cursorA));if(cursorB!==null)parts.push('B '+formatTime(cursorB));if(cursorA!==null && cursorB!==null){const delta=Math.abs(cursorB-cursorA);parts.push('Δ '+formatTime(delta));if(delta)parts.push('1/Δ '+(1/delta).toPrecision(5)+' Hz');}$('cursorReadout').textContent=parts.join('  ·  ');}
  else $('cursorReadout').textContent='单击 A · Shift＋单击 B · 滚轮缩放 · 拖动平移';
  $('renderStatus').textContent=paused?'显示已暂停，采样继续':active?'实时更新 / '+model.length.toLocaleString()+' 段电平':'保留 '+model.length.toLocaleString()+' 段电平';
}
function animate(t){if(needsPaint && t-renderAt>45){paint();needsPaint=false;renderAt=t;}requestAnimationFrame(animate);}requestAnimationFrame(animate);
function download(data,name,type){const url=URL.createObjectURL(new Blob([data],{type}));const a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),2000);}
function exportRows(){if(!model.length)return[];const from=Math.max(model.earliest,Math.floor(left*model.rate)),to=Math.min(model.total,Math.ceil(viewEnd*model.rate));const i=model.find(from),rows=[[0,model.value(i)]];for(let n=i+1;n<model.length && model.time(n)<to;n++)rows.push([(model.time(n)-from)/model.rate,model.value(n)]);return rows;}
$('exportCsv').onclick=()=>{const rows=exportRows();download('time_s,value_hex,CH0,CH1,CH2,CH3,CH4,CH5,CH6,CH7\n'+rows.map(([t,v])=>[t.toPrecision(12),'0x'+v.toString(16).padStart(2,'0'),...Array.from({length:8},(_,i)=>(v>>i)&1)].join(',')).join('\n'),'pulse-edges.csv','text/csv');};
$('exportVcd').onclick=()=>{let text='$timescale 1 ns $end\n$scope module pulse $end\n';for(let ch=0;ch<8;ch++)text+=`$var wire 1 ${String.fromCharCode(33+ch)} CH${ch} $end\n`;text+='$upscope $end\n$enddefinitions $end\n';for(const [t,v]of exportRows()){text+='#'+Math.round(t*1e9)+'\n';for(let ch=0;ch<8;ch++)text+=((v>>ch)&1)+String.fromCharCode(33+ch)+'\n';}download(text,'pulse-waveform.vcd','text/plain');};
$('exportLas').onclick=()=>{if(record){const a=document.createElement('a');a.href='/api/record?id='+encodeURIComponent(record);a.download=record+'.las';a.click();}};
async function loadStream(stream,label){resetData();const decoder=new StreamDecoder(received);const reader=stream.getReader();while(true){const {done,value}=await reader.read();if(done)break;decoder.feed(value);}decoder.finish();status(lastFlags?'异常记录':'历史波形',lastFlags?'error':'');$('integrity').textContent=lastFlags?'异常 · flags='+lastFlags:'记录传输校验通过';$('sessionBadge').textContent='本地数据';message(lastFlags?flagMessage(lastFlags):'已载入 '+label+'；显示的是保存的真实采集数据。',lastFlags?'error':'success');$('fit').click();enabled();}
$('legacy').onclick=async()=>{try{const r=await fetch('/api/legacy');if(!r.ok)throw Error('此前波形文件不可用');resetData();model.loadRaw(new Uint8Array(await r.arrayBuffer()),10000000);status('历史波形');$('statRate').textContent='10 MS/s';$('statInterval').textContent='采样间隔 100 ns';$('statSamples').textContent=model.total.toLocaleString();$('statDuration').textContent=(model.total/model.rate).toFixed(6)+' s';$('integrity').textContent='历史文件 · 原有校验见报告';$('recordName').textContent='first-10M.bin';$('sessionBadge').textContent='真实历史数据';message('已打开此前 RT1064 八通道 25 kHz 逐路二分信号的真实采集；此操作没有启动新的采样。','success');$('fit').click();enabled();}catch(e){message(e.message,'error');}};
$('import').onclick=()=>$('files').click();
$('files').onchange=async e=>{try{const files=[...e.target.files],las=files.find(f=>f.name.endsWith('.las'));if(las){await loadStream(las.stream(),las.name);$('recordName').textContent=las.name;return;}const bin=files.find(f=>f.name.endsWith('.bin')),json=files.find(f=>f.name.endsWith('.json'));if(!bin)throw Error('请选择 BIN 文件，可同时选择其 JSON 元数据；或选择完整 LAS 记录。');let metadata={};if(json)metadata=JSON.parse(await json.text());resetData();const rate=metadata.actual_rate_hz||Number($('rate').value);model.loadRaw(new Uint8Array(await bin.arrayBuffer()),rate);$('statRate').textContent=formatRate(rate);$('statSamples').textContent=model.total.toLocaleString();$('statDuration').textContent=(model.total/rate).toFixed(6)+' s';$('integrity').textContent=metadata.flags?'原记录 flags='+metadata.flags:'导入原始数据';$('recordName').textContent=bin.name;status('导入波形');message(json?'已按 JSON 元数据中的采样率载入波形。':'未提供 JSON，使用当前设置的采样率 '+formatRate(rate)+'；请确认它与原始记录一致。');$('fit').click();enabled();}catch(e){message(e.message,'error');}finally{e.target.value='';}};
$('records').onclick=async()=>{const list=$('recordList');list.hidden=!list.hidden;if(list.hidden)return;list.textContent='正在读取…';try{const records=await(await fetch('/api/records')).json();list.textContent='';if(!records.length)list.textContent='还没有本机记录';for(const item of records.sort((a,b)=>b.id.localeCompare(a.id)).slice(0,30)){const b=document.createElement('button');if(item.metadata.error||item.metadata.flags)b.className='record-error';b.textContent=new Date(Number(item.id)).toLocaleTimeString()+' · '+(item.metadata.samples||0).toLocaleString()+' 点'+(item.metadata.error?' · 异常':'');b.onclick=async()=>{try{const r=await fetch('/api/record?id='+item.id);if(!r.ok)throw Error('记录不存在');await loadStream(r.body,item.id+'.las');record=item.id;$('recordName').textContent=item.id+'.las';enabled();}catch(e){message(e.message,'error');}};list.append(b);}}catch(e){list.textContent=e.message;}};
document.addEventListener('keydown',e=>{if(e.code==='Space' && e.target.tagName!=='SELECT' && e.target.tagName!=='INPUT'){e.preventDefault();$('pause').click();}});
setInterval(()=>{if(active)updateState();},750);detect();enabled();
