# Testing

## One command

On Windows, with Python 3.10+ installed:

```powershell
.\build.cmd -Action Test
# Equivalent for terminals/CI:
python tools/run_tests.py --suite all
```

No ISO, GPU, compiler, MSYS2 packages, or generated game files are needed for these suites. The launcher can use a Python installed inside MSYS2, but it does not require MSYS2 for testing. It uses saved toolchain information only to help find Python. Exit code 0 means success; nonzero means failure. To select a suite, use `--suite unit` / `--suite acceptance`, or `build.cmd -Action Test -TestSuite unit`.

Unit tests run on other platforms too. Acceptance tests require Windows PowerShell and cmd; the runner explicitly rejects running that suite on unsupported platforms instead of reporting a misleading pass. GitHub Actions is configured to run both suites on Windows for pushes and pull requests; local preparation does not imply that remote CI has run.

## Coverage

- **Unit:** synthetic ISO verification/extraction, wrong or altered executables, malformed/truncated input, launch selection, environment isolation, and capture output-directory setup. Existing launcher unit tests remain in `tools/test_setup_ac4.py`; additional units live in `tests/unit`.
- **Automated acceptance:** actual `.cmd` and PowerShell launchers, dropped ISO arguments with spaces/special characters, saved-path builds, native capture selection, optimization step order, stopping on failures, missing inputs, check-only behavior, and play options. These tests use a recording helper in a temporary workspace to avoid compiling or executing a game. They verify orchestration, not successful gameplay or compiler correctness.
- **Native/runtime regressions:** existing `tools/test_ac4_*`, GS, texture and VU tests are separate targeted development tests. Several require a compiler, the legacy toolchain layout, or private local inputs. They are not blindly discovered by the public runner. Run the applicable tests when changing those subsystems and document their prerequisites/results.

Tests create disposable synthetic fixtures below ignored `work/`; they never read or overwrite your save files. Keep public fixtures synthetic and deterministic. Acceptance tests use timeouts to fail instead of hanging on an unexpected prompt.

## Local game acceptance

For changes affecting game behavior, complete the relevant scenarios using your own supported ISO and record expected versus observed behavior:

| Change area | Minimum applicable scenario and pass condition |
| --- | --- |
| Build/install | Fresh source folder, ISO verification, bootstrap build, normal boot; capture through briefing/flight, exit normally, native build and boot. Incorrect ISO must stop before generation/build. |
| Movies/audio | New Game, uninterrupted playback of both story movies; dialogue order and subtitle/video synchronization remain correct. Check skipping separately. |
| Renderer/textures | Opening, live briefing/hangar, Mission 1 island/airbase, ground/sun and third-person aircraft; no new missing geometry, corruption or flicker. |
| Menus/input | Start/confirm/back, pause and Return to Game; no unexpected exit or freeze. |
| Performance | Comparable route, resolution, cache state and background load; report frame-time distribution and worst stalls, with scene context. An average near 60 is not proof of locked 60. |
| Save/load/focus | If affected, use a disposable save copy, verify save/reload and Alt-Tab restoration. These areas remain incompletely validated globally. |

Record OS/GPU/driver, build commit, settings and cache state, route, failures, and which checks were not run. Do not upload private game files or raw captures as test evidence. Written observations and sanitized aggregate metrics are sufficient for a public report. These scenarios are requirements for affected changes, not claims that all have passed on the current project.

## Review checklist

Confirm that tests would fail for the reported regression, cover a meaningful failure path, do not depend on a developer-specific absolute path, and do not silently accept a missing test suite. Review modified assertions carefully. Require final-run evidence and relevant game acceptance before accepting a change. See [CONTRIBUTING.md](CONTRIBUTING.md).
