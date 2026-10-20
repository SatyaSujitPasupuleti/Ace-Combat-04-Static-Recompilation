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


## V3 renderer changes

For HUD placement research, record `PS2_RN_2D_LOG` privately and run
`python tools/analyze_ac4_hud_layout.py layout.txt --generated generated --output producers.json`.
The report maps sprite return addresses to generated parent functions and retains
observed bounds. It does not classify fixed/world elements or establish coverage
of numeric glyphs, lines, radar or captions. Review pixels and producer code before
using the report to change placement. Keep captures and generated source private.

Run `python tools/run_renderer_tests.py` in addition to the public suite when changing the native model calculation or mesh bridge. It uses the toolchain saved in `local.json`; alternatively supply `--cc` with the path to UCRT64 `gcc.exe`. All twenty-five C test executables use synthetic data and require neither an ISO nor a GPU. These compiler-dependent checks are separate from the public Python/launcher CI suite.

Real rendering changes additionally require the capture and gameplay comparisons in [the V3 renderer notes](docs/V3-RENDERER.md). An exact native-calculation comparison only validates its supported subset; do not count excluded clipping/material paths as covered.

GPU model replacement also has an opt-in local acceptance command after building `gsreplay` and shaders:

```powershell
python tools/test_ac4_model_gpu.py "path/to/private-model-capture.gscap" --fields 8
```

It requires the saved local toolchain configuration, Vulkan, and a capture exercising the recognized AC4 model programs. It runs baseline, audited model-mesh and non-audited model-mesh modes in an isolated directory under ignored `work/`, checks that verified GPU model draws occurred, checks the non-audited mode avoids reference evaluations and exercises specialized GIF decoding, and requires byte-identical final PPM output. A mismatch or unexercised path fails. Logs and images remain local. Review the images for visible content; matching black images are not visual acceptance. This does not compare every intervening frame, validate audio, or replace gameplay acceptance.

For native model-material changes, add `--materials` to the GPU comparison command. This adds a fourth run, requires native fragment-shader usage and exact final-image parity. Play locally with `play.cmd -ModelMaterials`; this implies `-ModelMesh`. Compare with `play.cmd -ModelMesh` and default play. See the explicit remaining gates in [AC5-PARITY.md](docs/AC5-PARITY.md).


Native texture adapter: use `play.cmd -ModelTextures` and compare with `play.cmd -ModelMaterials` over the same hangar/flight route. Verify aircraft markings, region clamps, distant minification, island/sun fallback and transitions. This option implies materials/meshes, stays off by default and is disabled for capture. Filtering can intentionally change pixels; exact material parity does not substitute for texture visual acceptance. Native C tests cover crop bounds, unsupported state, source immutability and allocation fallback. GPU parity rejects uniform final frames; a nonuniform frame still requires visual inspection and does not prove every replayed frame.


For the native unlit packet producer, add `--unlit-plan` to the private GPU comparison. The extra run must exercise unlit activations and match ordered packets, final memory, mapped controls and predicted registers; zero coverage fails even if the screenshot matches. This switch performs shadow verification: delayed arithmetic flags/pipeline state, Q/P pending state and original instruction counts are compared while original VU execution remains enabled. Actual bypass is tested separately with `--native-unlit`; clipping support is still pending. The hangar capture exercises this path; the current third-person flight capture does not.


Native unlit execution: `python tools/test_ac4_model_gpu.py <private-capture> --fields 8 --native-unlit --materials` adds original-execution shadow verification and actual bypass modes with/without native materials. Require matching nonzero activation/packet coverage, exact original instruction counts in the shadow run, and identical visible final frames. Launch with `play.cmd -NativeUnlit`; combine with `-ModelMaterials` or `-ModelTextures` for separate material/filtering acceptance. Unsupported geometry retains original execution. This subset still submits GIF packets, so do not describe it as full VU/GIF bypass.


Native unlit culling: add `--culling --materials` to the GPU comparison. This implies native execution tests and requires positive culling coverage with matching shadow/native counts, in addition to the existing packet/memory/register/flag/cycle/image gates. Synthetic tests cover all six planes and preserve empty submissions. Mixed and partially clipped batches must continue to fall back.


