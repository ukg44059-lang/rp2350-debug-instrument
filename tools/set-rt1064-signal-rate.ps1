param([ValidateRange(100,50000)][int]$StepHz=1000)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:PYTHONPATH="$projectRoot\tools\pyocd"
$env:PYTHONDONTWRITEBYTECODE='1'
$env:RT1064_STEP_HZ="$StepHz"
$python='C:\Users\666\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
@'
import os,time
from elftools.elf.elffile import ELFFile
from pyocd.core.helpers import ConnectHelper
hz=int(os.environ['RT1064_STEP_HZ'])
assert 24000000%hz==0, 'Requested step frequency must divide 24 MHz'
with open('artifacts/rt1064/rt1064-signals.elf','rb') as fp:
    symbols={s.name:s['st_value'] for s in ELFFile(fp).get_section_by_name('.symtab').iter_symbols()}
with ConnectHelper.session_with_chosen_probe(unique_id='0C36968AD31958CA',target_override='mimxrt1064',frequency=1000000,connect_mode='attach',auto_unlock=False,resume_on_disconnect=False) as session:
    t=session.target
    t.halt()
    try:
        assert t.read32(symbols['demo_signature'])==0x1064A008, 'Signal-test image is not running'
        t.write32(symbols['signal_step_ticks'],24000000//hz)
        t.flush()
        assert t.read32(symbols['signal_step_ticks'])==24000000//hz
    finally:
        t.resume()
time.sleep(.1)
print('SIGNAL_STEP_HZ='+str(hz)+' CH0_HZ='+str(hz/2))
'@ | & $python -
if ($LASTEXITCODE -ne 0) {throw 'Setting signal rate failed'}
