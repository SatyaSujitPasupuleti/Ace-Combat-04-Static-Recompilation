[CmdletBinding(PositionalBinding=$false)]
param(
    [ValidateSet('Build','Play','Optimize','Check')][string]$Action='Build',
    [Parameter(Position=0)][string]$Iso,
    [string]$Toolchain,
    [ValidateRange(1,64)][int]$Jobs=4,
    [ValidateSet('stream','memory')][string]$DiscCache='stream',
    [switch]$NonInteractive
)
$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$false
$driver=Join-Path $PSScriptRoot 'tools/setup_ac4.py'
$localPath=Join-Path $PSScriptRoot 'local.json'
$config=$null
if (Test-Path -LiteralPath $localPath) { $config=Get-Content -LiteralPath $localPath -Raw | ConvertFrom-Json }

function Find-Python([string]$Prefix) {
    $candidates=@()
    if ($Prefix) { $candidates+=@{Exe=(Join-Path $Prefix 'bin/python.exe');Prefix=@()} }
    foreach ($name in @('py.exe','python.exe','python3.exe')) {
        $command=Get-Command $name -ErrorAction SilentlyContinue
        if ($command -and $command.Source -notlike '*\WindowsApps\*') {
            $candidates+=@{Exe=$command.Source;Prefix=$(if ($name -eq 'py.exe') { @('-3') } else { @() })}
        }
    }
    foreach ($candidate in $candidates) {
        if (-not (Test-Path -LiteralPath $candidate.Exe)) { continue }
        try {
            $prefixArgs=@($candidate.Prefix)
            $result=& $candidate.Exe @prefixArgs -c "import sys; print('AC4-PYTHON-OK' if sys.version_info >= (3,10) else 'OLD')" 2>$null
            if ($LASTEXITCODE -eq 0 -and $result -eq 'AC4-PYTHON-OK') { return $candidate }
        } catch { }
    }
    throw 'Python 3.10+ was not found. Install the MSYS2 UCRT64 Python package or Python for Windows; see docs/BUILD.md.'
}

function Invoke-Builder([string[]]$Arguments) {
    $prefixArgs=@($script:python.Prefix)
    & $script:python.Exe @prefixArgs $driver @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Build helper failed (exit $LASTEXITCODE). See the output above; no later steps were run." }
}

try {
    if (-not $Toolchain -and $config) { $Toolchain=$config.toolchain }
    if (-not $Toolchain) {
        $candidates=@($env:MSYSTEM_PREFIX,'C:\msys64\ucrt64')
        $gcc=Get-Command gcc.exe -ErrorAction SilentlyContinue
        if ($gcc) { $candidates+=Split-Path (Split-Path $gcc.Source -Parent) -Parent }
        foreach ($candidate in $candidates) {
            if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'bin/gcc.exe'))) { $Toolchain=$candidate; break }
        }
    }
    if (-not $Toolchain -or -not (Test-Path -LiteralPath (Join-Path $Toolchain 'bin/gcc.exe'))) {
        if ($NonInteractive) { throw 'MSYS2 UCRT64 not found. Pass -Toolchain C:\msys64\ucrt64. See docs/BUILD.md for installation.' }
        Write-Host 'Install MSYS2 and its UCRT64 packages first; see docs/BUILD.md.'
        $Toolchain=(Read-Host 'MSYS2 UCRT64 folder (for example C:\msys64\ucrt64)').Trim().Trim('"')
    }
    $Toolchain=(Resolve-Path -LiteralPath $Toolchain).Path
    $script:python=Find-Python $Toolchain
    if ($Action -eq 'Play') {
        if (-not $config) { throw 'Build first: double-click build.cmd.' }
        if ($PSBoundParameters.ContainsKey('Iso') -or $PSBoundParameters.ContainsKey('Toolchain')) {
            throw 'Play uses the paths saved by Build. Run Build with the new paths first.'
        }
        Invoke-Builder @('run','--disc-cache',$DiscCache)
    } else {
        if (-not $Iso -and $config) { $Iso=$config.iso }
        if (-not $Iso -or -not (Test-Path -LiteralPath $Iso)) {
            if ($NonInteractive) { throw 'ISO not found. Pass -Iso with the path to your NTSC-U AC04 ISO.' }
            Add-Type -AssemblyName System.Windows.Forms
            $picker=New-Object System.Windows.Forms.OpenFileDialog
            try {
                $picker.Title='Select your Ace Combat 04 NTSC-U ISO (SLUS_201.52)'
                $picker.Filter='ISO images (*.iso)|*.iso'
                if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { throw 'ISO selection cancelled; no build started.' }
                $Iso=$picker.FileName
            } finally { $picker.Dispose() }
        }
        $Iso=(Resolve-Path -LiteralPath $Iso).Path
        Write-Host "AC4 $Action | ISO: $Iso | Toolchain: $Toolchain"
        $step=if ($Action -eq 'Check') { 'check' } else { 'prepare' }
        Invoke-Builder @($step,'--iso',$Iso,'--toolchain',$Toolchain)
        if ($Action -ne 'Check') {
            if ($Action -eq 'Optimize') {
                Invoke-Builder @('build','--jobs',"$Jobs")
                Write-Host 'Play through menus, briefing and flight, then exit with Escape to capture VU programs.'
                Invoke-Builder @('capture-vu','--disc-cache',$DiscCache)
                Invoke-Builder @('build-native','--jobs',"$Jobs")
            } elseif (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'out/vu_programs/manifest.json')) {
                Invoke-Builder @('build-native','--jobs',"$Jobs")
            } else {
                Invoke-Builder @('build','--jobs',"$Jobs")
                Write-Host 'Playable bootstrap build ready. Optional native VU optimization: .\build.ps1 -Action Optimize'
            }
            Write-Host 'Build complete. Double-click play.cmd to play. F4: settings; Escape: exit.'
        }
    }
} catch {
    Write-Host "AC4: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
