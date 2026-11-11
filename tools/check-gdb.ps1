param([string]$GdbScript = 'configs/stm32f407-inspect.gdb', [string]$ResultLog = 'artifacts/gdb-inspect.log')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$env:TEMP = "$projectRoot\.tmp"
$env:TMP = $env:TEMP
$server = $null
$serverReady = $false
try {
    # Only run after the previous OpenOCD process has released the probe.
    if (Test-Path -LiteralPath artifacts\gdb-server.log) { Copy-Item -LiteralPath artifacts\gdb-server.log -Destination artifacts\gdb-server.previous.log -Force }
    [IO.File]::WriteAllText((Join-Path $projectRoot 'artifacts\gdb-server.log'),'')
    $server = Start-Process -FilePath "$projectRoot\tools\openocd\openocd.exe" -ArgumentList @('-s','tools/openocd/scripts','-s','.','-f','configs/stm32f407-debug.cfg','-l','artifacts/gdb-server.log') -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    for ($attempt = 0; $attempt -lt 50; $attempt++) {
        if ($server.HasExited) { throw 'OpenOCD server exited before readiness' }
        if ((Test-Path -LiteralPath artifacts\gdb-server.log) -and (Select-String -LiteralPath artifacts\gdb-server.log -SimpleMatch 'Listening on port 3333' -Quiet)) { $serverReady=$true;break }
        Start-Sleep -Milliseconds 200
    }
    if (-not $serverReady) { throw 'OpenOCD readiness timed out' }
    $compilerRoot = Get-ChildItem -LiteralPath tools\arm-toolchain -Directory | Select-Object -First 1
    $gdb = Join-Path $compilerRoot.FullName 'bin\arm-none-eabi-gdb.exe'
    & $gdb -nx -batch -x $GdbScript *> $ResultLog
    if ($LASTEXITCODE -ne 0) { throw "GDB inspection failed; see $ResultLog" }
    Get-Content -LiteralPath $ResultLog
} finally {
    if ($serverReady -and $server -and -not $server.HasExited) {
        # Best-effort resume even if GDB exited midway; never leave a test halt unnoticed.
        $connection = [Net.Sockets.TcpClient]::new()
        try {
            $connection.Connect('127.0.0.1',6666)
            $stream = $connection.GetStream()
            $stream.ReadTimeout=3000
            $request = [Text.Encoding]::ASCII.GetBytes("resume; shutdown" + [char]26)
            $stream.Write($request,0,$request.Length)
            $reply = New-Object byte[] 1024
            try { [void]$stream.Read($reply,0,$reply.Length) } catch { Write-Warning 'OpenOCD RPC closed during shutdown; inspect target state in server log.' }
        } catch { Write-Warning "Could not request target resume: $_" } finally { $connection.Dispose() }
    }
    if ($server -and -not $server.HasExited) {
        if (-not $server.WaitForExit(3000)) { Stop-Process -Id $server.Id }
    }
}
