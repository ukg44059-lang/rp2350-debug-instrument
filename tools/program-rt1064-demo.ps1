param([switch]$RestoreOriginal,[switch]$SignalTest)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:TEMP = "$projectRoot\.tmp"
$env:TMP = $env:TEMP
$env:PYTHONPATH = "$projectRoot\tools\pyocd"
$env:PYTHONDONTWRITEBYTECODE = '1'
$env:RT1064_RESTORE = if ($RestoreOriginal) {'1'} else {'0'}
$env:RT1064_SIGNALS = if ($SignalTest) {'1'} else {'0'}
$python = 'C:\Users\666\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
# Python is the vendor tool API only; target and probe firmware are C++17.
@'
from pathlib import Path
import hashlib, json, os, time, datetime
from pyocd.core.helpers import ConnectHelper
from pyocd.core.target import Target
root=Path('artifacts/rt1064')
original=(root/'backup/original-flash.bin').read_bytes()
expected='864882e1d468a62d0ffd190769a9a9e3089f1a6c825108a3b630652987ef4253'
assert len(original)==0x400000 and hashlib.sha256(original).hexdigest()==expected
assert (root/'backup/original-flash-second.bin').read_bytes()==original
restore=os.environ['RT1064_RESTORE']=='1'
signals=os.environ['RT1064_SIGNALS']=='1' and not restore
output=root/('signals' if signals else '.')
output.mkdir(exist_ok=True)
image=(root/('rt1064-signals.bin' if signals else 'rt1064-b9-blink.bin')).read_bytes()
assert len(image)<0x10000 and image[:512]==original[:512]
payload=bytearray(original[:0x10000])
if not restore:
    payload[:len(image)]=image
    assert int.from_bytes(payload[0x100c:0x1010],'little')==0, 'SDRAM DCD must be absent'
(output/('restore-sector.bin' if restore else 'program-sector.bin')).write_bytes(payload)
times={}
begin=time.perf_counter()
with ConnectHelper.session_with_chosen_probe(unique_id='0C36968AD31958CA',target_override='mimxrt1064',frequency=1000000,connect_mode='attach',auto_unlock=False,resume_on_disconnect=False) as s:
    t=s.target
    t.halt()
    f=t.memory_map.get_boot_memory().flash
    # Stop the previous SDRAM application and clear Cortex-M7 cache state.
    t.reset_and_halt(Target.ResetType.SYSRESETREQ)
    f.init(f.Operation.VERIFY,reset=False)
    f.cleanup()
    # Ensure this is still the backed-up target or our known demo before erasing.
    current=bytes(t.read_memory_block8(0x70000000,512))
    assert current==original[:512], 'Unexpected target Flash configuration'
    tick=time.perf_counter()
    f.init(f.Operation.ERASE,reset=False)
    f.erase_sector(0x70000000)
    f.cleanup()
    times['erase_s']=time.perf_counter()-tick
    print('ERASE_OK',times['erase_s'],flush=True)
    tick=time.perf_counter()
    f.init(f.Operation.PROGRAM,reset=False)
    for off in range(0,0x10000,256):
        page=payload[off:off+256]
        if page!=b'\xff'*256:
            f.program_page(0x70000000+off,list(page))
    f.cleanup()
    times['program_s']=time.perf_counter()-tick
    print('PROGRAM_OK',times['program_s'],flush=True)
    tick=time.perf_counter()
    # Verify on a new hardware/cache state, not with previous XIP/AHB buffers.
    t.reset_and_halt(Target.ResetType.SYSRESETREQ)
    f.init(f.Operation.VERIFY,reset=False)
    f.cleanup()
    actual=bytes(t.read_memory_block8(0x70000000,0x10000))
    assert actual==payload, 'Flash readback mismatch: target remains halted'
    (output/'verified-sector.bin').write_bytes(actual)
    times['verify_s']=time.perf_counter()-tick
    print('VERIFY_OK',times['verify_s'],flush=True)
    t.reset(Target.ResetType.SYSRESETREQ)
times['total_s']=time.perf_counter()-begin
report={'date_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'restore_original':restore,'signal_test':signals,'probe_serial':'0C36968AD31958CA','requested_swd_hz':1000000,
        'image_bytes':len(image),'erase_address':'0x70000000','erase_bytes':65536,
        'backup_sha256':expected,'image_sha256':hashlib.sha256(image).hexdigest(),
        'sector_sha256':hashlib.sha256(payload).hexdigest(),'timing':times,
        'timing_scope':'Total includes connection, target check, one-sector erase/program/readback and reset; excludes compile and full backups.'}
(output/('restore-result.json' if restore else 'program-result.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2),flush=True)
'@ | & $python -
if ($LASTEXITCODE -ne 0) { throw 'RT1064 programming failed; inspect output before retrying' }

