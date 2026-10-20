# V3 hybrid renderer

Consolidated performance history and cache limits: [PERFORMANCE.md](PERFORMANCE.md).

## Mission 2 repeated stability checks (2026-10-19)

Four visible native-Vulkan/full-4K route runs retained all 68,400 requested source
fields in order. Three AOT runs cover two empty application caches and one warm
cache; a separate empty-cache run checks the default native CPU replacements.
No crashes, missing/duplicate fields, skipped submissions, gameplay pipeline
builds, trace overflows, interpreted VU or native arena refusals were observed.
Save/build hashes are unchanged. The first run extends flight by 3,600 fields.

Common flight window 12900..16199 (3,300 intervals per run):

| Run | CPU mode | p99 ms | Worst ms | >33.37 ms | Whole-route worst ms |
|---|---|---:|---:|---:|---:|
| cold-extended | AOT VU | 29.85 | 37.17 | 6 | 40.60 |
| warm-repeat | AOT VU | 30.08 | 36.99 | 3 | 46.88 |
| cold-repeat | AOT VU | 29.25 | 36.75 | 5 | 46.25 |
| native-default | Native CPU replacements | 30.09 | 39.40 | 1 | 46.40 |

The repeated AOT samples are rougher than the earlier best single run. Locked60
remains OPEN. Submission timing is not monitor scanout; live game progress/load
can differ. Default-native mode has only one new sample; this is not a causal
CPU-mode speedup comparison. Runtime/source build was unchanged throughout.

Muted tests keep the mixer active and skip the opening. Other missions, complete
tours, view/focus/pause transitions, save-write/load-roundtrips and audible audio
remain unaccepted. Existing Mission2 HUD issues remain open. Driver/OS caches
were not purged; user app-cache paths and unrelated processes were untouched.
The new cold loading fix held across all four starts: every loaded state was
prepared before gameplay. First-use compilation still costs time at startup.
No runtime optimization was added by this test batch. Keep the repeated baseline
and use matched captures before claiming any future candidate speedup.

Evidence and per-spike attribution:
E:/ac4-renderer-evidence/mission2-20261019/stability-combined.json
## Indexed decoding and cold Mission 2 loading (2026-10-19)

PSMT8 decoding now uses byte offsets derived once during GS table initialization,
retaining original pixels, palettes and texture hashes. Empty-cache replay median
was 21.570 -> 21.285 ms/field (1.32% less, a small observed result under external
load). All final replay images and work counts match. This adds 256 bytes of static
math tables, with no output-quality or persisted-cache-capacity changes.

A visible cold-cache live test exposed a separate 299.902 ms gameplay compile for
pipeline key 000302000064011F at mission-entry source field 12041. Its render state
is now included in shipped startup preparation (96 states). A second empty-cache
run compiled it in 312.974 ms BEFORE guest execution; no gameplay pipeline builds
followed. At that same source field, recording fell from 306.17 to 6.32 ms and the
source interval from 336.55 to 36.84 ms. This moves the compile cost before gameplay;
it does not eliminate compilation or depend on a saved application cache.

Final visible/native/full-4K route retained all 16,200 source fields. Flight p99 was
25.89 ms, worst 30.24 ms; no flight intervals over 33.37 ms. Overall worst was
36.84 ms. Locked60 remains OPEN: 756/3,300 flight intervals exceeded 20 ms, with
worker apply 29.91 ms at the worst flight interval. No interpreted VU executions,
native arena refusals or original-save changes. Other Mission2 UI issues remain.
Live positions/load may differ, so these percentiles are not a controlled CPU
speedup. The short hidden/visible replay did not reproduce the older acquire stall.

152 public tests and 37 native renderer test executables passed. New coverage:
622,592 decode pixel/hash/bounds cases and budgeted startup-state preparation.
Private evidence: `E:/ac4-renderer-evidence/mission2-20261019/INDEXED-DECODE.txt`.

## Mission 2 cache isolation (2026-10-19)

The retained worker fix was compared with its original control binary in fresh
processes, with empty application caches, identical warm seeds, and pipeline
caching disabled. Two repetitions reversed arm order; each process replayed the
same 90 fields three times. Median old/new pass times were 24.635/21.845 ms empty
(11.3% less), 24.395/21.705 ms warm (11.0%), and 24.415/21.595 ms disabled (11.6%).
Cold first-pass values were 24.84/24.43 ms control versus 22.39/22.05 ms fixed.
All 12 final full-resolution images match byte for byte; field/work counts match.
Native Vulkan/full 4K stayed enabled. Runtime caches remain enabled in both builds;
the fix includes legitimate resident-atlas reuse. Driver and OS caches were not
purged, and later passes retain in-process state. This isolates application-cache
warmup, not all caching or fresh-machine performance. Cache-off also disables
startup pipeline preparation. A warm-run outlier is retained in the raw results.
These are replay-throughput figures, not live FPS or a locked-60 acceptance.
Private evidence: `E:/ac4-renderer-evidence/mission2-20261019/CACHE-ISOLATION.txt`.

