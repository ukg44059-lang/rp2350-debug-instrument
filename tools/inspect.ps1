$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
# This diagnostic performs SWD line synchronization and DPIDR reads only.
& .\artifacts\probe_cli.exe swdcheck 0C36968AD31958CA
if ($LASTEXITCODE -ne 0) { throw 'SWD synchronization/DPIDR checks failed; no Flash operations attempted' }
& .\tools\openocd\openocd.exe -s tools/openocd/scripts -s . -f configs/stm32f407-readonly.cfg -l artifacts/stm32f407-inspect.log
if ($LASTEXITCODE -ne 0) { throw 'OpenOCD target inspection failed' }
Get-Content artifacts\stm32f407-inspect.log
