param([ValidateSet(150,300)][int]$ClockMHz=300)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:TEMP="$projectRoot\.tmp"
$env:TMP=$env:TEMP
$out="artifacts/overclock-$ClockMHz"
New-Item -ItemType Directory -Force $out | Out-Null
$runs=@()
$cases=@(@{Rate=1000000; Trigger=1; Channel=0})
foreach ($ch in 0..7) {foreach ($tr in 1,2) {
    $cases+=@{Rate=10000000; Trigger=$tr; Channel=$ch}
}}
$cases+=@{Rate=10000000; Trigger=0; Channel=0}
try {
    & ./tools/set-rt1064-signal-rate.ps1 -StepHz 50000
    if ($LASTEXITCODE -ne 0) {throw 'Signal source rate setup failed'}
    foreach ($case in $cases) {
        $prefix="$out/native-$($case.Rate)-t$($case.Trigger)-ch$($case.Channel)"
        $watch=[Diagnostics.Stopwatch]::StartNew()
        & ./artifacts/logic_cli.exe capture 0C36968AD31958CA $case.Rate 262144 $prefix $case.Trigger $case.Channel 5000 *> "$prefix.capture.log"
        if ($LASTEXITCODE -ne 0) {throw "Capture failed: $prefix"}
        $watch.Stop()
        $checkArgs=@($prefix,50000)
        if ($case.Trigger) {$checkArgs+=@($case.Trigger,$case.Channel)}
        & ./artifacts/logic_signal_check.exe @checkArgs *> "$prefix.check.log"
        $waveformPassed=$LASTEXITCODE -eq 0
        $meta=Get-Content -Raw -LiteralPath "$prefix.json" | ConvertFrom-Json
        if ($meta.system_clock_hz -ne $ClockMHz*1000000 -or $meta.actual_rate_hz -ne $case.Rate -or $meta.flags -ne 0) {throw 'Clock/rate/flags mismatch'}
        $runs+=[pscustomobject]@{prefix=$prefix; rate_hz=$case.Rate; trigger=$case.Trigger; channel=$case.Channel; samples=$meta.samples; flags=$meta.flags; waveform_passed=$waveformPassed; upload_ms=$meta.upload_ms; host_elapsed_s=$watch.Elapsed.TotalSeconds}
        Write-Output "WAVEFORM_PASSED=$waveformPassed $prefix"
    }
    [pscustomobject]@{system_clock_hz=$ClockMHz*1000000; runs=$runs; total_samples=($runs.samples | Measure-Object -Sum).Sum} | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 "$out/native-test-summary.json"
    $failed=@($runs | Where-Object {-not $_.waveform_passed})
    if ($failed.Count) {throw "Native overclock waveform failures: $($failed.Count) of $($runs.Count); all evidence retained"}
    Write-Output "NATIVE_OVERCLOCK_TEST_PASSED runs=$($runs.Count)"
} finally {
    & ./tools/set-rt1064-signal-rate.ps1 -StepHz 1000
}