The separate T8 block-copy experiment passed 715,904 oracle cases and image
checks but was removed: cold replay median was 22.45 versus 21.61 ms control.
The validated worker source and both executables were restored byte for byte.
No new runtime optimization was retained by this cache-isolation session.
The fresh sampler still identifies indexed texture decode/transfers as CPU targets.

## Mission 2 captured-worker optimization (2026-10-19)

A complete 90-field Mission 2 DMA capture now permits identical-workload comparisons.
Replay sampling identified GS texture preparation/store management as the largest
internal hot group. The live capture sampler did not emit a report; findings use
the separate replay sampler, not assumed live samples.

Texture collection now orders eligible images once per batch, preserving the exact
previous age/slot victim order and GPU/queued-frame lifetime checks. Authored mip
atlases copy contiguous wrapped row runs, and a full resident-cache lookup avoids
rebuilding an atlas after direct-cache collisions. Native Vulkan/full output
resolution, texture contents, supplied mips and all frame-order behavior remain.
The experimental cursor replacement policy was removed.

Alternating old/new binaries on the identical 90 fields (six pass averages each)
reduced median pass time from 21.455 to 18.795 ms/field (12.4%). All primitive,
register-write and transfer counts match. This is replay throughput, not live FPS
or per-frame p99. Twelve consecutive full-4K outputs and the final field after all
90 fields match byte for byte. 152 public tests and 35 renderer test executables
pass, including 20,000 row-copy oracle cases and 100 exact retirement-order cases.

Live follow-up retained all 16,200 fields and left the original save unchanged.
The 3,300-interval initial-flight window had p99 34.02 ms (prior 47.78), 39
intervals over 33.37 ms (prior 282), and five over 50 ms (prior 18). Worst was
67.37 ms, worse than the prior 61.04 ms: locked 60 is not achieved. Live routes
progressed differently, so these observations are not a controlled speedup.
The worst interval spent 64.64 ms acquiring a swapchain image, with no allocation;
this locates the wait but does not establish its cause. Worker apply bursts also
remain (79.23 ms at field 13158). Investigate acquire/presentation scheduling and
remaining GS preparation/transfer bursts next. The muted run is not audio acceptance.

Private evidence and limitations:
`E:/ac4-renderer-evidence/mission2-20261019/WORKER-FIX.txt`.
Locked 60 FPS remains open; 18.795 ms still exceeds the 16.67 ms budget. Broader
visual/audio/save acceptance and the existing Mission 2 UI flicker remain open.


## Mission 2 save and performance baseline (2026-10-19)

The user's existing Mission 02 "Imminent Threat" save on Normal loads successfully
from a private memory-card copy. The route was visually verified through F-4E/UGBM
selection into flight and retained as an input recording. The original card's hash
is unchanged. This validates loading this one existing save, not new-save creation,
other slots, full save/load roundtrips or failure recovery.

Screenshot-free AOT test: native Vulkan hybrid, full output resolution, 60 Hz cap,
audio muted with mixing active, memory disc preload, 16,200 source fields retained.
The initial-flight source window 12,900..16,199 (3,300 intervals, about 55 seconds)
had p99 47.78 ms and a worst interval of 61.04 ms, including 18 intervals over 50 ms.
Its mean was still 16.68 ms: average rate does not establish smooth frame delivery.
All 13,179,590 VU completions used AOT, with zero interpreter or unknown-JR fallback.

At the worst flight interval, graphics-worker apply took 57.80 ms with no idle time,
about 190.86 million worker cycles and 4.82 MB queued. EE producer gap was 16.77 ms,
renderer host work 3.48 ms, and no allocation occurred. Flight GPU timestamp windows
averaged about 4.6-5.1 ms with peaks around 7.3 ms; these exclude earlier queue waits.
The next target is CPU work inside worker apply, not missing compiled VU coverage.

A native/HLE follow-up also stalled, but ended at a different mission timer and
position despite identical recorded inputs. It is not a phase-aligned AOT/HLE speed
comparison or an image-parity test. Do not choose a winning policy from it. Capture
and replay identical Mission 2 DMA work next, separately from clean timing runs,
to profile compiled VU/flags, VIF unpack, GS/GIF parsing and texture preparation.

