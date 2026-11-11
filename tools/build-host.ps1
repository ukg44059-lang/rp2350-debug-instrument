param([string]$Compiler = 'G:\sofeware\clion\CLion 2022.1\bin\mingw\bin\g++.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:TEMP = "$projectRoot\.tmp"
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Force artifacts,build,.tmp | Out-Null
& $Compiler -std=c++17 -Wall -Wextra -Werror -O2 -static host\probe_cli.cpp -lsetupapi -lwinusb -o artifacts\probe_cli.exe
if ($LASTEXITCODE -ne 0) { throw 'Native host build failed' }
& $Compiler -std=c++17 -Wall -Wextra -Werror -O2 -static -Ifirmware tests\packet_guard_test.cpp firmware\packet_guard.cpp -o build\packet_guard_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Packet guard test build failed' }
& .\build\packet_guard_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Packet guard tests failed' }
