param([ValidateSet(100,500,1000)][int]$SpeedKhz=1000)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$backup = 'artifacts\backups\stm32f407-original-30001d-32334709-32313836.bin'
if ((Get-Item -LiteralPath $backup).Length -ne 1048576 -or (Get-FileHash -LiteralPath $backup).Hash -ne '2BE4A842C57AFF5F86A434BC02DB66D1B2DC3BD7BFB6D940C92F13F1359D0BDB') {throw 'Original verified backup missing or modified'}
& .\tools\build-pb2-demo.ps1
if ((Get-Item -LiteralPath artifacts/pb2-demo/stm32f407-pb2-blink.bin).Length -gt 16384) {throw 'PB2 image exceeds the authorized sector 0 range'}
& .\artifacts\probe_cli.exe swdcheck 0C36968AD31958CA
if ($LASTEXITCODE -ne 0) {throw 'SWD preflight failed'}
& .\tools\openocd\openocd.exe -s tools/openocd/scripts -s . -c "set PROBE_KHZ $SpeedKhz" -f configs/stm32f407-program-pb2-fast.cfg -l artifacts/pb2-demo/program.log
if ($LASTEXITCODE -ne 0) {throw 'PB2 programming did not complete; inspect artifacts/pb2-demo/program.log'}
Get-Content artifacts\pb2-demo\program.log