Private route, logs, summaries and protocol:
`E:/ac4-renderer-evidence/mission2-20261019/RESULTS.txt`.
No runtime changes were made in this investigation. Locked 60 FPS, the reported
below-50-FPS sections, Mission 2 UI flicker and Mission 1 flight stalls remain open.


## Manual performance follow-up (2026-10-19)

The user reports that the latest tested build feels faster, but still does not
hold a locked 60 FPS. Mission 2 dropped below 50 FPS during manual play. Treat
this as an open gameplay performance failure; the narrower automated AOT route
does not establish whole-mission performance. The report follows the AOT test
recommendation, but the exact launch flags, location, camera and duration of the
slowdown have not yet been captured. Do not infer its CPU/GPU/VU cause from FPS
alone, or claim the Mission 2 VU programs have zero interpreter fallbacks.

Next performance acceptance target: reproduce actual Mission 2 flight with native
Vulkan hybrid rendering and full 4K retained, record frame/graphics-worker timings
and VU fallback counts, then separate EE production gaps, VU/GS worker work and
render/GPU cost before choosing an optimization. Existing Mission 2 UI flicker
and Mission 1 actual-flight stalls also remain open.


## VU execution investigation (2026-10-19)

Native Vulkan hybrid rendering and full output resolution remain enabled. The
normal hybrid route already used native VU replacements plus AOT, with zero
interpreter fallbacks in that measured route. A captured view-switch workload
exposed a missing cockpit program hidden by native replacement coverage: a generic
initialization signature matched, but its rendering entries were not compiled.
The local private corpus now includes that program (nine generated programs,
none skipped). Twelve consecutive full-4K output frames match the previous control
byte for byte; all 27,116 activations use AOT with zero interpretation or JR bails.
This is captured-scene coverage, not whole-game completeness.

`python -m vurecomp --programs <earlier-census> --extra-programs <new-census> -o <include>`
merges scene coverage without losing earlier entry points; repeat `--extra-programs`
for more scenes. Save the earlier census before capture replaces its manifest.
Image identity conflicts are rejected. Keep microcode and generated game-derived
includes private/ignored. Deferred runtime fallback reports now identify full image,
entry, reason and count, with bounded memory and no activation-time file I/O.

Alternating fixed-workload tests (60 fields, three passes per arm, two arms each)
recorded median pass averages of 29.00 ms/field for the native CPU replacements and
19.21 ms/field for AOT with the same GPU meshes/materials/HUD/full resolution.
These are replay timings, not live FPS or per-frame percentile measurements.
The existing HLE preset remains the default pending broader scene acceptance.

Explicit FMA contraction disabling protects separate VU math operations. Scoped
round-toward-zero/FTZ probes did not establish bit-exact PS2 arithmetic: ordinary
finite ADD/SUB and multiply-accumulate counterexamples remain. MADD/MSUB accuracy
is still open; no blanket RTZ mode is enabled. The register-resident flag candidate
is private opt-in (`PS2_AC4_FLAGS_REGISTER_PATH=1`) because local helper gains did
not reproduce as a reliable whole-route improvement. The isolated small JIT and
alternate GS compatibility experiments are not installed in the game path.

Validation: 152 public tests, 34 native renderer executables and generated-C VU
entry/pair/E-bit/non-mutating-fallback checks pass. Model tests exercise both flag
paths with pending state and separate multiply/add cancellation. Private research
and measurements: `E:/ac4-renderer-evidence/research-20261019/RESULTS.txt`.
Manual Mission 2 UI flicker and Mission 1 actual-flight stalls remain open.

AOT live route: 10,800 fields retained; 14,780,464 compiled VU completions,
zero interpreter/JR fallbacks; p99 24.40 ms, maximum 34.36 ms, one interval above
33.37 ms and none above 50 ms. Briefing window maximum 26.20 ms; flight/test
window maximum 21.33 ms. These are runtime handoff intervals, not display flips
or whole-mission acceptance. Full-state replay verification: 27,116 activations,
zero mismatches against the runtime interpreter (not physical hardware).

Manual opt-in: `play.cmd -HybridRenderer -VuExecution Aot`.
Control: `play.cmd -HybridRenderer -VuExecution Native` (existing default).
Both retain native GPU rendering and full output resolution. The experimental
policy disables only four CPU VU replacements; other HLE/EE services are retained.
Unknown programs can still fall back to interpretation and are reported at exit.


> Evidence correction (2026-10-15): earlier entries called the two-field `terrain_ground_saved.gscap` replay a flight/terrain acceptance case. Its final image is black in every mode, so those exact hashes establish no visible rendering acceptance. At four fields the capture shows a carrier deck, not flight. Historical counts below remain operation counts only. The replacement visible-flight acceptance uses `third_person_controlled.gscap`, eight fields: 1,100 model draws and identical GS/audit/fast/native-material images. GPU acceptance now rejects uniform final frames.