Partial clipping: `--clipping` adds a positive-coverage gate for independently produced clipped packets and scratch-buffer writes. Combine with `--culling --materials` to check existing native bypass in the same route. Partial-clipping packets are still rendered by original VU execution; complete continuation state is not yet validated for bypass.


The partial-clipping gate also requires zero integer-register mismatches at every packet submission. Registers are compared before the outer triangle index advances. The gate also requires zero traced vector-state mismatches. This checks written temporary vectors, accumulator, I and Q when produced by the kernel, but does not yet verify the delayed arithmetic pipeline or full continuation state.

The clipping gate additionally checks Q/P and pending-value storage, clip history, final MAC and current zero/sign flags, and empty flag-pipeline valid bits. Run private clipping replay with `PS2_VU_QLAT=0` as well to cover immediate-Q pending-storage semantics. Sticky status, inactive slot payloads and exact clipping cycle counts remain unverified; passing these checks does not enable partial-clipping bypass.

Clipping acceptance now also requires nonzero activation-level instruction-count coverage with zero mismatches, including mixed ordinary, culled and clipped triangles. Instruction-count agreement is not wall-clock performance evidence. Full sticky arithmetic and complete mixed-activation continuation remain prerequisites for enabling native clipping.

The clipping gate now requires complete mixed-activation arithmetic-state agreement: status including sticky bits, MAC, final slot index and all four flag-pipeline payloads/valid bits. This supersedes the earlier drain-only check. Complete mixed-activation memory and preserved-register assembly still needs validation before clipping bypass is enabled.

Mixed clipping acceptance now includes the complete final 16 KiB VU memory image and independently counted packet sequence/address checks, covering ordinary and culled packets surrounding clipping. Untouched memory must remain identical. Whole-activation preserved-register assembly is still pending before native mixed execution.

The mixed clipping gate now requires complete final register-state agreement in addition to memory, arithmetic and instruction counts. Preserved registers are carried from activation input across each packet. Actual mixed native execution still requires separate opt-in execution tests; shadow parity alone does not establish bypass coverage.

Native mixed clipping is now covered by `--clipping --native-unlit --materials` (or `--clipping --culling --materials`). Require matching positive shadow/native mixed activation, packet and instruction counts, full shadow state parity and identical final images. Repeat with `PS2_VU_QLAT=0`. `play.cmd -NativeUnlit` enables this path; it still submits GIF and keeps clipped geometry on GS drawing. Check interactive hangar/mission transitions separately; a final-image replay comparison does not cover every frame.

Mixed execution verification includes split unlit batches with no clipped output as well as partially clipped batches. Keep shadow-eligible and actual-native activation/packet/instruction counts equal. Full coverage claims must name the tested capture; they do not establish unobserved programs or scenes.

Lit secondary stream: `python tools/test_ac4_model_gpu.py <private-capture> --fields 8 --lit-plan --materials`. Requires positive lit vertex coverage and exact secondary STQ/colour/geometry (ADC excluded) plus both stream headers. This is shadow-only, with clipped submissions skipped. It does not require unlit coverage, so use the third-person flight capture as well as the hangar. Full lit execution remains on the original path.

Lit geometry checks now include ADC/backface suppression in both streams. `--lit-plan` rejects missing/mismatched ADC reports. Record the marked-vertex count when claiming suppression coverage; a scene with none cannot validate the active branch. Synthetic tests cover the fractional-fog rounding difference between float flag addition and packed-bit OR. Clipped lit polygons remain outside this gate's current scope.

`--lit-plan` now also compares every word of both complete material packets, including the primary stream. Positive packet coverage and zero complete-packet mismatches are mandatory. Lit clipping and final continuation state remain unverified by this gate.

Clipped lit output: add `--lit-clipping` (implies `--lit-plan`) to require positive complete clipped-packet coverage, matching clipping scratch writes and zero declines. Track empty-result coverage separately. This gate does not yet cover winding scratch qwords 0/1 or complete lit continuation state.

The lit-plan replay gate also checks the full 16 KiB activation memory, winding scratch, independently selected packet order/banks and every packet. Positive activation and packet counts are required; any mismatch or declined prediction fails acceptance. This is shadow evidence, not native lit execution or frame-time evidence.

Lit-plan acceptance additionally requires all 16 integer registers at each kick and the final continuation state to match, with exactly the same packet/activation coverage as the full-memory check. Empty polygons from partial clipping currently decline integer prediction and fail this gate rather than silently reducing coverage.

