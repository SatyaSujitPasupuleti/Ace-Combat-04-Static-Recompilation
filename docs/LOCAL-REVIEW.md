# Local review before publishing

Publication is paused pending the owner's review. A public empty repository has been created, but no source commits have been pushed. This folder uses fresh Git history; the original development checkout and its history are not being pushed.

Review `README.md`, `CREDITS.md`, `LICENSE`, and `docs/BUILD.md`. The source contains AC5-derived code and explicitly credits it. The original upstream README is retained as historical documentation, not a claim of AC4 feature parity.

For local play after building:

```powershell
python tools/setup_ac4.py run --disc-cache memory
```

If `python` is not available on PATH, use a Python 3 executable's full path. The ignored `local.json` already points to the developer's ISO and MSYS2 installation on this machine. The runtime/source files are copies of the accepted development build; public setup/build documentation is new. The original development checkout is unchanged.

Game data, generated sources, executables, VU captures, settings, saves, and local machine paths are excluded from Git. Only reviewed source and documentation will be pushed after approval. No game executable is planned as a release asset.
