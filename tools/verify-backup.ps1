$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$blocks = 'artifacts\backups\chunks'
$destination = 'artifacts\backups\stm32f407-original-30001d-32334709-32313836.bin'
$staging = "$destination.partial"
$manifest = @()
# Validate every block BEFORE creating a recovery image.
for ($blockIndex = 0; $blockIndex -lt 64; $blockIndex++) {
    $first = Join-Path $blocks ('{0:D2}-a.bin' -f $blockIndex)
    $second = Join-Path $blocks ('{0:D2}-b.bin' -f $blockIndex)
    if ((Get-Item -LiteralPath $first).Length -ne 16384 -or (Get-Item -LiteralPath $second).Length -ne 16384) { throw "Incomplete block $blockIndex" }
    $firstHash = (Get-FileHash -LiteralPath $first -Algorithm SHA256).Hash
    if ($firstHash -ne (Get-FileHash -LiteralPath $second -Algorithm SHA256).Hash) { throw "Two reads differ for block $blockIndex" }
    $manifest += [pscustomobject]@{ index=$blockIndex; address=('0x{0:x8}' -f (0x08000000 + $blockIndex * 16384)); length=16384; sha256=$firstHash }
}
$stream = [IO.File]::Create((Join-Path $projectRoot $staging))
try {
    for ($blockIndex = 0; $blockIndex -lt 64; $blockIndex++) {
        $data = [IO.File]::ReadAllBytes((Join-Path $projectRoot (Join-Path $blocks ('{0:D2}-a.bin' -f $blockIndex))))
        $stream.Write($data,0,$data.Length)
    }
} finally { $stream.Dispose() }
if ((Get-Item -LiteralPath $staging).Length -ne 1048576) { throw 'Merged backup size mismatch' }
Move-Item -LiteralPath $staging -Destination $destination -Force
$record = [ordered]@{ target_uid=@('0x0030001d','0x32334709','0x32313836'); flash_address='0x08000000'; bytes=1048576; independent_reads_per_block=2; sha256=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash; blocks=$manifest }
$record | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$destination.manifest.json" -Encoding utf8
Write-Output "Verified 64 block pairs and merged 1048576 bytes. SHA256: $($record.sha256)"
