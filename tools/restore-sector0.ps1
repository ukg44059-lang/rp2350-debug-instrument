# Companion to program-selftest.ps1; use under the same target-write authorization.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$backup = 'artifacts\backups\stm32f407-original-30001d-32334709-32313836.bin'
if ((Get-FileHash -LiteralPath $backup).Hash -ne '2BE4A842C57AFF5F86A434BC02DB66D1B2DC3BD7BFB6D940C92F13F1359D0BDB') { throw 'Original backup changed' }
$bytes = [IO.File]::ReadAllBytes((Join-Path $projectRoot $backup))
[IO.File]::WriteAllBytes((Join-Path $projectRoot 'artifacts\backups\stm32f407-sector0-original.bin'),[byte[]]$bytes[0..16383])
& .\artifacts\probe_cli.exe swdcheck 0C36968AD31958CA
if ($LASTEXITCODE -ne 0) { throw 'SWD preflight failed' }
& .\tools\openocd\openocd.exe -s tools/openocd/scripts -s . -f configs/stm32f407-restore-sector0.cfg -l artifacts/stm32f407-restore-sector0.log
if ($LASTEXITCODE -ne 0) { throw 'Original sector restore did not complete successfully; inspect log' }
Get-Content artifacts\stm32f407-restore-sector0.log
