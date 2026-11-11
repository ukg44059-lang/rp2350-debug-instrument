param([string]$Compiler='G:\sofeware\clion\CLion 2022.1\bin\mingw\bin\g++.exe')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:TEMP="$projectRoot\.tmp";$env:TMP=$env:TEMP
New-Item -ItemType Directory -Force artifacts,build,.tmp | Out-Null
& $Compiler -std=c++17 -Wall -Wextra -Werror -O2 -static -Ifirmware host/logic_cli.cpp -lsetupapi -lwinusb -o artifacts/logic_cli.exe
if ($LASTEXITCODE -ne 0) {throw 'Logic host build failed'}
& $Compiler -std=c++17 -Wall -Wextra -Werror -O2 -static -Ifirmware tests/logic_protocol_test.cpp -o build/logic_protocol_test.exe
if ($LASTEXITCODE -ne 0) {throw 'Logic protocol test build failed'}
& .\build\logic_protocol_test.exe
if ($LASTEXITCODE -ne 0) {throw 'Logic protocol tests failed'}