Status: experimental integration, separate `v3-native-mesh` branch. The published native baseline remains the default. Performance tuning is deferred unless a new issue is reported.

## Pending manual-play reports (2026-10-18)

- **Mission 2 UI flickering:** user observed multiple UI flickering issues during the latest manual playtest. Affected elements and triggers have not yet been isolated. Open; investigation deferred at the user's request.
- **Mission 1 flight performance:** user observed large frame drops during actual in-mission flight, making flight noticeably fail to hold a solid 60 FPS. This is a gameplay issue, not limited to briefing/loading. Open; investigation deferred at the user's request.
- These manual findings take precedence over the narrower automated route's performance/visual acceptance. Do not mark UI flicker or sustained flight performance resolved based on those tests. No code changes or new test runs requested for these reports.

## Current capability summary

The opt-in `play.cmd -HybridRenderer` preset now combines validated native
lit/unlit/cockpit/terrain execution, guarded whole-mesh GIF bypass, native GPU
materials, mipmapped/anisotropic textures, sky/cloud geometry and native-resolution
HUD/radar/radio composition. Unsupported states retain GS rendering. Mission 1
flight/cockpit/third-person coverage and hidden 720p/1080p/4K resize comparisons
pass. This does not establish full campaign or interactive display-transition
acceptance. See [AC5-PARITY.md](AC5-PARITY.md) for current evidence and gaps.

The milestones below preserve the development sequence; statements that a
feature was pending describe that milestone, not the current preset.

## Architecture and milestones

1. **Triangle bridge (implemented, opt-in):** use the inherited indexed Vulkan mesh pipeline for compatible GS triangles. Preserve screen positions, depth, texture coordinates/Q, fog, colors, and GS fragment shading. Keep sprites/lines/points and unsupported triangles on the existing path. Flush queued meshes before fallback draws or presentation. Check vertex, index, and draw-block capacity before accepting the mesh submission.
2. **AC4 model path (opt-in GPU draw replacement implemented; VU/GIF bypass pending):** identify one AC4 VU program and its input layout from local captures. Add a native model adapter with explicit program recognition and supported-state checks. Submit model-space vertices and transform constants directly. Keep reference VU/GIF execution for verification until a separate bypass is validated. Compare output against the baseline before widening coverage.
3. **Expand coverage (pending):** aircraft and terrain, then HUD/radar/radio captions at window resolution; retain GS fallback for uncovered menus, briefing/hangar content, and effects.
4. **Later port features:** texture replacement for HUD/icons/fonts, menus/briefings/subtitles and terrain; PC options; save/load and Alt-Tab validation.

The triangle bridge is an integration test, **not yet the direct model renderer**. It retains VU transformation and GIF decoding and may add overhead. No performance improvement or full visual parity is claimed. It uses ordinary vertex shaders and indexed draw calls, not the Vulkan mesh-shader extension. AC5 guest addresses and layouts remain disabled for AC4.

## Run

Build normally from your own supported ISO; the existing Optimize workflow supplies native VU programs. From this checkout:

```powershell
.\play.cmd -MeshBridge triangles
```

Baseline comparison (also the default):

```powershell
.\play.cmd -MeshBridge off
```

Python equivalent: `python tools/setup_ac4.py run --mesh-bridge triangles`.
Direct runtime flag: `PS2_AC4_MESH_BRIDGE=triangles`. The public helper intentionally clears inherited developer flags, so use its explicit option. VU capture sessions always disable the bridge.

## Validation

Public unit/launcher acceptance tests: `python tools/run_tests.py --suite all`.

Synthetic production-code renderer tests (from a configured UCRT64 toolchain shell):

```powershell
New-Item -ItemType Directory -Force work | Out-Null
gcc -std=c2x -Wall -Wextra -DPS2_GAME_AC4=1 -Iruntime/include tools/ac4_mesh_bridge_test.c -o work/ac4_mesh_bridge_test.exe -lm
.\work\ac4_mesh_bridge_test.exe
```

These check explicit opt-in, rejection of unsupported/fractional colors, preservation of vertex attributes and indices, batching/state changes, fallback ordering, unavailable meshes, failed storage, full draw arenas, and the batch-size boundary. They use stubbed GPU functions and synthetic data; they do not establish GPU or gameplay correctness.

Manual acceptance before enabling by default: compare deterministic captures and full opening, post-New-Game movies, live briefing plane, mission aircraft/terrain/sun, HUD/effects, pause/resume. Verify audio remains synchronized. Compare frame times separately; a matching screenshot is not performance evidence. Keep captures, generated code, and game microcode local and ignored.

