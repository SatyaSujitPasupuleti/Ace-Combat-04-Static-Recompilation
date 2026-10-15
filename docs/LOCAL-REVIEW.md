# Local review before publishing

The owner confirmed the first-use build succeeded and approved source publication on 2026-10-14. Local build/play outputs were cleared before publication. This folder uses fresh Git history; the original development checkout and its history are not being pushed.

Review `README.md`, `CREDITS.md`, `LICENSE`, and `docs/BUILD.md`. The source contains AC5-derived code and explicitly credits it. The original upstream README is retained as historical documentation, not a claim of AC4 feature parity.

For local play after building:

```powershell
.\play.cmd -DiscCache memory
```

The launcher detects Python from the saved toolchain or installed Python. On a fresh checkout, run `build.cmd` with your own ISO to create ignored local configuration. The runtime/source files are copies of the accepted development build; public setup/build documentation is new. The original development checkout is unchanged.

Game data, generated sources, executables, VU captures, settings, saves, and local machine paths are excluded from Git. Only source and documentation are included in Git. No game executable is planned as a release asset.
