$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:TEMP = "$projectRoot\.tmp"
$env:TMP = $env:TEMP
$compilerRoot = Get-ChildItem -LiteralPath tools\arm-toolchain -Directory | Select-Object -First 1
$targetCompiler = Join-Path $compilerRoot.FullName 'bin\arm-none-eabi-g++.exe'
$objcopy = Join-Path $compilerRoot.FullName 'bin\arm-none-eabi-objcopy.exe'
$sizeExe = Join-Path $compilerRoot.FullName 'bin\arm-none-eabi-size.exe'
New-Item -ItemType Directory -Force artifacts\target-test | Out-Null
& $targetCompiler -mcpu=cortex-m4 -mthumb -std=c++17 -Og -g3 -ffreestanding -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-threadsafe-statics -fdata-sections -ffunction-sections -Wall -Wextra -Werror -nostdlib -nostartfiles '-Wl,--gc-sections' '-Wl,-Map=artifacts/target-test/stm32f407-selftest.map' -T target_test/stm32f407.ld target_test/main.cpp -o artifacts/target-test/stm32f407-selftest.elf
if ($LASTEXITCODE -ne 0) { throw 'Target test build failed' }
& $objcopy -O binary artifacts/target-test/stm32f407-selftest.elf artifacts/target-test/stm32f407-selftest.bin
if ($LASTEXITCODE -ne 0) { throw 'Target binary conversion failed' }
& $sizeExe artifacts/target-test/stm32f407-selftest.elf