## Local evidence (2026-10-14)

- Windows/UCRT64 GCC 16.2: native executable and `gsreplay` built successfully.
- Public suite: 27 tests passed. Synthetic C bridge checks passed with `-Wall -Wextra`, including batch-limit fallback.
- Radeon RX 7900 XTX: aircraft-selection capture, 8 fields. Bridge submitted 33,283 triangles in 24,028 indexed mesh draws with no arena refusal. The final visible hangar/aircraft/menu image matched the baseline pixel for pixel (3200x2240 internal image, default 4K window). This validates this captured scene only.
- Title-screen capture: final visible image matched exactly; this scene used GS sprite fallback rather than mesh triangles.
- Earlier 60-field VU capture exercised 126,600 bridged triangles, but its screenshot was black in both modes. It is **not** counted as visual evidence.
- Initial one-pass replay timings were worse with the bridge (hangar: baseline 26.75 ms/field, bridge 138.79 ms/field). These were short runs with pipeline warmup differences and concurrent compilation, not controlled gameplay benchmarks. Do not enable this path by default or advertise it as an optimization. The direct model-space adapter is still required to remove VU/GIF work.

Local evidence is under ignored `work/`: `v3-build.log`, `v3-replay-build.log`, `v3-tests.log`, `aircraft-off/on.log` and PPMs, `boot-off/on.log` and PPMs. No game images/captures are part of the source package.
- Full-game smoke: bridge enabled, opening shown, 60 FPS cap, 1,800 vblanks delivered; exited normally at the frame budget (exit 0). 5,554,880 triangles claimed, no mesh-arena refusals. This is boot/stability evidence, not full-campaign or audio acceptance. Log: `work/mesh-boot-smoke/run.log`.


## Native AC4 model calculation and verification

`runtime/src/rn/rn_ac4_model.c` implements projection, perspective UVs, fog and optional lighting from AC4 model inputs. Two full VU image signatures identify the supported unlit and lit variants. Their resume addresses and output packet header sizes differ; these are handled separately. No AC5 guest addresses are enabled.

Input layout, relative to VU TOP (quadwords): projection columns 0x00..0x03; clip columns 0x04..0x07; normal transform 0x08..0x0B; directional lighting 0x0C..0x0F; light colors 0x10..0x13; ambient 0x14; count/flags 0x17; clipping configuration 0x1D; fog parameters 0x1E. Vertex records begin at 0x20 with four quadwords each: signed fixed-point UV/Q (12 fractional bits), integer RGBA, float position, signed fixed-point normal. These are reverse-engineered descriptions, not distributed game assets.

The observer snapshots constants and inputs before execution, then compares the original first-pass outputs at XGKICK. It never modifies VU state, suppresses a kick, or replaces a draw. It supports batches of 3..23 input vertices and both whole-batch and per-triangle strip output. A conservative homogeneous clip-volume check excludes triangles whose vertices could have been replaced by clipping, even if the output is still a triangle. Culled/empty packets, unknown programs/entry points and unsupported layouts remain outside the comparison. ADC/backface bits and the second material pass are **not** verified by this kernel.

Run normally with verification:

```powershell
.\play.cmd -ModelAudit
```

Python: `python tools/setup_ac4.py run --model-audit`. Direct executable/replay: `PS2_AC4_MODEL_AUDIT=1`. Disabled by default and forcibly disabled for VU capture sessions. It adds comparison work and is not a performance setting. The audit summary is printed on normal exit.

Native tests (saved local toolchain, or pass `--cc` with a UCRT64 compiler path):

```powershell
python tools/run_renderer_tests.py
```

This runs four synthetic C test executables. Model coverage includes projection/perspective, lighting and clamping, fog bounds, invalid inputs, unchanged observer memory, strip indexing/output banks, variant-specific headers/resume points, and the regression where a clipped polygon still has three vertices.

Local replay results: hangar/aircraft, 8 fields, 45,252/45,252 compared vertex outputs exact (40,716 lit, 4,536 unlit); flight/terrain, 2 fields, 7,125/7,125 exact (all lit). Zero position/fog/UV/color differences or rejected vertices in the compared subset. Hangar PPM matches the baseline SHA256 `ac60c995db5730d9d0b9306e0066479f55cab8d0b3f3ee6d283f1a4ed2db66e0`. These counts include repeated vertices across strips and frames, not unique models. Logs are ignored local files `work/model-audit.log` and `work/model-terrain.log`.

**Remaining:** expand verified GPU coverage, preserve clipping/culling/material passes and VU continuation state, then bypass the corresponding VU/GIF work. The GPU submission below does not yet remove that work or claim a performance gain.

