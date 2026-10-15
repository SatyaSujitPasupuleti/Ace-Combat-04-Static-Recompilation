param(
    [ValidateSet('Build','Run','Verify')][string]$Action = 'Run',
    [ValidateSet('Show','Skip','GameDefault')][string]$Opening = 'Show',
    [ValidateSet(-1,0,30,60,120)][int]$FpsLimit = 60,
    [ValidateSet('On','Off')][string]$StartupPreparation = 'On',
    [Alias('Mode')][ValidateSet('Custom','Old','Startup','Textures','Both')][string]$PerformanceMode = 'Custom',
    [ValidateSet('Stream','Memory')][string]$DiscCache = 'Stream',
    [switch]$TraceCutscenes,
    [switch]$Metrics,
    [switch]$ColdCache,
    [ValidateRange(1,240)][int]$MetricsMinutes = 60,
    [switch]$NoImagePrewarm,
    [switch]$LegacyTextureDecode,
    [switch]$Diagnostics,
    [switch]$BootTest,
    [switch]$Hidden,
    [int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'
if ($PerformanceMode -ne 'Custom') {
    if ($PSBoundParameters.ContainsKey('StartupPreparation') -or $PSBoundParameters.ContainsKey('LegacyTextureDecode') -or $PSBoundParameters.ContainsKey('NoImagePrewarm')) {
        throw 'Use either -PerformanceMode or the individual optimization flags, not both.'
    }
    $StartupPreparation = if ($PerformanceMode -in @('Startup','Both')) { 'On' } else { 'Off' }
    $LegacyTextureDecode = $PerformanceMode -in @('Old','Startup')
}
if ($ColdCache -and $Action -eq 'Build') { throw '-ColdCache is for Run or Verify, not Build.' }
if ($Metrics -and $Action -ne 'Run') { throw '-Metrics is for -Action Run.' }
$compilerRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../toolchain/msys64/ucrt64'))
$workRoot = Join-Path $PSScriptRoot 'work/native-poc'
$buildRoot = Join-Path $PSScriptRoot 'build/native-poc'
$envNames = @('PS2_AC4_MOVIE_TEST_STALL_FIELDS','PS2_AC4_MOVIE_UNPACED','PS2_AC4_MOVIE_LIFETIME_TRACE','PS2_AC4_MOVIE_REFILL_TRACE','PS2_PAD_RECORD','PS2_CLOCK_TIMELINE','PS2_AC4_AUDIO_CONTROL_TRACE','PS2_DISC_PRELOAD','PATH','PYTHONPATH','PS2_SHADER_DIR','PS2_SETTINGS_FILE','PS2_VU_RECOMP','PS2_VU_VERIFY','PS2_VU_CENSUS','PS2_VU_WATCH','PS2_VU_PROFILE','PS2_TRACE_VU','PS2_PAD_PLAY','PS2_PAD_PLAY_STOP','PS2_PAD_SCRIPT','PS2_PAD_CONTROL','PS2_SHOT_EVERY','PS2_FIELD_HZ','PS2_FIELD_HZ_AFTER','PS2_AC4_OPENING','PS2_FPS_CAP','PS2_PRESENT_QUEUE','PS2_PRESENT_PACE','PS2_PIPELINE_CACHE','PS2_PIPELINE_PREWARM','PS2_IMAGE_PREWARM','PS2_SNAPSHOT_PREWARM','PS2_RT_RESERVE_HEIGHT','PS2_T4_FAST','PS2_T4_PARTIAL','PS2_T4_SOURCE_HASH','PS2_FRAME_TIMELINE','PS2_FRAME_TIMELINE_CAPACITY','PS2_SPEED_LOG','PS2_AC4_AUDIO_SYNC_TRACE')
$saved = @{}
foreach ($name in $envNames) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
Push-Location $PSScriptRoot
try {
    foreach ($name in $envNames) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PATH = (Join-Path $compilerRoot 'bin') + ';' + $saved['PATH']
    $env:PS2_PIPELINE_CACHE = $saved['PS2_PIPELINE_CACHE']
    if ($Action -eq 'Build') {
        if ($Jobs -lt 1) { throw 'Jobs must be positive.' }
        $env:PYTHONPATH = Join-Path $PSScriptRoot 'tools'
        New-Item -ItemType Directory -Force (Join-Path $workRoot 'generated') | Out-Null
        $programRoot = Join-Path $workRoot 'programs'
        if (-not (Test-Path (Join-Path $programRoot 'manifest.json'))) { $programRoot = Join-Path $workRoot 'boot-programs' }
        & (Join-Path $compilerRoot 'bin/python.exe') -m vurecomp --programs $programRoot -o (Join-Path $workRoot 'generated/ps2_vu1_progs.inc')
        if ($LASTEXITCODE -ne 0) { throw 'VU generation failed.' }
        & (Join-Path $compilerRoot 'bin/cmake.exe') -S $PSScriptRoot -B $buildRoot -G Ninja -DCMAKE_BUILD_TYPE=Release ('-DPS2_DIAG='+$(if ($Diagnostics) { 'ON' } else { 'OFF' })) -DPS2_GENERATED_OPT=-O1 ('-DPS2_VU_PROGRAM_DIR='+ (Join-Path $workRoot 'generated')) ('-DCMAKE_C_COMPILER='+$compilerRoot+'/bin/gcc.exe') ('-DCMAKE_CXX_COMPILER='+$compilerRoot+'/bin/g++.exe') ('-DCMAKE_MAKE_PROGRAM='+$compilerRoot+'/bin/ninja.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Native POC configuration failed.' }
        & (Join-Path $compilerRoot 'bin/cmake.exe') --build $buildRoot --target ac4 gsreplay -j $Jobs
        if ($LASTEXITCODE -ne 0) { throw 'Native POC build failed.' }
    } else {
        $exe = Join-Path $buildRoot 'ac4.exe'
        if (-not (Test-Path -LiteralPath $exe)) { throw 'Run .\native-poc.ps1 -Action Build first.' }
        $settings = Join-Path $buildRoot 'ac4_settings.ini'
        if (-not (Test-Path -LiteralPath $settings)) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'build/ac4/ac4_settings.ini') -Destination $settings }
        $runRoot = Join-Path $workRoot ('runs/'+(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'-'+$PerformanceMode+'-'+$Action)
        New-Item -ItemType Directory -Force $runRoot | Out-Null
        if ($ColdCache) {
            # A fresh prefix isolates this run without deleting existing caches.
            $env:PS2_PIPELINE_CACHE = Join-Path $runRoot ('cold-pipelines-'+[Guid]::NewGuid().ToString('N'))
            if ((Test-Path -LiteralPath ($env:PS2_PIPELINE_CACHE+'.cache')) -or (Test-Path -LiteralPath ($env:PS2_PIPELINE_CACHE+'.keys'))) {
                throw 'Fresh pipeline-cache path unexpectedly exists.'
            }
            Write-Host 'Cold application cache: fresh for this run. GPU driver cache is unchanged.'
        }
        $env:PS2_SHADER_DIR = Join-Path $buildRoot 'shaders'
        $env:PS2_SETTINGS_FILE = $settings
        $env:PS2_VU_RECOMP = '1'
        if ($DiscCache -eq 'Memory') { $env:PS2_DISC_PRELOAD = '1'; Write-Host 'Preloading ISO into RAM before gameplay (about 3.3 GiB for AC4).' }
        $env:PS2_PRESENT_QUEUE = '1'
        if (-not $LegacyTextureDecode) {
            $env:PS2_T4_PARTIAL = '1'
            $env:PS2_T4_SOURCE_HASH = '1'
        }
        $env:PS2_PIPELINE_PREWARM = if ($StartupPreparation -eq 'On') { '1' } else { '0' }
        if ($StartupPreparation -eq 'On' -and -not $NoImagePrewarm) {
            # Mission 1 needs 16 targets (one already exists) and eight snapshots.
            $env:PS2_IMAGE_PREWARM = '15'
            $env:PS2_SNAPSHOT_PREWARM = '8'
            $env:PS2_RT_RESERVE_HEIGHT = '896'
        }
        $env:PS2_VU_VERIFY = if ($Action -eq 'Verify') { '1' } else { '0' }
        $env:PS2_SHOT_EVERY = '0'
        $env:PS2_AC4_OPENING = $Opening
        if ($FpsLimit -ge 0) { $env:PS2_FPS_CAP = [string]$FpsLimit }
        if ($FpsLimit -eq 120) { Write-Host '120 is a cap test only; the current renderer produces at most 59.94 new game frames/s.' }
        $fields = 0
        if ($BootTest -or $Action -eq 'Verify') {
            $env:PS2_PAD_PLAY = Join-Path $workRoot 'boot-input.pad'
            $env:PS2_PAD_PLAY_STOP = '0'
            $fields = 1800
            if (-not $PSBoundParameters.ContainsKey('Opening')) { $env:PS2_AC4_OPENING = 'GameDefault' }
        }
        $capLabel = if ($FpsLimit -lt 0) { "F4 settings" } else { [string]$FpsLimit }
        Write-Host "Startup preparation: $StartupPreparation; texture optimization: $(-not $LegacyTextureDecode)."
        Write-Host "Opening: $env:PS2_AC4_OPENING; presentation cap: $capLabel. Game clock remains 59.94 Hz."
        $runArgs = @('--data',(Join-Path $PSScriptRoot 'generated'),'--disc',([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Ace Combat 04 - Shattered Skies (USA).iso'))),'--seconds','0','--vblanks',"$fields",'--watchdog','30')
        if ($Hidden) { $runArgs += '--hidden' }
        Write-Host "Native VU POC; log: $runRoot/game.log"
        Write-Host 'Unrecognized vector programs use interpreter fallback. F4 settings; Escape exits.'
        if ($TraceCutscenes) {
            $env:PS2_PAD_RECORD = Join-Path $runRoot 'input.pad'
            $env:PS2_CLOCK_TIMELINE = Join-Path $runRoot 'clock.csv'
            $env:PS2_AC4_AUDIO_CONTROL_TRACE = '1'
            $env:PS2_AC4_MOVIE_LIFETIME_TRACE = '1'
            $env:PS2_AC4_MOVIE_REFILL_TRACE = '1'
            $env:PS2_AC4_AUDIO_SYNC_TRACE = '1'
            Write-Host 'Cutscene trace: controller inputs, audio controls, and first 65536 guest clock events.'
        }
        if ($Metrics) {
            $env:PS2_FRAME_TIMELINE = Join-Path $runRoot 'frames.csv'
            $env:PS2_FRAME_TIMELINE_CAPACITY = [string]($MetricsMinutes * 60 * 60)
            $env:PS2_SPEED_LOG = '1'
            $env:PS2_AC4_AUDIO_SYNC_TRACE = '1'
            Copy-Item -LiteralPath $settings -Destination (Join-Path $runRoot 'settings-at-start.ini')
            $metricConfig = [ordered]@{
                started_utc = [DateTime]::UtcNow.ToString('o')
                trace_cutscenes = [bool]$TraceCutscenes
                disc_cache = $DiscCache
                performance_mode = $PerformanceMode
                startup_preparation = $StartupPreparation
                image_prewarm = ($StartupPreparation -eq 'On' -and -not $NoImagePrewarm)
                prewarm_target_pairs = $env:PS2_IMAGE_PREWARM
                prewarm_snapshots = $env:PS2_SNAPSHOT_PREWARM
                texture_optimization = (-not $LegacyTextureDecode)
                opening = $Opening; fps_limit = $FpsLimit
                audio_sync_trace = $true
                timeline_capacity = $MetricsMinutes * 60 * 60
                executable_sha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
                settings_sha256 = (Get-FileHash -LiteralPath $settings -Algorithm SHA256).Hash
                cold_application_cache = [bool]$ColdCache
                pipeline_cache_override = $env:PS2_PIPELINE_CACHE
                note = 'Manual whole-run capture, including menus/loading; CPU submissions, not monitor scanout. Driver cache is unchanged; cold_application_cache records whether this run uses a fresh isolated application cache.'
            }
            $metricConfig | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 (Join-Path $runRoot 'metrics-config.json')
            Write-Host "Metrics: $runRoot (up to $MetricsMinutes minutes at 60 fps). Exit with Escape to save the timeline."
        }
        $oldAction = $ErrorActionPreference
        $oldNativeAction = $PSNativeCommandUseErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $PSNativeCommandUseErrorActionPreference = $false
            & $exe @runArgs *> (Join-Path $runRoot 'game.log')
            $runExit = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $oldAction
            $PSNativeCommandUseErrorActionPreference = $oldNativeAction
        }
        if ($Metrics) {
            [ordered]@{ ended_utc = [DateTime]::UtcNow.ToString('o'); exit_code = $runExit } | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $runRoot 'metrics-exit.json')
            & (Join-Path $compilerRoot 'bin/python.exe') (Join-Path $PSScriptRoot 'tools/native_run_metrics.py') $runRoot
            if ($LASTEXITCODE -ne 0) { Write-Warning "Metrics summary failed; raw logs remain in $runRoot." }
        }
        if ($runExit -ne 0) { throw "Native POC exited $runExit; see $runRoot/game.log" }
        if ($Action -eq 'Verify') {
            $log = Get-Content -Raw (Join-Path $runRoot 'game.log')
            if ($log -notmatch 'vu1 verify: [1-9][0-9]* activations run BOTH ways and compared, 0 mismatched') { throw 'Native VU verification did not pass.' }
            Write-Host 'Native VU verification passed.'
        }
    }
} finally {
    Pop-Location
    foreach ($name in $envNames) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
}