Lit-plan also requires scalar Q/P (including pending values and deadlines), I, clipping history and random state to agree at every packet and final continuation, with matching coverage and zero declines. Exercise both default delayed Q and `PS2_VU_QLAT=0` when changing this path.

Lit-plan now requires full vector-register and accumulator agreement at every packet and final continuation. The vector report must cover every memory-verified packet/activation and report zero mismatches/skips; ordinary-only coverage is insufficient.

Lit-plan additionally requires exact executed instruction-pair counts for every activation covered by the memory/register checks. These are runtime instruction-guard counts, not host frame-time measurements.

Lit-plan additionally requires MAC/status, pipeline-slot contents/validity and final slot index to match for every activation. Unsaturated inherited clipping flags explicitly decline and fail acceptance; do not reduce coverage to hide this case.


Use `tools/test_ac4_model_gpu.py <capture> --native-lit --materials` for native-lit replay acceptance; add `--lit-clipping` on a capture with positive clipped-lit coverage. This requires full shadow memory/register/count/flag evidence, identical native activation/packet/instruction totals, and matching final images. `-NativeLit` launcher/capture exclusion and atomic preflight rejection are covered by automated tests. Final-image equality does not establish every-frame or full-game acceptance.


Add `--second-pass` to model GPU replay tests when validating secondary material geometry. It requires positive secondary draws and equal coverage across every enabled mesh mode, alongside final-image parity. Zero secondary draws is a coverage failure, not a pass.

`--terrain-prep` on a visible flight model replay additionally compares decoded terrain records and distance morphing against original VU memory. It requires both mode-2 morphed and mode-4 unmorphed patches, complete activation coverage, and zero mismatches/declines. It does not claim native terrain rendering or complete terrain VU state.

Terrain preparation acceptance now includes complete ordinary two-stream packets and independent output-bank selection. It requires equal packet/preparation coverage and zero packet/bank mismatches or declines; mode-1 clipped fans are not yet implemented by this reference builder.

Native terrain GPU acceptance uses `python tools/test_ac4_model_gpu.py <private-visible-flight-capture> --fields 8 --terrain-mesh --materials --native-lit --second-pass`. It implies terrain preparation/packet/topology checks, requires positive and consistent primary/secondary native terrain draw coverage, and keeps strict final-image byte equality. It does not prove every frame, clipped terrain or terrain VU/GIF bypass. The synthetic renderer suite also compiles the actual shader fog-normalization helper into a C harness and compares all 256 input bytes with an independent division oracle.

Use `--terrain-only` for DMA scenes without model geometry; it implies `--terrain-mesh` and retains positive terrain draw and full packet/topology gates. GS-only or invalid capture headers are rejected before replay. A final-image match with unsupported clipped packets still fails coverage acceptance. The 120-field private island-turn capture is the mode-1 clipping/culling fixture; its packet, topology and GPU coverage gates now pass. Full VU continuation-state validation remains pending.


Terrain replay acceptance also requires independent instruction-pair counts for every prepared activation, including partially clipped fans: built/ended totals must equal preparation coverage, predicted and observed instruction sums must match and be positive, and mismatches/exclusions must be zero. This is guest instruction accounting, not a frame-time or FPS test. Full vector/ACC and arithmetic flag-pipeline state remain prerequisites for terrain execution bypass.


Terrain vector/ACC acceptance requires complete exit and packet-submission coverage, matching the validated integer-state totals, including partially clipped fans. Any missing report, mismatch or exclusion fails. End validation includes packet-callback count consistency. Arithmetic flag-pipeline validation remains required before terrain VU bypass; successful vector checks alone do not authorize it.


Terrain arithmetic-flag acceptance requires exact MAC/status, all pipeline payloads/validity bits and the final slot index for every prepared activation, including clipped fans. Any exclusion, mismatch or incomplete report fails. Synthetic scheduling checks include unset inherited sticky flags, entry-trampoline pipeline rotation, clipped schedule totals and empty-polygon drain state. Component observer success still requires separate actual native-bypass and full-continuation validation before replacing terrain VU execution.


