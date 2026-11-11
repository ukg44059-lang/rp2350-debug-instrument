param([switch]$Clean,[ValidateSet(150,300)][int]$ClockMHz=300)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
New-Item -ItemType Directory -Force .tmp,.cache,build,artifacts | Out-Null
$env:TEMP = "$projectRoot\.tmp"
$env:TMP = $env:TEMP
$env:PYTHONDONTWRITEBYTECODE = '1'
$env:PICO_SDK_PATH = "$projectRoot\third_party\pico-sdk"
$toolchainRoot = Get-ChildItem -LiteralPath "$projectRoot\tools\arm-toolchain" -Directory | Select-Object -First 1
$env:PICO_TOOLCHAIN_PATH = $toolchainRoot.FullName
$cmakeExe = "$projectRoot\tools\cmake\bin\cmake.exe"
$ninjaExe = "$projectRoot\tools\ninja\ninja.exe"
if (-not (Test-Path -LiteralPath $cmakeExe)) { throw 'Missing local tools/cmake/bin/cmake.exe' }
$buildPath = if ($Clean) { "build/probe-$ClockMHz-clean" } else { "build/probe-$ClockMHz" }
& $cmakeExe -S firmware -B $buildPath -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninjaExe" "-Dpicotool_DIR=$projectRoot/tools/picotool/picotool" "-DPROBE_SYS_CLOCK_MHZ=$ClockMHz" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
& $cmakeExe --build $buildPath --parallel 6
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
Copy-Item -LiteralPath "$buildPath/ai_debugprobe.uf2","$buildPath/ai_debugprobe.elf" -Destination artifacts -Force
