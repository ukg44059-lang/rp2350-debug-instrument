param([ValidateSet(150,300)][int]$ClockMHz=300)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:TEMP="$projectRoot\.tmp"
$env:TMP=$env:TEMP
$env:PYTHONPATH="$projectRoot\tools\pyocd"
$env:PYTHONDONTWRITEBYTECODE='1'
$env:PROBE_TEST_CLOCK="$ClockMHz"
$python='C:\Users\666\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
New-Item -ItemType Directory -Force "artifacts/overclock-$ClockMHz" | Out-Null
& ./artifacts/probe_cli.exe selftest 0C36968AD31958CA
if ($LASTEXITCODE -ne 0) {throw 'DAP selftest failed'}
& ./artifacts/logic_cli.exe selftest 0C36968AD31958CA
if ($LASTEXITCODE -ne 0) {throw 'Logic selftest failed'}
try {
    & ./tools/set-rt1064-signal-rate.ps1 -StepHz 50000
    @'
import json,os,struct,subprocess,threading,time,zlib
from pathlib import Path
from elftools.elf.elffile import ELFFile
from pyocd.core.helpers import ConnectHelper
clock=int(os.environ['PROBE_TEST_CLOCK'])*1000000
out=Path('artifacts')/('overclock-'+os.environ['PROBE_TEST_CLOCK'])
serial='0C36968AD31958CA'
with open('artifacts/rt1064/rt1064-signals.elf','rb') as fp:
    syms={s.name:s['st_value'] for s in ELFFile(fp).get_section_by_name('.symtab').iter_symbols()}
rows=[];results=[];failures=[]
stop=threading.Event();ready=threading.Event()
def run(args):
    p=subprocess.run([str(a) for a in args],capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
    if p.returncode: raise RuntimeError(p.stdout+'\n'+p.stderr)
    return p.stdout
with ConnectHelper.session_with_chosen_probe(unique_id=serial,target_override='mimxrt1064',frequency=100000,connect_mode='attach',auto_unlock=False,resume_on_disconnect=False) as s:
    target=s.target
    assert target.read32(syms['demo_signature'])==0x1064a008
    counter_before=target.read32(syms['signal_count'])
    # libusb on Windows opens composite interfaces exclusively. Share pyOCD's
    # existing device in this test instead of opening a second native handle.
    dev=s.probe._link._interface.dev
    def read_exact(n):
        data=bytearray()
        while len(data)<n:
            chunk=dev.read(0x84,n-len(data),timeout=5000).tobytes()
            if not chunk: raise RuntimeError('Empty logic USB read')
            data.extend(chunk)
        return bytes(data)
    def command(op,session=0,rate=0,samples=0,trigger=0,channel=0):
        req=struct.pack('<16I',0x31414c41,op,session,rate,samples,trigger,channel,5000,*([0]*8))
        assert dev.write(0x04,req,timeout=5000)==64
        reply=struct.unpack('<16I',read_exact(64))
        assert reply[0]==0x31414c41 and reply[1]==op and reply[2]==0 and reply[15]==1,reply
        return reply
    def capture(n,rate,tr,ch,prefix):
        sid=0x300000+n
        command(2,sid,rate,262144,tr,ch)
        states=[];deadline=time.perf_counter()+6
        while True:
            r=command(3)
            states.append({'time_s':time.perf_counter(),'state':r[3]})
            assert r[4]==sid and r[9]==0 and r[3] not in (4,5),r
            if r[3]==3: break
            assert time.perf_counter()<deadline,'Capture timeout'
            time.sleep(.001)
        data=bytearray();upload_start=time.perf_counter()
        while len(data)<r[8]:
            count=min(4096,r[8]-len(data))
            h=command(4,sid,len(data),count)
            assert h[4]==sid and h[13]==len(data) and h[11]==count
            block=read_exact(count)
            assert zlib.crc32(block)==h[12],'Block CRC mismatch'
            data.extend(block)
        meta={'system_clock_hz':r[6],'actual_rate_hz':r[6]*128/r[7],'divider256':r[7],'samples':r[8],'flags':r[9],'duration_s':r[8]/rate,'requested_rate_hz':rate,'channels':8,'crc32':zlib.crc32(data),'upload_ms':(time.perf_counter()-upload_start)*1000}
        prefix.with_suffix('.bin').write_bytes(data)
        prefix.with_suffix('.json').write_text(json.dumps(meta,indent=2))
        prefix.with_suffix('.states.json').write_text(json.dumps(states,indent=2))
        return meta
    def read_swd():
        try:
            while not stop.is_set():
                a=time.perf_counter()
                cpuid=target.read32(0xe000ed00)
                assert cpuid==0x411fc271
                rows.append({'start_s':a,'end_s':time.perf_counter(),'cpuid':hex(cpuid)})
                ready.set()
        except BaseException as ex:
            failures.append(repr(ex));ready.set()
    worker=threading.Thread(target=read_swd)
    worker.start()
    try:
        assert ready.wait(5) and not failures,'SWD worker failed to start'
        cases=[(1000000,1,0)]+[(10000000,tr,ch) for ch in range(8) for tr in (1,2)]+[(10000000,0,0)]
        for n,(rate,tr,ch) in enumerate(cases):
            prefix=out/('capture-%02d-%d-t%d-ch%d'%(n,rate,tr,ch))
            start=time.perf_counter()
            meta=capture(n,rate,tr,ch,prefix)
            end=time.perf_counter()
            check=run(['artifacts/logic_signal_check.exe',prefix,50000,*([tr,ch] if tr else [])])
            prefix.with_suffix('.log').write_text(check)
            assert meta['system_clock_hz']==clock and meta['actual_rate_hz']==rate
            assert meta['flags']==0 and meta['samples']==262144
            assert not failures,failures
            during=[r for r in rows if start<=r['start_s'] and r['end_s']<=end]
            assert during,'No concurrent SWD reads during capture command'
            results.append({'prefix':str(prefix),'rate_hz':rate,'trigger':tr,'channel':ch,'samples':262144,'flags':0,'host_elapsed_s':end-start,'swd_reads_during_command':len(during),'upload_ms':meta['upload_ms']})
            print('PASS',prefix,'SWD reads during command=',len(during),flush=True)
    finally:
        stop.set();worker.join(10)
        assert not worker.is_alive(),'SWD worker did not stop'
        (out/'concurrent-swd.json').write_text(json.dumps({'reads':rows,'failures':failures},indent=2))
    counter_after=target.read32(syms['signal_count'])
assert not failures,failures
assert counter_after>counter_before
summary={'system_clock_hz':clock,'runs':results,'total_samples':sum(r['samples'] for r in results),'swd_read_iterations':len(rows),'note':'Same-process shared libusb handle for concurrent SWD and logic endpoints. See states timestamps for observed acquisition windows; no per-sample SWD timestamp.'}
(out/'test-summary.json').write_text(json.dumps(summary,indent=2))
print('OVERCLOCK_CAPTURE_PASSED runs=%d samples=%d swd_iterations=%d'%(len(results),summary['total_samples'],len(rows)))
'@ | & $python -
    if ($LASTEXITCODE -ne 0) {throw 'Overclock capture test failed'}
} finally {
    & ./tools/set-rt1064-signal-rate.ps1 -StepHz 1000
}
