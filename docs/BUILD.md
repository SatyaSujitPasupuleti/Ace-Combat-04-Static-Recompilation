# Building on Windows

## Requirements

- 64-bit Windows, a Vulkan-capable GPU and current vendor driver.
- Your own NTSC-U Ace Combat 04 ISO containing `SLUS_201.52`.
- Python 3.10 or newer (the UCRT64 Python package below works, or use Python for Windows).
- MSYS2 UCRT64 toolchain. The working development build uses GCC 16.2. A compiler supporting `[[gnu::musttail]]` is required; CMake checks it.

Install [MSYS2](https://www.msys2.org/). In its **UCRT64** terminal, update packages following MSYS2's instructions, then install:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-sdl3 mingw-w64-ucrt-x86_64-vulkan-headers mingw-w64-ucrt-x86_64-vulkan-loader mingw-w64-ucrt-x86_64-shaderc
```

The Python build tool uses only the standard library. No Ghidra installation is needed. Clone this repository or extract its source ZIP to a writable directory. Run the commands below from that directory in PowerShell. Use `python --version` to check that Python is installed.

## Easy build and play

**Drag and drop one ISO onto `build.cmd`** in Explorer to select it and start building. Alternatively, double-click **build.cmd** to use the ISO picker. Dropped paths containing spaces are supported; the selected ISO path is remembered for future builds. Drop the ISO onto the script file, not onto an already-running terminal window. It reuses saved paths on subsequent runs. On first use it detects `C:\msys64\ucrt64` or a toolchain on PATH (or asks for its location), finds Python, and opens an ISO file picker. It verifies the executable, generates code and builds a playable executable. Compilation can take several minutes. If anything fails, later stages stop and the terminal stays open so you can read the error.

Then double-click **play.cmd**. It supplies ISO/data/shader paths and the DLL search path automatically. It prefers a built native VU executable, otherwise bootstrap. The launchers support paths containing spaces and do not permanently change your system PATH or execution policy.

Optional native VU optimization:

```powershell
.\build.cmd -Action Optimize
```

This builds the capture executable, launches a play session, and compiles captured VU programs after you exit normally with Escape. Play through menus, briefing and flight for useful coverage. The interpreted-VU session can be slower. Each capture replaces the previous manifest; back up `out/vu_programs` before replacing a valuable capture. Unseen programs still use interpretation. Performance depends on capture coverage.

Subsequent builds use the captured corpus automatically. The launchers do not install dependencies; the one-time manual MSYS2 step above follows AC5's approach.

Useful PowerShell commands:

```powershell
# Explicit paths, no prompts:
.\build.ps1 -Iso "D:\Games\AC04.iso" -Toolchain "C:\msys64\ucrt64" -Jobs 4 -NonInteractive
# Check saved paths/tool binaries and ISO hash without generating or building:
.\build.ps1 -Action Check -NonInteractive
# Play with optional RAM disc cache:
.\build.ps1 -Action Play -DiscCache memory
```

If PowerShell policy blocks a direct `.ps1` invocation, use `build.cmd` with the same flags. Its wrapper applies a process-only policy setting. The build console pauses before closing; automation can set `AC4_NO_PAUSE=1` when using `.cmd`, or call `.ps1` directly. `play.cmd` pauses only on failure.

The underlying Python commands below remain available for developers and troubleshooting. If Python is only installed inside MSYS2, use that installation's `ucrt64/bin/python.exe` for those commands; the easy launcher detects it automatically.

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

The run command defaults to 60 FPS, original game timing, startup preparation and optimized texture decoding. It selects the native build if available, otherwise bootstrap; use `--renderer bootstrap` or `--renderer native` to select explicitly. F4 opens graphics settings; Escape exits. The ISO is streamed by default; `run --disc-cache memory` preloads it into RAM (roughly 3.3 GiB additional memory). Settings and saves are local and ignored. There is no frame-generation mode in this workflow.

## Limitations and validation

Movie audio was accepted by the user on the preserved development build after playing through Mission 2. This does not certify the full campaign. Performance spikes remain, particularly first-use shaders and scene preparation. Capture-VU mode intentionally runs more slowly and is not a native performance benchmark.

The publication workflow is validated separately from gameplay. See the repository release notes for the exact validation performed; do not assume a full clean-room campaign test. Older `native-poc.ps1`/`ac4.ps1` scripts are developer workflows with a sibling `toolchain/msys64` layout; use `setup_ac4.py` for configurable public installs.
