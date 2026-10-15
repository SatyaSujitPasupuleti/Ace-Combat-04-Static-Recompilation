# Building on Windows

## Requirements

- 64-bit Windows, a Vulkan-capable GPU and current vendor driver.
- Your own NTSC-U Ace Combat 04 ISO containing `SLUS_201.52`.
- Python 3.10 or newer on PATH.
- MSYS2 UCRT64 toolchain. The working development build uses GCC 16.2. A compiler supporting `[[gnu::musttail]]` is required; CMake checks it.

Install [MSYS2](https://www.msys2.org/). In its **UCRT64** terminal, update packages following MSYS2's instructions, then install:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-sdl3 mingw-w64-ucrt-x86_64-vulkan-headers mingw-w64-ucrt-x86_64-vulkan-loader mingw-w64-ucrt-x86_64-shaderc
```

The Python build tool uses only the standard library. No Ghidra installation is needed. Clone this repository or extract its source ZIP to a writable directory. Run the commands below from that directory in PowerShell. Use `python --version` to check that Python is installed.

## Prepare from your ISO

```powershell
python tools/setup_ac4.py prepare --iso "D:\Games\AC04.iso" --toolchain "C:\msys64\ucrt64"
```

The tool reads ISO9660 directly, extracts only the main executable into ignored local work storage, and verifies this exact SHA-256 before recompiling:

```text
38ecebd6965c2f74ec8973f538781506bf3a2cfad86f908deb6c5d585d528301
```

Other regions/revisions and modified executable images are unsupported. The ISO stays at the supplied path and is read at runtime. Local paths are saved in ignored `local.json`; do not upload that file.

## Build, capture VU programs, then build native VU

```powershell
python tools/setup_ac4.py build --jobs 4
python tools/setup_ac4.py capture-vu
```

Play through scenes you want covered: opening, menus, briefing/hangar, and flight. **Exit with Escape** so the runtime can save `out/vu_programs/manifest.json` and its program images. Do not kill the process. Capture files contain game microcode and remain ignored. Each capture replaces the manifest with that session's coverage, so use a representative continuous session.

```powershell
python tools/setup_ac4.py build-native --jobs 4
python tools/setup_ac4.py run
```

`build` makes `build/bootstrap/ac4.exe` with VU interpretation. `build-native` produces `build/native-poc/ac4.exe` with your captured VU programs compiled as native code. Unseen/unsupported programs fall back to interpretation. The public repository intentionally excludes the developer's captured program images; capture coverage affects performance.

The run command defaults to 60 FPS, original game timing, native VU enabled, startup preparation and optimized texture decoding. F4 opens graphics settings; Escape exits. The ISO is streamed by default; `run --disc-cache memory` preloads it into RAM (roughly 3.3 GiB additional memory). Settings and saves are local and ignored. There is no frame-generation mode in this workflow.

## Limitations and validation

Movie audio was accepted by the user on the preserved development build after playing through Mission 2. This does not certify the full campaign. Performance spikes remain, particularly first-use shaders and scene preparation. Capture-VU mode intentionally runs more slowly and is not a native performance benchmark.

The publication workflow is validated separately from gameplay. See the repository release notes for the exact validation performed; do not assume a full clean-room campaign test. Older `native-poc.ps1`/`ac4.ps1` scripts are developer workflows with a sibling `toolchain/msys64` layout; use `setup_ac4.py` for configurable public installs.