Final build validation for this step: both `ac4.exe` and `gsreplay.exe` were rebuilt with the current model source. The 1,800-vblank full-game audit smoke exited normally and compared 555,711 lit vertex outputs, all exact, with zero wrong-kick matches and zero rejected compared vertices. Unsupported programs and clipping paths were explicitly excluded; this does not imply full renderer coverage. Final log: `work/model-game-smoke/run.log`. Public suite: 29 tests passed; both synthetic renderer executables passed. The working renderer remains the default and no V3 changes have been pushed.


## Opt-in GPU model-space draws (2026-10-14)

```powershell
.\play.cmd -ModelMesh
```

Python equivalent: `python tools/setup_ac4.py run --model-mesh`. Direct runtime/replay: `PS2_AC4_MODEL_MESH=1`. Off by default; capture sessions disable it. Normal model drawing no longer implies the CPU model audit; add `-ModelAudit` for reference comparisons. `-ModelAudit` alone continues to observe without replacing draws. The separate triangle bridge is not required.

`rn_ac4_mesh.c` submits raw AC4 model coordinates, normals, colors, UVs and transform/light constants to Vulkan's indexed vertex pipeline (`mesh.vert`, program 6). The GPU performs projection, optional lighting, perspective UVs, fog and GS coordinate/depth packing. The existing GS fragment shader retains texture/material behavior. It is an ordinary vertex shader, not a Vulkan mesh-shader extension.

Only a verified, unclipped, smooth-shaded first-pass triangle from a recognized program is eligible. Its GS attributes must also match the predicted first-pass output exactly. Each candidate expires after one draw attempt or the current XGKICK; it cannot accidentally replace a later material pass. Unsupported state, attribute mismatches and full mesh arenas fall back to the existing draw path. Clipping, sprites, other programs and later material passes remain on the compatibility renderer.

The shader refines reciprocal and depth-division residuals with explicit fused multiply-add operations. Initial GPU divisions produced large coplanar aircraft-surface differences despite matching CPU reference vertices; refining both eliminated the observed hangar mismatch. This is validated on the local GPU, not a guarantee of bit-identical division on every driver/device.

Current GPU acceptance:

- Hangar/aircraft selection, 8 fields: 14,444 GPU model-space draws; final 3200x2240 PPM byte-identical to baseline (SHA256 `ac60c995db5730d9d0b9306e0066479f55cab8d0b3f3ee6d283f1a4ed2db66e0`).
- Flight/terrain capture, 2 fields: 2,268 GPU model-space draws; final PPM byte-identical to baseline (SHA256 `c83f95c198d0d33968393c14a196d1e67322c27c32cb3a6fa38840f6e8d9e4dd`). The final frame is black; this is not visible flight or terrain acceptance.
- Public unit/launcher acceptance: 31 passed. Three synthetic native C executables passed, including raw-coordinate upload, one-use matching, material/state rejection and arena fallback.
- Reproduce local capture comparisons with `python tools/test_ac4_model_gpu.py <private-capture> --fields 8`; prerequisites and limitations are in TESTING.md. No capture or game-derived image is distributed.

**Still experimental:** VU execution and GS state updates remain active. CPU comparisons are optional. This stage proves GPU model drawing alongside fallback; it is not a performance optimization yet. Full interactive mission, movies/audio, clipping/material coverage and additional GPUs need validation before enabling by default. No V3 changes have been pushed.


Playable build validation: `ac4.exe` rebuilt successfully. A bounded opening/boot smoke with `-ModelMesh` equivalent settings delivered 1,800 vblanks and exited normally (exit 0). The adapter accepted 179,661 model-space draws; Vulkan reported 177,798 recorded native mesh draws and no arena refusals. The retained CPU audit compared 555,711 vertex outputs, all exact, with zero wrong-kick matches or rejected compared vertices. This is stability/coverage evidence, not visual acceptance of every frame or an audio/performance result. Private log: `work/gpu-model-game-smoke/run.log`.


## Removing duplicate work: first stage

`play.cmd -ModelMesh` now uses the original VU packet outputs for one-use draw matching without recalculating their lighting, UVs and fog on the CPU. Projection safety and clip-volume checks remain. Use `play.cmd -ModelMesh -ModelAudit` to restore the previous reference-comparison path. Both are still opt-in; ordinary play retains the working baseline.

For eligible complete ST/RGBA/XYZF triangles, the non-audited mode bypasses generic GIF payload decoding. It preserves GS register writes, Q latch, ADC, register order and GIF parser continuation state. Tags, material setup and other packet layouts keep the ordinary parser. The synthetic GIF regression compares actual production parser register writes and final state, including a packet wrapping around VU memory and rejection of partial/unsupported payloads.

