# Execute only after the user authorizes replacing the STM32 startup program.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$backup = 'artifacts\backups\stm32f407-original-30001d-32334709-32313836.bin'
$expectedHash = '2BE4A842C57AFF5F86A434BC02DB66D1B2DC3BD7BFB6D940C92F13F1359D0BDB'
if ((Get-Item -LiteralPath $backup).Length -ne 1048576 -or (Get-FileHash -LiteralPath $backup).Hash -ne $expectedHash) { throw 'Verified original firmware backup missing or modified' }
if (-not (Test-Path -LiteralPath artifacts\target-test\stm32f407-selftest.elf)) { throw 'Prepared target test ELF missing' }
& .\artifacts\probe_cli.exe swdcheck 0C36968AD31958CA
if ($LASTEXITCODE -ne 0) { throw 'SWD preflight failed' }
& .\tools\openocd\openocd.exe -s tools/openocd/scripts -s . -f configs/stm32f407-program-selftest.cfg -l artifacts/stm32f407-program-selftest.log
if ($LASTEXITCODE -ne 0) { throw 'Programming/verification did not complete successfully; inspect log before proceeding' }
Get-Content artifacts\stm32f407-program-selftest.log
