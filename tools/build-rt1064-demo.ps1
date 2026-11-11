param([switch]$SignalTest)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:TEMP = "$projectRoot\.tmp"
$env:TMP = $env:TEMP
$backup = [IO.File]::ReadAllBytes("$projectRoot\artifacts\rt1064\backup\original-flash.bin")
if ($backup.Length -ne 0x400000 -or [BitConverter]::ToUInt32($backup,0) -ne 0x42464346) { throw 'Invalid RT1064 backup / FCB' }
[IO.File]::WriteAllBytes("$projectRoot\artifacts\rt1064\original-fcb.bin", $backup[0..511])
$toolchain = (Get-ChildItem tools\arm-toolchain -Directory | Select-Object -First 1).FullName
$sdk = "$projectRoot\.tmp\RT1064_Library\SeekFree_RT1064_Opensource_Library\libraries\sdk"
$imageBase = if ($SignalTest) {'artifacts/rt1064/rt1064-signals'} else {'artifacts/rt1064/rt1064-b9-blink'}
$defines = @()
if ($SignalTest) { $defines += '-DRT1064_SIGNAL_TEST=1' }
& "$toolchain\bin\arm-none-eabi-g++.exe" @defines -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -std=c++17 -Og -g3 -ffreestanding -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-threadsafe-statics -fdata-sections -ffunction-sections -Wall -Wextra -Werror -nostdlib -nostartfiles -isystem "$sdk\deceive" -isystem "$sdk\CMSIS\Include" '-Wl,--gc-sections' "-Wl,-Map=$imageBase.map" -T target_demo/rt1064/rt1064.ld target_demo/rt1064/blink.cpp target_demo/rt1064/fcb.S -o "$imageBase.elf"
if ($LASTEXITCODE -ne 0) { throw 'RT1064 build failed' }
& "$toolchain\bin\arm-none-eabi-objcopy.exe" -O binary "$imageBase.elf" "$imageBase.bin"
if ($LASTEXITCODE -ne 0) { throw 'RT1064 objcopy failed' }
& "$toolchain\bin\arm-none-eabi-size.exe" "$imageBase.elf"