This is **not full VU/GIF bypass**. The original model VU programs still produce state and later material passes needed by the compatibility renderer. Removing those programs requires implementing and comparing their complete continuation/register/memory effects, culling and secondary passes, rather than simply suppressing XGKICK. No FPS improvement is claimed from the short correctness captures.

Hangar acceptance: baseline/audit/non-audited final PPMs identical, all 14,444 model draws retained. Non-audited mode avoided 45,252 CPU reference vertex evaluations and bypassed generic decoding for 34,299 payload quadwords. The new three-mode acceptance script fails if either image comparison or model draw counts disagree, if the fast path is not exercised, or if CPU reference evaluations remain enabled there.


Final validation of this stage: flight/terrain capture also matched across all three modes, with 2,268 model draws in each mesh mode. Both executables rebuilt successfully. The non-audited 1,800-vblank opening smoke exited normally, retained 179,661 accepted model draws, avoided 555,711 CPU reference vertex evaluations, and bypassed generic decoding for 284,463 payload quadwords; no draw-attribute/state/capacity fallbacks or rejected candidate vertices were reported. Four synthetic C test programs and the public 31-test suite passed. These checks do not establish locked 60 FPS, full-game visual parity or audio acceptance. Local logs: `work/build-model-fast.log`, `work/model-fast-public-tests.log`, `work/gpu-model-fast-smoke/run.log`; captures are in ignored `work/model-gpu-parity-*` directories.


## Per-activation safety-check reuse

The user reported improved performance with the preceding non-audited model path. No controlled FPS improvement has been established locally.

The next optimization caches immutable clipping and projection-safety results per input vertex for one VU activation. Shared strip vertices reuse both positive and negative results. Every new accepted activation clears the cache; it cannot persist across changed transforms, models or frames. Mutable packet UVs and GS draw attributes are still checked on every use. Audit mode deliberately recomputes its checks. Empty/clipping-generated packets are rejected from their headers before any geometry safety transforms.

Synthetic regression coverage includes adjacent strip reuse, invalidation on changed constants, cached rejection, unchanged snapshot semantics after live VU memory changes, per-packet NaN rejection despite cached geometry, uncached audit checks, and early empty-packet rejection. All four native test executables passed. Both executables were rebuilt.

Final hangar/flight baseline-audit-fast comparisons remain byte-identical, retaining 14,444 and 2,268 model draws respectively. The hangar fast mode evaluated 71,907 safety transforms and reused 25,956 results. An intermediate cache-only build evaluated 114,351 transforms; early packet rejection removes further unnecessary work. These are operation counts, not frame-rate measurements. Logs: `work/model-gpu-parity-jkxz1djp` (hangar), `work/model-gpu-parity-ix4yd4ho` (flight).

Launch options remain `play.cmd -ModelMesh`, or add `-ModelAudit` for independent reference comparison. This optimization is confined to the experimental model path. Complete VU program bypass still requires native culling, secondary material passes and continuation-state preservation.


Final opening smoke: 1,800 vblanks, exit 0; 179,661 accepted model draws, 555,711 forwarded vertices, zero attribute/state/capacity fallbacks and zero rejected candidate vertices. Safety cache reused 310,260 results; 806,392 checks were evaluated. The public 31-test suite also passed. Private log: `work/gpu-model-safety-smoke/run.log`. No V3 changes were pushed and the baseline remains the default.


## Reuse identical GPU model constants

AC4 model program 6 now reuses the preceding identical constant block in the current mesh arena. The comparison covers the complete block, including transforms, lighting, fog, offsets and depth normalization. Every draw still records its own geometry, material state and order. Other mesh programs retain their original allocation behavior. Arena reset naturally invalidates reuse; no index or persistent disk cache survives into a different arena.

The synthetic production-helper regression checks exact reuse, changed offsets/fog/depth, preservation of previously stored values, arena reset, capacity exhaustion, and disabled/non-model behavior. All four native test programs and the 31-test public suite passed. Both executables rebuilt; existing signedness warnings in the unrelated overlay bounding-box function remain.

Baseline/audit/fast final images remain identical in both local captures:

- Hangar, 8 fields: 14,444 model draws retained; 13,596 duplicate blocks reused (about 94%), eliminating 5,655,936 bytes of duplicate constant data from the recorded arenas.
- Flight, 2 fields: 2,268 model draws retained; 2,175 duplicate blocks reused, eliminating 904,800 bytes.

