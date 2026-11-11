param([ValidateRange(4,262144)][int]$Samples=262144,
      [ValidateSet(0,1,2)][int]$Trigger=0,
      [ValidateRange(0,7)][int]$Channel=0,
      [string]$Prefix='')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
if ($Samples%4) {throw 'Samples must be a multiple of four'}
if (-not $Prefix) {$Prefix='artifacts/captures/10MSps-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')}
& ./artifacts/logic_cli.exe capture 0C36968AD31958CA 10000000 $Samples $Prefix $Trigger $Channel 5000
if ($LASTEXITCODE -ne 0) {throw '10 MS/s capture failed'}
