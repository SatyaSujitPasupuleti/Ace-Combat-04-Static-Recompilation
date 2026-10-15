# Ace Combat 04 Static Recompilation

Experimental Windows recompilation of **Ace Combat 04: Shattered Skies**, NTSC-U (`SLUS_201.52`). Bring your own game ISO; build the game code locally.

## Credits and provenance

This project **contains code derived from [sal063/Ace-Combat-5-Static-recompilation](https://github.com/sal063/Ace-Combat-5-Static-recompilation)**, starting from commit `078662b4018b5e79d5aa5ee9b9c60aceec9e7649`. Its PS2Recomp tooling, console runtime, Vulkan renderer, native rendering infrastructure, and build tools provided the foundation. This repository starts with a fresh commit history; that does not imply independently authored code. AC4-specific configuration and runtime changes adapt that foundation to a different game.

The inherited [Apache-2.0 license](LICENSE) and third-party notices are retained. See [credits](CREDITS.md) and the [original AC5 README](docs/AC5-UPSTREAM-README.md). The original README describes AC5, **not AC4 compatibility**. This is an unofficial project, not affiliated with or endorsed by Bandai Namco.

## Current status

- User-tested through Mission 2; movie audio ordering and synchronization confirmed working on the current development build.
- Terrain, sun, general rendering, and pause-menu actions have received user acceptance.
- Native CPU recompilation and optional native VU recompilation, with interpreter fallback for uncaptured vector programs.
- Vulkan rendering through the GS compatibility path. The inherited AC5 direct-mesh/HUD replacements are not enabled for AC4.
- 4K output defaults, automatic internal scaling, widescreen projection, 16x anisotropy, and F4 graphics settings. Original textures remain original-resolution assets.
- Default game clock is 59.94 Hz with a 60 FPS presentation cap; frame generation is not used.

This is a **proof of concept**, not a fully validated PC port. Frame-time spikes remain; locked 60 FPS is not guaranteed. Save/load and Alt-Tab need broader testing. Only the listed US executable is supported. Full campaign compatibility has not been established. Higher-resolution HUD/text and a game-specific mesh renderer are future work.

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

## Reporting problems

Include your GPU/driver, Windows version, game executable hash, build commit, scene/mission, reproduction steps, and whether the issue occurs with a native VU build. Check logs for private paths before sharing. Do not attach ISOs, extracted game files, saves, generated sources, or raw game captures to issues.
