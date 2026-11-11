$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:TEMP = "$projectRoot\.tmp"
$env:TMP = $env:TEMP
$compilerRoot = Get-ChildItem -LiteralPath tools\arm-toolchain -Directory | Select-Object -First 1
$targetCompiler = Join-Path $compilerRoot.FullName 'bin\arm-none-eabi-g++.exe'
$objcopy = Join-Path $compilerRoot.FullName 'bin\arm-none-eabi-objcopy.exe'
$sizeExe = Join-Path $compilerRoot.FullName 'bin\arm-none-eabi-size.exe'
New-Item -ItemType Directory -Force artifacts\pb2-demo,.tmp | Out-Null
& $targetCompiler -mcpu=cortex-m4 -mthumb -std=c++17 -Og -g3 -ffreestanding -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-threadsafe-statics -fdata-sections -ffunction-sections -Wall -Wextra -Werror -nostdlib -nostartfiles '-Wl,--gc-sections' '-Wl,-Map=artifacts/pb2-demo/stm32f407-pb2-blink.map' -T target_test/stm32f407.ld target_demo/pb2_blink.cpp -o artifacts/pb2-demo/stm32f407-pb2-blink.elf
if ($LASTEXITCODE -ne 0) { throw 'PB2 demo build failed' }
& $objcopy -O binary artifacts/pb2-demo/stm32f407-pb2-blink.elf artifacts/pb2-demo/stm32f407-pb2-blink.bin
if ($LASTEXITCODE -ne 0) { throw 'PB2 binary conversion failed' }
& $sizeExe artifacts/pb2-demo/stm32f407-pb2-blink.elf
