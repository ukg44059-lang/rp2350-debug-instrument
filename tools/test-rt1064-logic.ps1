param([ValidateSet('baseline','triggers','fast')][string]$Mode='baseline')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:TEMP="$projectRoot\.tmp"
$env:TMP=$env:TEMP
$compiler='G:\sofeware\clion\CLion 2022.1\bin\mingw\bin\g++.exe'
& $compiler -std=c++17 -O2 -Wall -Wextra -Werror -static host/logic_signal_check.cpp -o artifacts/logic_signal_check.exe
if ($LASTEXITCODE -ne 0) {throw 'Capture checker build failed'}
$runs = @()
if ($Mode -eq 'baseline') {
    $runs += @{Name='baseline-100k'; Rate=100000; Samples=262144; Trigger=0; Channel=0; Step=1000}
    $runs += @{Name='baseline-10k'; Rate=10000; Samples=65536; Trigger=0; Channel=0; Step=1000}
} elseif ($Mode -eq 'triggers') {
    foreach ($ch in 0..7) { foreach ($trigger in 1,2) {
        $runs += @{Name="trigger-$trigger-ch$ch"; Rate=100000; Samples=65536; Trigger=$trigger; Channel=$ch; Step=1000}
    }}
} else {
    foreach ($rate in 1000000,10000000) {
        $runs += @{Name="fast-$rate"; Rate=$rate; Samples=262144; Trigger=1; Channel=0; Step=50000}
    }
}
New-Item -ItemType Directory -Force artifacts/rt1064/signals | Out-Null
foreach ($run in $runs) {
    $prefix="artifacts/captures/rt1064-$($run.Name)"
    & ./artifacts/logic_cli.exe capture 0C36968AD31958CA $run.Rate $run.Samples $prefix $run.Trigger $run.Channel 5000
    if ($LASTEXITCODE -ne 0) {throw "Capture failed: $prefix"}
    $checkArgs=@($prefix,$run.Step)
    if ($run.Trigger) {$checkArgs+=@($run.Trigger,$run.Channel)}
    & ./artifacts/logic_signal_check.exe @checkArgs
    if ($LASTEXITCODE -ne 0) {throw "Waveform validation failed: $prefix"}
}
