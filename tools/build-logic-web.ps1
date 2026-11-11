$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
New-Item -ItemType Directory -Force .tmp,artifacts,build | Out-Null
$env:TEMP="$projectRoot\.tmp";$env:TMP=$env:TEMP
$compiler='G:\sofeware\clion\CLion 2022.1\bin\mingw\bin\g++.exe'
& $compiler -std=c++17 -O2 -Wall -Wextra -Werror -static -Ifirmware tests/stream_codec_test.cpp -o build/stream_codec_test.exe
if ($LASTEXITCODE -ne 0) {throw 'Streaming test build failed'}
& ./build/stream_codec_test.exe
if ($LASTEXITCODE -ne 0) {throw 'Streaming test failed'}
& $compiler -std=c++17 -O2 -Wall -Wextra -Werror -static -Ifirmware -Ihost tests/stream_parser_test.cpp -o build/stream_parser_test.exe
if ($LASTEXITCODE -ne 0) {throw 'Stream parser test build failed'}
& ./build/stream_parser_test.exe
if ($LASTEXITCODE -ne 0) {throw 'Stream parser test failed'}
& $compiler -std=c++17 -O2 -Wall -Wextra -Werror -static -Ifirmware host/logic_web.cpp -lsetupapi -lwinusb -lws2_32 -o artifacts/logic_web.exe
if ($LASTEXITCODE -ne 0) {throw 'Web bridge build failed'}
