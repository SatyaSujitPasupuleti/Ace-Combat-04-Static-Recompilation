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