Terrain acceptance now also requires the atomic full continuation report: all component gates must pass, assembled-state activation counts must match preparation, packet totals must match integer/scalar/vector evidence, and instruction totals must match the independent count observer. Full-state mismatches or declines fail. This validates the predicted final state against original execution; native terrain execution/bypass requires separate acceptance.


Full terrain continuation acceptance also requires ordered submission coverage: every expected packet must match its bank address and complete intermediate VU memory, with zero sequence or memory mismatches. Sparse commit tests check unchanged-qword elision, capacity limits and atomic overflow rejection. Predicted submission acceptance is separate from actual native terrain bypass acceptance.


`--native-terrain` adds actual experimental terrain VU execution to replay comparisons and implies terrain preparation/mesh checks. It requires exact native activation, packet and original-instruction totals against full observer continuation evidence, plus existing image/draw checks. The runtime switch is `PS2_AC4_TERRAIN_NATIVE=1`; normal launchers clear it. Native terrain still uses original GIF/GS submission. Do not infer performance or full-game correctness from final-image parity.


Combining `--native-terrain --native-lit` adds a simultaneous native execution mode and requires both program families' actual totals to match their original observer evidence. The GIF synthetic executable compares specialized packed mesh payloads of 3/4/9/10 vertices against generic register writes and complete saved GIF/Q state, including EOP and wrapping. Tag/setup processing remains generic.


Fast/native mesh replay modes now require positive whole-packet GIF bypass coverage with equal packet/qword totals across those modes. Audit and plan modes retain generic decoding. The GIF synthetic executable additionally compares complete wrapped A+D-plus-two-geometry-stream packets, PRE and tag counters, and verifies that unsupported later tags reject before any setup write. The specialized path still drives GS register interfaces; it does not bypass GS rendering or unsupported layouts.


The renderer suite also checks the preparatory AC4 sky-band encoder: adjacent
visibility, closing segment, four-corner clipping, packet colour/depth order,
maximum capacity and atomic rejection. These synthetic checks do not constitute
live sky replacement acceptance; the encoder is not enabled in gameplay yet.


For developer live sky evidence, set `PS2_AC4_SKY_AUDIT=1` and optionally
`PS2_AC4_SKY_SNAPSHOT` to a path under ignored `work/`, then run a route into
flight and exit normally. Validate its log and fixture with
`python tools/check_ac4_sky_audit.py <log> <snapshot>`.
A run that only reaches the hangar or ends without the final audit report is
not a pass. This observer does not enable a native sky replacement.

Use `--dome-plan` with the live sky evidence checker to require complete dome
packet comparisons in addition to band encoding. This gate covers dome packet
bytes and lengths; it does not certify full haze plans, guest continuation or
native GPU sky replacement.

### AC4 native screen composition

For a complete private DMA capture containing flight HUD draws, use:

```text
python tools/test_ac4_screen_gpu.py <capture> --precompose
```

This requires identical 1x GS/in-place/pre-display images, complete native group
ownership, actual lifted draws, and a copied/presented native display target.
It does not certify every HUD/radar/radio producer or an entire mission.

Use `--display-fallback` instead of `--precompose` to force the extra display
target unavailable. This still requires exact original-target pixels and
positive pre-display composition, but requires that no native display target
was copied/presented. It verifies the fallback rather than counting fallback
as successful high-resolution coverage.

Check independent scene/display resolutions with:

```text
python tools/test_ac4_screen_resolution.py <capture> --output-root <private-evidence-directory> --width 1920 --height 1080
python tools/test_ac4_screen_resolution.py <capture> --output-root <private-evidence-directory> --width 3840 --height 2160
```

These hidden Vulkan replays keep the scene at 1x and exercise automatic overlay
scaling. They require nonuniform complete images, positive native composition
and presentation, the expected output dimensions and a sufficiently large
native target. **They are resolution smoke tests, not image-parity or audio
tests.** Inspect the images and test live transitions separately. Keep captures,
screenshots and generated evidence out of source control. Large outputs should
go to a drive with sufficient free space.

### Reproducible model/terrain GPU replays

