# Ace Combat 04 Static Recompilation

Experimental Windows recompilation of **Ace Combat 04: Shattered Skies**, NTSC-U (`SLUS_201.52`). Bring your own game ISO; build the game code locally.

V3 development: see the [hybrid renderer experiment](docs/V3-RENDERER.md). The opt-in hybrid path adds AC4-specific native geometry and HUD rendering; normal play retains the working renderer.

## Credits and provenance

This project **contains code derived from [sal063/Ace-Combat-5-Static-recompilation](https://github.com/sal063/Ace-Combat-5-Static-recompilation)**, starting from commit `078662b4018b5e79d5aa5ee9b9c60aceec9e7649`. Its PS2Recomp tooling, console runtime, Vulkan renderer, native rendering infrastructure, and build tools provided the foundation. This repository starts with a fresh commit history; that does not imply independently authored code. AC4-specific configuration and runtime changes adapt that foundation to a different game.

The inherited [Apache-2.0 license](LICENSE) and third-party notices are retained. See [credits](CREDITS.md) and the [original AC5 README](docs/AC5-UPSTREAM-README.md). The original README describes AC5, **not AC4 compatibility**. This is an unofficial project, not affiliated with or endorsed by Bandai Namco.

## Current status

- User-tested through Mission 2; movie audio ordering and synchronization confirmed working on the current development build.
- Terrain, sun, general rendering, and pause-menu actions have received user acceptance.
- Native CPU recompilation and optional native VU recompilation, with interpreter fallback for uncaptured vector programs.
- Vulkan rendering with a GS compatibility default and an opt-in AC4 hybrid mesh/HUD renderer (see below).
- 4K output defaults, automatic internal scaling, widescreen projection, 16x anisotropy, and F4 graphics settings. Original textures remain original-resolution assets.
- Default game clock is 59.94 Hz with a 60 FPS presentation cap; frame generation is not used.
- Ordered AC4 field handoff preserves queued GS/native draws under renderer delays; unsupported rendering states still use GS fallback.

This is a **proof of concept**, not a fully validated PC port. Frame-time spikes remain; locked 60 FPS is not guaranteed. Save/load and Alt-Tab need broader testing. Only the listed US executable is supported. Full campaign compatibility has not been established. The hybrid renderer remains experimental; higher-resolution replacement artwork and broader texture modding remain future work.

One 544.8-second development run averaged 59.95 CPU submissions/s, with p99 23.95 ms and a later 66.18 ms stall associated with shader compilation. These are whole-run measurements, not a gameplay-only benchmark or monitor scanout results. A fresh machine/captured VU corpus may perform differently.

## Build and play

Read the [Windows build guide](docs/BUILD.md). The repository does not ship a game executable, ISO, extracted game files, generated game code, captured VU images, or memory-card saves. You generate these locally from your own copy.

After installing the prerequisites in the [build guide](docs/BUILD.md):

1. **Drag your ISO onto `build.cmd`**, or double-click **`build.cmd`** to choose it. It detects Python/MSYS2, asks you to select your ISO on first use, verifies the supported executable, generates the code and builds it.
2. Double-click **`play.cmd`** to play. No terminal environment setup is needed.
3. Optional: run `build.cmd -Action Optimize` to capture VU programs during a play session and compile them after you exit with Escape. Future builds reuse that local capture.

For a scripted build:

```powershell
.\build.ps1 -Iso "D:\Games\AC04.iso" -Toolchain "C:\msys64\ucrt64" -NonInteractive
.\build.ps1 -Action Play
```

This follows AC5's verify, generate, CMake build and play workflow, with VU capture as an optional optimization. A first build is playable with interpreted VU execution. Native VU performance requires a representative capture: opening, menus, briefing and flight. Unseen programs retain interpreter fallback. Captures and all generated game files stay local and ignored by Git. The scripts do not install packages or download game data.

## Experimental V3 hybrid renderer

With a native build already available:

```powershell
.\play.cmd -HybridRenderer
```

This opts into native model/terrain/cockpit processing, GPU meshes and materials,
guarded mesh-packet decoding, native mipmapped texture filtering, and
window-resolution HUD/radar/radio composition, including grouped speed/altitude
brackets and normal radar backgrounds, masks and icons. Menus,
effects and unsupported states retain the GS path. It uses your saved resolution
and anisotropy settings; F4 opens the PC settings panel.

This is an experimental combined path, not a declaration of AC5 parity. Captured
rendering and a live Mission 1 route have been tested, but broader missions,
continuous visual/audio review and interactive settings transitions remain open.
Run `play.cmd` without the switch to return to the existing default. The switch
does not rewrite your saved renderer choice or modify game assets. Capture/Optimize
continues using original execution for VU collection. A missing native build is
reported instead of silently falling back; see the Optimize workflow above.

Full output resolution coverage is now enabled by default in hybrid mode:

```powershell
.\play.cmd -HybridRenderer
```

The older `-FullOutputResolution` switch remains accepted for compatibility.
Keep **F4 > Internal resolution** on **Auto**. This makes automatic scene and HUD
scaling cover both window dimensions. At 3840x2160, a 512x448 display crop uses
8x integer scaling (4096x3584), then downsamples to the window. It renders more
pixels and can substantially increase GPU cost. It does not install HD textures
or promise locked 60 FPS. Fixed internal-resolution selections remain respected;
hardware limits and allocation failures can still restrict the achieved scale.
The hybrid renderer remains opt-in; full output coverage is automatic within it.

## Reporting problems

Include your GPU/driver, Windows version, game executable hash, build commit, scene/mission, reproduction steps, and whether the issue occurs with a native VU build. Check logs for private paths before sharing. Do not attach ISOs, extracted game files, saves, generated sources, or raw game captures to issues.

## Contributing and tests

To contribute, fork the repository, create a focused branch, and open a pull request explaining the problem and resulting behavior. Discuss larger renderer or architecture changes in an issue first.

Contributors must write/update unit and acceptance tests, review their adequacy, and run them before submitting changes. Include the exact test commands/results and any manual game acceptance evidence in the pull request. Bug fixes should include a regression test; explain any checks you could not run. Do not submit game files, generated game code, captures, saves, or local configuration. Preserve upstream credits and license notices. Reviewers must inspect the tests and acceptance evidence before merge. See [CONTRIBUTING.md](CONTRIBUTING.md) and [TESTING.md](TESTING.md).

```powershell
.\build.cmd -Action Test
# Or:
python tools/run_tests.py --suite all
```

The public suites use synthetic inputs and do not require a game ISO or compiler. GitHub Actions is configured to run them on Windows. Game/runtime changes additionally require applicable native regression checks and local game acceptance; passing launcher tests does not certify gameplay.

For native hybrid full-4K Auto rendering, the runtime can prepare known loading
images before gameplay when the GPU driver reports sufficient memory budget.
This commits some GPU memory earlier and can reduce scene-transition allocation
pauses; it does not replace textures or reduce rendering resolution. Adapters
without budget reporting or sufficient headroom retain the smaller reserve.
Startup and frame-time logs distinguish preparation time from gameplay stalls.


### Experimental VU execution comparison

With a native build, compare CPU VU execution while retaining the hybrid GPU
renderer, full output resolution and the 60 FPS limit:

```powershell
.\play.cmd -HybridRenderer -VuExecution Aot
```

Return to the existing CPU policy with
`.\play.cmd -HybridRenderer -VuExecution Native`, or omit `-VuExecution`.
The AOT option disables the four model/terrain/cockpit CPU VU replacements and
uses captured microprograms compiled ahead of time. Native GPU meshes, materials,
textures and HUD stay enabled. Other HLE services stay active. Uncaptured programs
may still interpret; the exit report identifies missing images/entry points.
This is experimental scene-limited coverage, not a guarantee of locked 60 FPS.
See [renderer investigation notes](docs/AC5-PARITY.md) for measurements and limits.
