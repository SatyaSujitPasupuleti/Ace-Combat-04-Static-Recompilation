param([switch]$Bridge)
$ErrorActionPreference = 'Stop'
$toolRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../toolchain/reverse'))
$healthy = $false
try { $healthy = (Invoke-WebRequest -UseBasicParsing 'http://127.0.0.1:8089/check_connection' -TimeoutSec 2).StatusCode -eq 200 } catch {}
if (-not $healthy) {
    $ghidraRoot = Join-Path $toolRoot 'ghidra_12.1.4_PUBLIC'
    $jarPaths = (Get-ChildItem (Join-Path $ghidraRoot 'Ghidra') -Recurse -Filter '*.jar' | ForEach-Object FullName) -join ';'
    $argLines = @('-Xmx4g', ('-Dghidra.home='+$ghidraRoot), '-Dapplication.name=GhidraMCP', '-classpath', ('"'+$jarPaths+'"'), 'com.xebyte.headless.GhidraMCPHeadlessServer', '--bind', '127.0.0.1', '--port', '8089', '--project', ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../work/AC4Native.gpr'))), '--program', 'SLUS_201.52')
    $argFile = Join-Path $toolRoot 'server.args'
    ($argLines -join "`n").Replace('\','/') | Set-Content -Encoding ascii $argFile
    $server = Start-Process -FilePath (Join-Path $toolRoot 'jdk-21.0.12.1+1/bin/java.exe') -ArgumentList ('@'+$argFile.Replace('\','/')) -WindowStyle Hidden -RedirectStandardOutput (Join-Path $toolRoot 'server.log') -RedirectStandardError (Join-Path $toolRoot 'server.err.log') -PassThru
    $server.Id | Set-Content (Join-Path $toolRoot 'server.pid')
    for ($attempt=0; $attempt -lt 45; $attempt++) {
        Start-Sleep -Seconds 1
        if ($server.HasExited) { throw 'Ghidra backend exited; see toolchain/reverse/server.err.log' }
        try { $healthy = (Invoke-WebRequest -UseBasicParsing 'http://127.0.0.1:8089/check_connection' -TimeoutSec 1).StatusCode -eq 200 } catch {}
        if ($healthy) { break }
    }
    if (-not $healthy) { throw 'Ghidra backend did not become ready.' }
}
if ($Bridge) {
    & (Join-Path $toolRoot 'mcp-venv/Scripts/python.exe') -m bridge_mcp_ghidra --lazy
    exit $LASTEXITCODE
}
Write-Host 'Ghidra MCP ready on localhost:8089'