`tools/test_ac4_model_gpu.py` accepts `--output-root` to place private artifacts
on another disk and `--scale 1` (or `--scale 5`) to select the internal scale.
Replays use hidden windows and lockstep submission. The requested field must
exist in a complete DMA capture; asking for more fields than the capture holds
now fails before execution. `--native-terrain --materials` also checks their
combined mode, including positive native material and native execution coverage.
For aircraft plus terrain, add `--native-lit --second-pass`; for a terrain-only
capture, use `--terrain-only`. Exact comparisons cover the final captured image,
not every intermediate frame or live gameplay.

The native material shader now accepts validated AC4 packed mip atlases while
preserving the game's supplied levels and LOD calculation. This compatibility
path is separate from hardware mip-chain conversion. The developer-only
`PS2_AC4_ATLAS_TEXTURE=1` conversion remains visually unaccepted and must not be
used to claim texture/filtering parity.


For developer-only supplied-level checks, enable `PS2_AC4_SUPPLIED_MIPS=1` with
native material and texture switches in a private replay. Require positive
`ac4 supplied mip chains` counts, zero allocation fallback, and image comparison
against the packed-atlas reference. This preserves AC4 mip artwork in actual
Vulkan image levels but intentionally keeps the original LOD/interpolation as a
reference; do not report anisotropic filtering from this counter alone.


### Supplied mip filtering comparison

After building the native replay binary/shaders:

```powershell
python tools/test_ac4_texture_filter.py "private-terrain.gscap" --fields 8 --scale 5 --output-root "C:/private-renderer-evidence"
```

This compares GS, preserved native levels with exact sampling, hardware trilinear,
and anisotropic sampling. The exact reference must be byte-identical to GS;
filtered modes require equal positive supplied-level coverage, no allocation
fallbacks, and matching hardware-filter counters. Anisotropic mode also requires
an enabled anisotropic Vulkan sampler. Filtered images are retained for visual
and temporal review; a successful evidence check is **not** automatic visual
acceptance. Use terrain-only fixtures: ordinary aircraft native mip generation
can intentionally differ and must be evaluated separately.

Developer modes use `PS2_AC4_SUPPLIED_MIPS=1` and
`PS2_AC4_SUPPLIED_FILTER=0` (exact), `1` (hardware trilinear), or `2`
(anisotropic gradients), alongside native material/texture flags. Only linear
magnification plus trilinear minification is eligible for the hardware modes;
other supplied states retain exact sampling. Normal launchers sanitize these
experimental flags. Do not enable them as release defaults before broader review.


The texture-filter checker accepts `--anisotropy 2|4|8|16` to compare a chosen
sampler setting. Baseline and exact native modes use the same setting; compare
within that run, not against a hash captured at another setting. The renderer
suite now also executes the actual sampler update functions with mocked Vulkan
allocation/fence failures and descriptor binding checks, including the native HUD
targets. Interactive settings changes still require a live-game acceptance check.


### Sampler transition regression

`python tools/test_ac4_sampler_cycle.py "private-screen-capture.gscap" --height 2160 --output-root "C:/private-evidence"`
runs 16x/off/4x/16x in one Vulkan session, then compares each screenshot to a fresh
process at that setting. Use `--height 1080` for the lower-resolution display case.
It requires native mip and high-resolution display coverage. Replay's
`--aniso-cycle` requires `--hidden --loop 4`; it uses the renderer's real sampler
update implementation and does not save settings.

Snapshot restore clears both overlay lists and the cross-field hazard history.
Ordinary gameplay retains that history. The strict byte comparison passes on the
private two-field fixture at 1080p and 4K after fixing this replay-state regression; it is
not a mouse/keyboard UI interaction test or a full-game temporal validation.


### Combined hybrid live route

`python tools/test_ac4_hybrid_route.py --pad <private.pad> --saves <private-save-directory> --output-root <private-evidence-directory> --fields 10400`
uses the actual `-HybridRenderer` launcher preset with isolated copied saves and
settings. Supply a recording that reaches flight and produces HUD/radio output.
The default opening is `Show`, matching play; `--opening GameDefault` is an
explicit route override, not full default-launch acceptance. Screenshots and
configuration stay in the private output directory. A normal process exit alone
is insufficient: the checker requires positive native model/terrain/sky/cloud,
mip filtering, screen-group and high-resolution display coverage. It records
texture fallback counts and interval rates, which still need diagnosis/review;
passing does not establish audio synchronization, visual correctness or locked FPS.


