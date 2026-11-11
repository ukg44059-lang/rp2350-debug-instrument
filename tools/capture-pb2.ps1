param([ValidateSet('immediate','rising','falling')][string]$Trigger='immediate')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:TEMP="$projectRoot\.tmp";$env:TMP=$env:TEMP
$mode=@{immediate=0;rising=1;falling=2}[$Trigger]
$prefix='artifacts/captures/pb2-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
# Requires PB2 -> GP8, common ground, and the existing 200-ms PB2 demo running.
# This script never writes target Flash or configures any target output.
& .\artifacts\logic_cli.exe capture 0C36968AD31958CA 100000 262144 $prefix $mode 0 5000
if ($LASTEXITCODE -ne 0) {throw 'PB2 capture failed'}
$record=Get-Content -LiteralPath "$prefix.json" -Raw | ConvertFrom-Json
$channel=$record.channel_stats[0]
if ($channel.edges -lt 4 -or $channel.min_edge_interval_s -lt 0.18 -or $channel.max_edge_interval_s -gt 0.22) {
    throw "CH0 does not match the expected 200-ms PB2 waveform; verify PB2 -> GP8 and target program. Raw capture retained at $prefix"
}
if ($mode -ne 0) {
    $data=[IO.File]::ReadAllBytes((Join-Path $projectRoot "$prefix.bin"))
    $expected=if ($mode -eq 1) {1} else {0}
    if (($data[0] -band 1) -ne $expected) {throw 'First sample has unexpected triggered level'}
}
"PB2_WAVEFORM_CHECK_PASSED: $prefix"
