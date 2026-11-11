param([ValidateRange(1024,65535)][int]$Port=8765,[switch]$NoBrowser)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
New-Item -ItemType Directory -Force artifacts/streaming,.tmp | Out-Null
$env:TEMP="$projectRoot\.tmp";$env:TMP=$env:TEMP
if (-not (Test-Path artifacts/logic_web.exe)) {& ./tools/build-logic-web.ps1}
try {
    $response=Invoke-WebRequest -Uri "http://127.0.0.1:$Port/api/state" -TimeoutSec 2
    $null=$response.Content | ConvertFrom-Json
    Write-Output "Logic service is already running: http://127.0.0.1:$Port/"
} catch {
    $process=Start-Process -FilePath "$projectRoot\artifacts\logic_web.exe" -ArgumentList @('--port',"$Port") -WorkingDirectory $projectRoot -WindowStyle Hidden -RedirectStandardOutput "$projectRoot\artifacts\streaming\server.log" -RedirectStandardError "$projectRoot\artifacts\streaming\server-error.log" -PassThru
    $process.Id | Set-Content artifacts/streaming/server.pid
    Start-Sleep -Milliseconds 600
    if ($process.HasExited) {throw 'Logic service failed to start; check artifacts/streaming/server-error.log'}
    Write-Output "Logic service started: http://127.0.0.1:$Port/"
}
if (-not $NoBrowser) {Start-Process "http://127.0.0.1:$Port/"}