This reduces constant-block storage and the corresponding per-frame upload, not draw count or original VU execution. No FPS gain is claimed without a controlled timing comparison. Launch remains `play.cmd -ModelMesh`; the default renderer is unchanged. Local capture logs: `work/model-gpu-parity-ctqs6afx` and `work/model-gpu-parity-cfpxuw20`. Build log: `work/build-model-blocks.log`.

Final opening smoke: 1,800 vblanks, exit 0, 179,661 model draws accepted, zero attribute/state/capacity fallbacks. Reused 170,398 constant blocks (70,885,568 bytes of duplicate recorded constants avoided). Log: `work/gpu-model-blocks-smoke/run.log`. This is boot stability evidence, not full mission/audio acceptance. Nothing was pushed.


## Active AC5 parity effort

See [AC5-PARITY.md](AC5-PARITY.md) for the capability checklist and completion gates. Native model materials are now separately selectable with `play.cmd -ModelMaterials` (implies model meshes). Supported states use mesh.frag; unsupported materials retain GS shading. The original VU still runs. Hangar and flight four-mode comparisons match exactly; 33 public tests and four native test programs passed. Native texture preparation/mips, full VU bypass and the window-resolution HUD/radar/radio layer are still outstanding.

## Native unlit program replacement

`play.cmd -NativeUnlit` additionally replaces supported unclipped, culled and mixed/clipped unlit VU activations with native calculations. It validates the complete activation before submitting packets and preserves original GIF/GS side effects. This expands CPU-side program coverage; the GPU model-mesh eligibility described above remains limited to its validated first-pass triangles. See [AC5 parity evidence](AC5-PARITY.md) for coverage and remaining gaps.


`play.cmd -NativeLit` extends opt-in native VU execution to supported lit model programs, including both material streams and nonempty clipped polygons. It also enables native unlit execution/model meshes. Unsupported layouts and continuation states fall back before any packet is committed. Combine with `-ModelMaterials` or `-ModelTextures` to test those paths. Original GIF/GS side effects remain; GPU mesh coverage still has the limitations above. See AC5-PARITY.md for capture-specific acceptance and remaining gaps.


The opt-in model mesh path now also covers eligible secondary material triangles for the recognized lit model program. Raw normals produce environment-map coordinates in the vertex shader; both passes are individually checked against GS submissions. This does not expand clipped polygon or unsplit-strip GPU eligibility. `--second-pass` in the model replay test requires actual secondary draw coverage.


## Combined experimental play preset

Use `play.cmd -HybridRenderer` (or `python tools/setup_ac4.py run --hybrid-renderer`)
with a native build. The preset combines supported model/terrain VU bypass,
native geometry/materials, supplied terrain mip filtering, sky/cloud/sprite paths,
and automatic window-resolution HUD precomposition. Unsupported states retain GS
fallback. Saved resolution/anisotropy remain in use. Capture mode forcibly ignores
the preset; it cannot be combined with bootstrap selection or the older triangle
bridge. Normal play defaults remain unchanged. This provides a way to validate the
combined path; it does not promote it to release-quality parity.


### Mission 2 graphics-worker experiments (2026-10-19, rejected candidates)

Native hybrid rendering and full 4K output remained enabled. A lowest-vacant
texture-slot hint failed to improve the matched replay workload and was removed.
A SIMD PSMT8 upload-column prototype preserved scalar VRAM/state results in
544,320 oracle cases and produced identical final images on both captures.
However, corrected same-binary enabled/disabled comparisons were mixed: early
flight improved 8.26% on cold first passes, while late flight regressed 2.30%.
The live cold-application-cache native route also failed acceptance: flight
p99 was 35.036 ms, with 56 intervals above 33.37 ms and one 104.836 ms interval.
The prototype was removed; source and executable were restored to the prior
validated build. These experiments establish no retained performance gain.

The 104.836 ms outlier includes 69.532 ms between publication and renderer
latch, rather than a matching GPU submission or allocation stall. The worker
spent 34.434 ms applying that field; Vulkan host work was 4.104 ms, and traced
allocation time was zero. Other 46-49 ms intervals do coincide with worker CPU
bursts. Both worker work and renderer handoff/event scheduling remain open.
All 16,200 source fields were delivered; the original save was unchanged.

Cold tests use empty application pipeline-cache prefixes, with shipped startup
pipeline preparation enabled. Driver/OS caches are not purged. An earlier
prototype harness incorrectly used a presence-based disable variable set to 0;
those preliminary comparisons are invalid and excluded from the results above.
Private evidence: E:/ac4-renderer-evidence/mission2-20261019/
WORKER-NEXT-EXPERIMENTS.txt, columns-isolated-comparison.json,
columns-live-native-cold/summary.json and spike-detail.json.
