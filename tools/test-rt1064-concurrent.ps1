param([string]$OutputPrefix='artifacts/captures/rt1064-concurrent-swd')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
& ./tools/set-rt1064-signal-rate.ps1 -StepHz 1000
if ($LASTEXITCODE -ne 0) {throw 'Set baseline rate failed'}
& ./artifacts/logic_cli.exe arm 0C36968AD31958CA 10000 65536
if ($LASTEXITCODE -ne 0) {throw 'Arm failed'}
$env:PYTHONPATH="$projectRoot\tools\pyocd"
$env:PYTHONDONTWRITEBYTECODE='1'
$python='C:\Users\666\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
@'
import json,time
from pathlib import Path
from elftools.elf.elffile import ELFFile
from pyocd.core.helpers import ConnectHelper
from pyocd.core.target import Target
with open('artifacts/rt1064/rt1064-signals.elf','rb') as fp:
 symbols={s.name:s['st_value'] for s in ELFFile(fp).get_section_by_name('.symtab').iter_symbols()}
rows=[]
with ConnectHelper.session_with_chosen_probe(unique_id='0C36968AD31958CA',target_override='mimxrt1064',frequency=1000000,connect_mode='attach',auto_unlock=False) as s:
 t=s.target
 for n in range(3):
  row={'time':time.perf_counter(),'running':t.get_state()==Target.State.RUNNING,'cpuid':hex(t.read32(0xe000ed00)),'signature':hex(t.read32(symbols['demo_signature'])),'signal_count':t.read32(symbols['signal_count'])}
  print(row,flush=True);rows.append(row);time.sleep(.2)
 assert all(r['running'] and r['cpuid']=='0x411fc271' and r['signature']=='0x1064a008' for r in rows)
 assert rows[-1]['signal_count']>rows[0]['signal_count']
Path('artifacts/rt1064/signals/concurrent-swd.json').write_text(json.dumps(rows,indent=2))
'@ | & $python -
if ($LASTEXITCODE -ne 0) {throw 'Concurrent SWD check failed'}
$deadline=[DateTime]::UtcNow.AddSeconds(10)
do {
    $status=& ./artifacts/logic_cli.exe status 0C36968AD31958CA
    if ($LASTEXITCODE -ne 0) {throw 'Concurrent status failed'}
    $status
    if ($status -match 'state=3 ') {break}
    if ($status -match 'state=5 ') {throw 'Capture failed during SWD reads'}
    Start-Sleep -Milliseconds 250
} while ([DateTime]::UtcNow -lt $deadline)
if ([DateTime]::UtcNow -ge $deadline) {throw 'Concurrent capture timed out'}
& ./artifacts/logic_cli.exe fetch 0C36968AD31958CA $OutputPrefix
if ($LASTEXITCODE -ne 0) {throw 'Concurrent upload failed'}
& ./artifacts/logic_signal_check.exe $OutputPrefix 1000
if ($LASTEXITCODE -ne 0) {throw 'Concurrent waveform failed'}
