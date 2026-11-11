# Reprogram the same user-authorized PB2 demo, collecting actual phase timings.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$logPath = 'artifacts/pb2-demo/program-timed.log'
$timer = [Diagnostics.Stopwatch]::StartNew()
& .\tools\openocd\openocd.exe -d3 -s tools/openocd/scripts -s . -f configs/stm32f407-program-pb2.cfg -l $logPath
$programExit = $LASTEXITCODE
$timer.Stop()
if ($programExit -ne 0) {throw 'Timed programming failed; see log'}
$events = @{}
foreach ($line in Get-Content -LiteralPath $logPath) {
    if ($line -match '^User\s*:\s*\d+\s+(\d+)\s+.*?(\*\* Programming Started \*\*|\*\* Programming Finished \*\*|\*\* Verify Started \*\*|\*\* Verified OK \*\*|\*\* Resetting Target \*\*|shutdown command invoked)') {
        $events[$Matches[2]]=[long]$Matches[1]
    }
}
foreach ($eventName in @('** Programming Started **','** Programming Finished **','** Verify Started **','** Verified OK **','** Resetting Target **','shutdown command invoked')) {
    if (-not $events.ContainsKey($eventName)) {throw "Missing phase marker: $eventName"}
}
$record = [ordered]@{
    user_date='2026-11-02'; timezone='America/Denver'; run_timestamp_utc=[DateTime]::UtcNow.ToString('o');
    probe_serial='0C36968AD31958CA'; target_uid=@('0x0030001d','0x32334709','0x32313836');
    requested_swd_khz=100; image_bytes=(Get-Item artifacts\pb2-demo\stm32f407-pb2-blink.bin).Length;
    erased_sector_bytes=16384; bin_sha256=(Get-FileHash artifacts\pb2-demo\stm32f407-pb2-blink.bin).Hash;
    total_process_ms=[Math]::Round($timer.Elapsed.TotalMilliseconds,3);
    erase_and_write_ms=$events['** Programming Finished **']-$events['** Programming Started **'];
    verify_ms=$events['** Verified OK **']-$events['** Verify Started **'];
    reset_to_shutdown_ms=$events['shutdown command invoked']-$events['** Resetting Target **'];
    phase_markers_ms=$events; result='programmed_verified_reset';
    scope='A repeat of the same PB2 image. Total includes OpenOCD startup, USB/target setup, UID check, erase/write, verification, reset and process exit; excludes compilation and original backup. Phase clock has millisecond resolution.'
}
$record | ConvertTo-Json -Depth 5 | Set-Content artifacts\pb2-demo\timing.json -Encoding utf8
$record | ConvertTo-Json -Depth 5