Experimental radar masking: run python tools/test_ac4_screen_mask.py CAPTURE --fields N --native-geometry --output-root PRIVATE_DIRECTORY. This requires a complete private capture and positive mask/native terrain coverage, comparing original-resolution output exactly against GS. Use --scale 3 only for higher-resolution visual review. This does not replace live camera/transition/resize testing.
# Cockpit activation state gate

Add `--native` to the cockpit acceptance command to compare actual native
activation execution and native-material execution with the original capture.
Use `--state-plan --secondary --clipping --native` for the complete six-mode
comparison. Runtime activation is opt-in with `PS2_AC4_COCKPIT_NATIVE=1` and is
enabled by the opt-in hybrid preset. Ordinary play retains its existing defaults.
The gate requires native activation and
packet totals to equal original coverage, positive replaced instruction counts,
and exact images. Add `--gif` for the eight-mode comparison with guarded GIF
bypass and cockpit-specific packet/qword coverage. Live coverage remains a
separate requirement; unsupported packet formats retain generic parsing.

Add `--state-plan` to `tools/test_ac4_cockpit_gpu.py --secondary --clipping`
to require complete memory, integer, scalar, vector, accumulator, arithmetic flags and instruction-count coverage alongside the rendered
image comparison. The gate rejects missing/duplicate reports, skipped
activations, packet-count differences, mismatches, and declines. It does not
prove whole-program VU bypass or whole-game coverage.

`tools/run_renderer_tests.py --work-dir E:\ac4-renderer-evidence` can place
temporary test binaries on another volume when the checkout drive is full.



Fixed HUD producer census (private live run): set `PS2_AC4_HUD_GROUP_AUDIT=1`
in the test launcher environment. This observer checks PTS, TIME, weapons, DMG
TGT, speed/altitude, their brackets and alternate-view producers without moving
draws or changing guest memory. Validate the
completed runtime log with:

```powershell
python tools/analyze_ac4_hud_groups.py path/to/run.log --output path/to/hud-groups.json
```

The checker requires positive coverage for every producer, balanced hook pairs,
consistent totals, and zero declined allocation ranges. It does not establish
visual correctness, whole-game coverage, or centered HUD acceptance by itself.
`tools/run_renderer_tests.py` covers packet preflight, malformed/oversized lists,
atomic rejection, observer immutability, hook rollback and nested callbacks.

Experimental producer ownership is enabled separately by
`PS2_AC4_HUD_GROUP_NATIVE=1`; it is not in the public or hybrid launcher preset.
For those private runs pass `--native` to `analyze_ac4_hud_groups.py` to require
ownership for every matched producer, in addition to all twelve census reports.
The centered layout still needs visual acceptance and remaining HUD coverage.

Use `--views` alongside `--native` to require positive alternate-view producer
coverage. Without `--views`, the two alternate-view counters may be zero, but
reports must still be present and balanced, and the other ten producers must
be exercised. Pixel comparisons and visual inspection remain separate gates.

The alternate map candidate currently remains observation-only. It appears under
`ac4 HUD candidate`, and do not count toward native group ownership. For private
analysis only, `PS2_AC4_HUD_CANDIDATE_DUMP=<existing-directory>` saves one bounded
allocation per candidate. These contain game data: keep them outside the source
package and do not commit or publish them.


### Native window-size transitions

With a native build and a private DMA capture containing visible native HUD,
run `python tools/test_ac4_window_cycle.py <capture> --fields 4 --output-root <private-evidence-folder>`.
It resizes a hidden SDL/Vulkan window through 720p, 1080p, 4K and back to 720p,
checks drawable sizes, and requires each image to equal a fresh-process control.
The renderer suite covers failure handling; the public suite rejects incomplete
or inconsistent resize reports. Interactive dragging, Alt-Tab and F4 settings
remain separate acceptance scenarios. Captures and output images stay private.

The hybrid route checker now also requires fixed HUD/radar ownership; use
`--cockpit-views --fields 13400` with the developer Mission 1 route to require
alternate-view speed/altitude and cockpit coverage. A route report is coverage
evidence, not proof of perceptual audio synchronization or stable frame times.


