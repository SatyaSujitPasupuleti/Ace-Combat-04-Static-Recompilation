param(
    [ValidateSet('generate','build','trace','all')][string]$Action = 'all',
    [int]$Jobs = 4,
    [int]$Seconds = 30,
    [int]$Vblanks = 300,
    [switch]$Video
)
$ErrorActionPreference = 'Stop'
$ac4Python = Join-Path $PSScriptRoot '..\toolchain\msys64\ucrt64\bin\python.exe'
$ac4Driver = Join-Path $PSScriptRoot 'tools\ac4.py'
if (-not (Test-Path -LiteralPath $ac4Python)) { throw 'Project-local toolchain is missing; see README-AC4.md.' }
$ac4Arguments = @($ac4Driver, $Action, '--jobs', $Jobs, '--seconds', $Seconds, '--vblanks', $Vblanks)
if ($Video) { $ac4Arguments += '--video' }
& $ac4Python @ac4Arguments
exit $LASTEXITCODE
