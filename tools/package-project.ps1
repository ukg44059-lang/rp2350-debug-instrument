param([string]$Version='0.3.0')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:TEMP="$projectRoot\.tmp"; $env:TMP=$env:TEMP
New-Item -ItemType Directory -Force packages,.tmp | Out-Null
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Write-Package([string]$Destination,[string[]]$Files) {
    $stream=[System.IO.File]::Open($Destination,[System.IO.FileMode]::CreateNew)
    $zip=[System.IO.Compression.ZipArchive]::new($stream,[System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach($relative in ($Files | Sort-Object -Unique)) {
            $path=Join-Path $projectRoot $relative
            if(-not (Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing package file: $relative"}
            $null=[System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$path,$relative.Replace('\','/'),[System.IO.Compression.CompressionLevel]::Optimal)
        }
    } finally {$zip.Dispose();$stream.Dispose()}
}
$source="packages/rp2350-debug-instrument-v$Version-source.zip"
$runtime="packages/rp2350-debug-instrument-v$Version-windows.zip"
foreach($path in @($source,$runtime)){if(Test-Path -LiteralPath $path){throw "Package already exists: $path; move it aside before rebuilding"}}
$tracked=@(git -c core.quotepath=false ls-files)
if($LASTEXITCODE -ne 0 -or -not $tracked.Count){throw 'Stage or commit project files before packaging'}
Write-Package $source $tracked
$binary=@('artifacts/ai_debugprobe.uf2','artifacts/ai_debugprobe.elf','artifacts/probe_cli.exe','artifacts/logic_cli.exe','artifacts/logic_signal_check.exe','artifacts/logic_web.exe','tools/start-logic-web.ps1','README.md','docs/logic-web.md','docs/github-delivery.md','docs/logic-analyzer-usage.md','reports/逻辑分析仪网页与USB流采样_2026-11-02.md')
$binary+=@(Get-ChildItem web -File | ForEach-Object {'web/'+$_.Name})
$binary+=@(Get-ChildItem artifacts/streaming -Filter 'web-*.png' -File | ForEach-Object {'artifacts/streaming/'+$_.Name})
Write-Package $runtime $binary
$lines=@($source,$runtime) | ForEach-Object {(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLower()+'  '+[System.IO.Path]::GetFileName($_)}
[System.IO.File]::WriteAllLines((Join-Path $projectRoot 'packages/SHA256SUMS'),$lines,[System.Text.UTF8Encoding]::new($false))
Get-Item $source,$runtime,packages/SHA256SUMS | Select-Object Name,Length