For fixed HUD/radar placement use `python tools/test_ac4_hud_gpu.py <capture>
--settings <private-run.ini> --output-root <private-evidence-folder> --scale 1`
(on one command line), then repeat with `--scale 3`. The capture must contain
native group records from a hybrid run. GS, stretched in-place and stretched
overlay output must match exactly; centered output must exercise reanchoring.
Inspect the centered image for clipping, radar masks and bracket alignment.
A changed centered image alone is not visual acceptance.


### HUD temporal diagnostics

`gsreplay <private-capture> --hidden --loop 1 --fields N --field-shots <prefix>`
writes one PPM per rendered field (1..120 fields), in addition to the final shot.
`--skip-field K` suppresses one presentation while still executing its draws;
K must precede the final field. These hidden-only diagnostics are not playback
modes or performance benchmarks. Compare matched capture fields at equal scale
and HUD layout; different centered/stretched layouts are not a parity test.

Full output coverage is the HybridRenderer default; FullOutputResolution remains a compatible explicit alias. Synthetic checks
cover width/height ratios, placeholder sizes, overflow and the scale limit.
Compare Auto with an equivalent fixed-scale control and inspect the actual scene
scale as well as overlay dimensions; a 4K window alone is insufficient evidence.


Ordered-field regressions: the native renderer suite exercises delayed consumers
(including delays beyond 100 ms), spurious wakeups, cancellation and shutdown
without replacing pending work. It also covers per-image presentation semaphore
ownership, SUBOPTIMAL acquisitions and acquire-barrier stages. For live checks,
use HybridRenderer at 3840x2160 with Internal Resolution Auto, cycle all flight
views, toggle radar/map, pause/resume and review the post-mission replay. Require
zero `vk: ... fields dropped` messages; guest-clock missed ticks and presentation
frame intervals are separate metrics and can still spike. Diagnostic screenshot
or capture runs are not clean frame-time benchmarks.


Loading image reserve regressions are included in tools/run_renderer_tests.py.
They exercise exact Vulkan image requirements/usage flags, shared-heap checks,
budget overflow guards, low/unsupported-budget fallback, runtime pressure stops,
opt-out and release of unused images. At full 4K Auto, inspect the startup
loading-reserve and prewarmed-image log entries. Budget-dependent counts are
expected; never require a high count on a constrained adapter. Compare private
matched routes with PS2_LOADING_PREWARM=0 and =1 injected after launcher
environment isolation. Keep native/full output coverage, audio mixing, scene
route and capture settings identical. Record the source IDs, allocation bytes/
time, worst frame intervals and consumed reservation count. Startup allocation
time must be reported separately: this prepares resources before gameplay and
does not eliminate the allocation cost or prove campaign-wide coverage.


CPU stream-row regressions are part of tools/run_renderer_tests.py. The
715,904 scalar differential cases cover CT32 pair ordering, odd starts and
counts, all swizzled/linear block rows, unaligned input, VRAM-edge clipping,
32-bit address wrapping, aliasing and buffer guards. Also run the native
ac4 executable with --gs-test: it checks whole/partial/split IMAGE streams,
VRAM, cursor, remaining pixels and upload recording against per-qword writes.
PS2_TRX_ROW_SCALAR=1 is a private rollback of the paired CT32 row strategy;
remove the variable to enable the normal path. T8 retains its existing loop.
Do not convert isolated copy benchmarks into whole-game FPS claims.

PS2_GFX_TIMELINE=<private.csv> optionally records graphics-queue field markers
in memory, then writes them after worker shutdown (including bounded-run
finish). It is off by default and must be injected after launcher environment
isolation for private tests. Rows include EE ready/queued times, producer
thread identity/CPU counter, worker ready/CPU counter, accumulated idle waits,
previous GS-present call duration and queue bytes. The cap is 32,768 rows;
overflow is reported and never discards game fields. Windows GetThreadTimes
counters are coarse: inspect aggregate deltas and wall/idle/queue timings,
not purported exact single-frame CPU costs. Only subtract producer CPU counters
when producer_tid matches. Non-Windows CPU/thread-id fields are unavailable
(zero). Join to the renderer timeline using monotonic timestamps, rather than
assuming every GS field necessarily produced a displayed field. For clean
timing runs disable PS2_PROFILE_HOST: its sampler suspends the target thread.
Queue unit tests check traced/untraced field order, copied data, backpressure,
ring wrapping, delayed workers, deferred file IO and bounded overflow.

