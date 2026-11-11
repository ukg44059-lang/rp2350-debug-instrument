param([ValidateSet(100,500,1000)][int]$SpeedKhz=100)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:TEMP="$projectRoot\.tmp"
$env:TMP=$env:TEMP
$backup='artifacts/backups/stm32f407-original-30001d-32334709-32313836.bin'
if ((Get-FileHash -LiteralPath $backup).Hash -ne '2BE4A842C57AFF5F86A434BC02DB66D1B2DC3BD7BFB6D940C92F13F1359D0BDB') {throw 'Original backup mismatch'}
$image='artifacts/pb2-demo/stm32f407-pb2-blink.bin'
if ((Get-Item -LiteralPath $image).Length -ne 484 -or (Get-FileHash -LiteralPath $image).Hash -ne '8F8E53141FD6EE34EA09C613D0E8BF9D01CC231A3741C9DBC70C2428355E7A67') {throw 'Expected authorized PB2 image changed'}
$logPath="artifacts/pb2-demo/fast-${SpeedKhz}khz.log"
# Retain the previous measurement when rerunning this rate.
if (Test-Path -LiteralPath $logPath) {Copy-Item -LiteralPath $logPath -Destination "$logPath.previous" -Force}
$jsonPath="artifacts/pb2-demo/fast-${SpeedKhz}khz.json"
if (Test-Path -LiteralPath $jsonPath) {Copy-Item -LiteralPath $jsonPath -Destination "$jsonPath.previous" -Force}
$timestamp=[DateTime]::UtcNow.ToString('o')
$timer=[Diagnostics.Stopwatch]::StartNew()
& .\tools\openocd\openocd.exe -d3 -s tools/openocd/scripts -s . -c "set PROBE_KHZ $SpeedKhz" -f configs/stm32f407-program-pb2-fast.cfg -l $logPath
$programExit=$LASTEXITCODE
$timer.Stop()
if ($programExit -ne 0) {throw "OpenOCD exited with code $programExit; inspect $logPath before further action"}
$events=@{}
foreach ($line in Get-Content -LiteralPath $logPath) {
    if ($line -match '^User\s*:\s*\d+\s+(\d+)\s+.*?(\*\* (?:Erase Started|Erase Finished|Write Started|Write Finished|Verify Started|Verified OK|Resetting Target) \*\*|shutdown command invoked)') {$events[$Matches[2]]=[long]$Matches[1]}
}
foreach ($name in @('Erase Started','Erase Finished','Write Started','Write Finished','Verify Started','Verified OK','Resetting Target')) {
    if (-not $events.ContainsKey("** $name **")) {throw "Missing timing marker: $name"}
}
$record=[ordered]@{
    run_timestamp_utc=$timestamp; requested_swd_khz=$SpeedKhz;
    image_bytes=484; erased_sector_bytes=16384;
    total_process_ms=[Math]::Round($timer.Elapsed.TotalMilliseconds,3);
    erase_ms=$events['** Erase Finished **']-$events['** Erase Started **'];
    write_ms=$events['** Write Finished **']-$events['** Write Started **'];
    verify_ms=$events['** Verified OK **']-$events['** Verify Started **'];
    result='programmed_verified_reset'; phase_markers_ms=$events;
    log=$logPath; scope='Separate sector erase, exact 484-byte BIN write, BIN verify, software reset. Total includes process startup/exit, target setup and UID checks; excludes build and backup. Clock is requested, not instrument-measured.'
}
$record | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
$record | ConvertTo-Json -Depth 5
