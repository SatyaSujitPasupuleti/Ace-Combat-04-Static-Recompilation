# Publication preparation validation — 2026-10-14



Performed locally before publication; not a full campaign or fresh-machine certification:



- Extracted the executable from the user-provided NTSC-U ISO, verified its supported SHA-256, and regenerated 5,029 functions.

- All 26 generated output hashes matched the accepted development manifest.

- All copied runtime source files matched the accepted development checkout.

- Built the bootstrap executable from the source-only package with GCC 16.2, Release / generated -O1.

- Completed a 600-field hidden bootstrap boot and captured two VU program images. The check exposed a missing output-directory setup in the helper; fixed and successfully repeated.

- Built native VU code from that newly captured corpus and completed another 600-field boot: 25,254 native VU runs, zero interpreted runs on this limited route.

- Confirmed malformed/non-ISO input is rejected.

- Verified the separate private working checkpoint's 326 recorded SHA-256 hashes.

- Audited staged source for game/binary outputs and common credential patterns; none found. Generated code/data, ISO files, VU images, executables, local configuration and saves are ignored.



For the owner's local review, the ignored VU corpus and graphics settings are subsequently restored from the accepted eight-program development checkpoint and the native build is rebuilt. Public users must capture their own corpus; coverage and performance will vary. These private files will not be published.



The accepted development build was manually played through Mission 2. The user confirmed movie audio is fixed; remaining frame drops were less noticeable, and their occurrence during gameplay is unconfirmed. The newly packaged build still requires the owner's manual validation before any push. No locked-60 or full-campaign claim is made.



Existing compiler sign-comparison warnings and whitespace warnings in inherited/vendor sources were not altered for packaging.


## Easy launcher validation

- Windows PowerShell 5.1: successful saved-path prerequisite/ISO check and complete generate/native-build workflow through `build.ps1`.
- `build.cmd` prerequisite check passed; non-interactive missing-ISO input returned failure.
- Five Python launch-routing tests passed: bootstrap fallback, native preference, explicit missing-native rejection, capture directory/routing, and developer environment isolation.
- Six isolated PowerShell orchestration cases passed using a stub helper: bootstrap build, captured-native build, optimization sequence, capture failure stopping before native build, preparation failure stopping before compilation, and play routing. The test repository path contained spaces.
- ISO file-picker interaction is left for manual review. No runtime/game behavior changes were introduced by this launcher update.

Drag-and-drop launcher argument validation: real ISO hash check passed via positional `.cmd` invocation. Isolated `.cmd` build-routing test passed with spaces in the repository/ISO paths and an ampersand in the ISO filename. Explorer mouse interaction itself remains a manual check.

## Contributor test suites

The public runner passed 21 tests locally: 12 units (launch behavior and synthetic ISO parsing/verification) and 9 Windows launcher acceptance scenarios. Reviewed fixture isolation, assertions, failure propagation and timeouts. The CI workflow is configured but has not run remotely because publication remains pending approval. These checks do not replace game acceptance or hardware-dependent runtime tests.

Toolchain-discovery regression: 23 public tests passed, including skipping a GCC-only installation and rejecting an explicit prefix missing CMake. A check-only invocation against the local ISO selected the complete project-local UCRT64 installation and verified the supported executable without generating/building the game.