Graphics command timing (diagnostic only): set PS2_GFX_JOB_TIMING=1 together with PS2_GFX_TIMELINE after launcher environment isolation. This adds per-command wall clocks and writes <timeline>.jobs.csv at shutdown, bounded to the same 32768 fields. Rows contain field, opcode, count, total_ns, max_ns and max_bytes (size of the slowest record). Opcodes 1/2 are VIF/GIF transfers, 3 VIF register writes, 6 GS privileged writes, 8 ordinary presentation, 9 tags, 10 native intents and 11 traced presentation. A field includes commands before its marker and the presentation at that marker. Wall time includes descheduling/waits; it is not CPU execution time. Use alongside field CPU/idle counters. Disable for final performance acceptance because packet clocks add overhead. Queue tests verify attribution, delayed records and overflow.

Bounded model transform checks (2026-10-18): ac4_model_test now compares 200000 transform and flag-state traces against the scalar PS2 math oracle, including conservative-range boundaries, cancellation, signed zeros, arbitrary bit patterns, infinities, NaNs and denormals. The ordinary-value path keeps all four flag writes and separate multiply/add rounding. Nonzero inputs outside [2^-30,2^30] use the existing clamped path. Private PS2_AC4_TRANSFORM_CONSERVATIVE=1, injected after launcher environment isolation, disables this path for matched comparisons; this is not a quality setting.

Field traces now append worker_cycles, worker_lock_ns and worker_apply_ns. Cycles are raw Windows per-thread CPU cycle counts, not nanoseconds. Lock/apply values are cumulative wall clocks: lock is the queue mutex acquisition interval (which may include descheduling), apply is queue command batches including previous presentation and trace bookkeeping. Interval wall minus apply/lock/idle estimates remaining queue bookkeeping/scheduling time. None of these counters alone proves a specific OS scheduling cause or turns wall time into precise CPU milliseconds. They are enabled only with PS2_GFX_TIMELINE; packet clocks stay separately opt-in.

Field wakeup regression (2026-10-18): the real async queue test now counts condition notifications, inserts 600 VIF words before each of its first 70 fields, and asserts one field-completion notification per completed field despite multiple ring-progress batches. A separate 512 KiB transfer case with a 1 MiB ring and eight pending fields forces actual full-ring producer waits and checks payload/order preservation. The private PS2_GFX_FIELD_WAKE_LEGACY=1 comparison restores shared-condition wakeups after launcher isolation. Field tracing appends worker_notify_ns to isolate cumulative ring/field notification wall time from remaining queue bookkeeping. The notification interval can itself include descheduling; it is not a CPU-only measurement.


GS worker regression coverage (2026-10-19): tools/run_renderer_tests.py now runs
35 native executables. The new mip row-copy check compares 20,000 wrapped/cropped,
strided and unaligned cases against the original per-pixel mapping, including
padding guards. Texture-retirement checks additionally compare 100 age/slot victim
orders with the previous repeated oldest scan (ties, live epochs and counter wrap),
and validate full resident-cache identity and ownership renewal. Existing failed
GPU wait, descriptor fallback and owned-mip budget checks remain required.
Performance acceptance uses identical captured packets with diagnostics disabled;
GPU parity and sampling are separate runs. Live routes alone are not matched-workload
comparisons when their mission timers/progress diverge.

Performance claims must distinguish first-pass/fresh-process application-cache results from warm repeated passes. Compare old/new on the same captured workload with empty prefixes and identical warm seeds; keep raw outliers and verify workload/image parity. Do not clear user caches or label an application-cache reset as a driver/OS-cache reset. Report startup separately from gameplay and never infer locked 60 from replay averages.

The renderer runner now builds 36 test executables. PSMT8 byte-offset decoding adds 622,592 comparisons against the original bit-offset pixel/hash loop across GS and linear layouts, row subranges, scalar tails, varied palettes, source alignment and destination guards.

The final renderer runner builds 37 test executables. Startup pipeline preparation additionally checks unique shipped states, lossless key decoding, the Mission 2 transition state, budgeted continuation, cleanup and empty re-entry. First-run acceptance uses a fresh application-cache prefix and checks compile phase, not just average FPS. The validated missing state compiled at startup instead of during mission entry; this shifts compilation cost before gameplay rather than removing it.
