# AC5 renderer parity target

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


Reference: the local AC5 source at `reference/ac5`, particularly README.md, `rn_vp.c`, `rn_vp_model.c`, the native GS state/texture adapter, and Vulkan mesh shaders. This is a renderer capability comparison against that checkout, not a promise of identical games or an unverified comparison against future upstream changes.

The user requested continuing until parity. The objective remains active. Small CPU/storage optimizations do not satisfy it. The accepted AC4 renderer stays available and V3 is not to be pushed without review/authorization.

## Pending manual-play reports (2026-10-18)

- **Mission 2 UI flickering:** user observed multiple UI flickering issues during the latest manual playtest. Affected elements and triggers have not yet been isolated. Open; investigation deferred at the user's request.
- **Mission 1 flight performance:** user observed large frame drops during actual in-mission flight, making flight noticeably fail to hold a solid 60 FPS. This is a gameplay issue, not limited to briefing/loading. Open; investigation deferred at the user's request.
- These manual findings take precedence over the narrower automated route's performance/visual acceptance. Do not mark UI flicker or sustained flight performance resolved based on those tests. No code changes or new test runs requested for these reports.

| Capability | AC4 status | Remaining acceptance |
| --- | --- | --- |
| GPU model projection and lighting | Hybrid preset covers validated lit/unlit primary, secondary, clipped and continuation paths; aircraft/hangar captures pass | Hybrid preset now includes validated cockpit meshes/native execution; Mission 1 camera route passes, broader scenes remain |
| Native material shading | Hybrid preset enables eligible model and supplied terrain mip materials; exact capture comparisons pass with preserved sampling | Interactive materials/effects review across relevant scenes |
| VU/GIF bypass | Hybrid preset runs validated model/terrain variants natively and bypasses recognized whole mesh GIF packets; unsupported states retain original execution | Broader game-state coverage and fallback review |
| Terrain | Native ordinary/clipped terrain, continuation and supplied mip chains validated against flight/island captures | Motion/minification and multiple mission acceptance |
| Sky and clouds | Raw GPU dome/haze, cloud-plane and sprite adapters pass captured comparisons; native band/row helpers enabled | Broader live scenes; full EE producer elimination is not an AC5 requirement (see below) |
| Mipmaps and anisotropic native textures | Native generated model mips and preserved terrain mips; hardware trilinear/aniso selectable, sampler changes pass exact 1080p/4K controls | Broad temporal filtering review and texture-store pressure validation |
| Window-resolution HUD and normal radar | AC4 helper groups plus pre-display composition; exact 1x capture parity, automatic high-resolution display and live Mission 1 developer-route evidence | Alternate map, broader producer coverage and combined preset transition acceptance |
| Window-resolution radio captions | Caption font/name draws are lifted by the safe trailing-draw path; direct per-draw coverage and visible live captions verified | Live caption transitions and unsupported-state coverage; audio sync needs interactive review |
| GS fallback | Retained for unsupported geometry/material/UI states, menus/hangar/effects | Continue checking scene transitions alongside native draws |

`play.cmd -HybridRenderer` selects these features together. Ordinary play retains
its previous defaults. This table summarizes current implementation; dated notes
below preserve earlier milestones and failures, not the current feature inventory.
Full renderer parity is still unaccepted pending the listed coverage checks.

## Implementation order

1. Native material shader eligibility and fallback; validate without changing texture sampling.
2. Native texture preparation and filtering, independently selectable from geometry. Do not apply AC5 guest addresses to AC4.
3. Full program replacement for one AC4 model variant. Record and compare continuation state and every pass; eliminate original execution only for the validated subset.
4. Expand AC4 program coverage to the other model variants, terrain, sky and clouds.
5. Adapt HUD/radar/radio captions to native resolution using AC4-specific guest data and hooks.
6. Run the full renderer acceptance route and document any remaining uncovered categories honestly.

Texture replacement packs, broader menus/fonts overhaul, PC options and save/load/Alt-Tab are tracked separately in V3 notes; they are not silently counted as done by renderer parity.

Reference qualification (2026-10-15): local AC5 `runtime/src/rn/rn_intent.c`
registers `ps2_hook_after` for `F_SKY_DOME`, before/after hooks for
`F_SKY_HAZE`, `F_CLOUD_PROJECT` and `F_SPRITE_ROWS` (lines 642-669).
The taps read entry arguments, original output pointers and producer state;
they return 0, retaining original EE producers. Therefore eliminating every
original sky/cloud EE producer is an additional optimization, not something
proved necessary by AC5's renderer description. AC4's native sky-band/cloud-row
replacements are useful separately. This distinction does not waive native
geometry/shading, validated VU/GIF bypass, material/texture coverage or broad
live acceptance gates, which remain incomplete. Review actual reference paths
when resolving the older milestone notes' broader "producer bypass" wording.

AC5 2D qualification: `runtime/src/rn/rn_intent.c` records generic
`RN_INT_PRIM2D` primitives and `RN_INT_GROUP2D` packet ranges (`tap_writer`,
`tap_group`, lines 170-270). Its `rn_2d.c` consumes these groups, preserving GS
material state and optionally reanchoring screen elements. A separate semantic
font/string rewrite is therefore not established as a requirement for matching
its window-resolution captions. AC4 still needs producer/placement/transition
coverage; its conservative `RN_G_WORLD` flag preserves original positions rather
than asserting that every captured element is world-attached. Higher-resolution
replacement font artwork remains a separate modding feature.

## Latest material milestone

`play.cmd -ModelMaterials` implies model meshes and enables native fragment shading only where the inherited material predicate accepts the GS state. `-ModelMesh` retains GS fragment shading for comparison. Capture sessions forcibly disable both. Normal play is unchanged.

Hangar: all 14,444 model draws used mesh.frag; final PPM identical to baseline. Correction: that earlier two-field capture ended on a black frame and cannot establish visible flight acceptance. Replacement eight-field third-person flight capture: 1,100 model draws, all four modes identical (SHA256 `66357448cd87cdc488b5c652ac35cbb896bd2493b8e871482e0dd4df34f295cc`). Logs: `work/model-gpu-parity-v3dyyw4q`. Uniform final frames now fail GPU acceptance. Baseline, audit, non-audited meshes and native materials were compared using `tools/test_ac4_model_gpu.py <capture> --fields N --materials`. Synthetic runtime tests passed; 33 public unit/launcher tests passed. This validates the captured subset, not whole-game parity. Local logs are under ignored `work/model-gpu-parity-jnlv4pmk`, `work/model-gpu-parity-pl8qku3g`, and `work/model-material-public-tests.log`.

Native-material opening smoke: 1,800 vblanks, normal exit; 179,661 model draws recorded with mesh.frag, zero attribute/state/capacity fallbacks and no rejected candidate vertices. Log: `work/gpu-model-material-smoke/run.log`. This is bounded boot stability evidence; interactive mission/movie/audio acceptance remains required. Next: adapt AC4 texture preparation using the inherited native texture API, with explicit opt-in and independent validation.


## Native texture adapter (2026-10-15)

`play.cmd -ModelTextures` implies native materials and model meshes. It copies eligible decoded model textures into the inherited native mip cache, leaving GS source textures intact. Unsupported states and allocation failures retain the prior path. Capture sessions disable it; default play remains unchanged. Region clamp bounds and ownership/fallback cases are covered by synthetic C tests. Mip sampling and anisotropy follow the inherited renderer settings; point-filtered draws do not imply visible lower-LOD sampling.

Against native materials without texture conversion, the visible hangar (14,444 converted draws) differed by at most 3 RGB levels; third-person flight (1,100) by at most 2. Images were inspected for intact aircraft and scene rendering. A four-field carrier-deck frame (4,536) differed by at most 21 levels. These are bounded final-image checks, not all-frame or whole-game acceptance. Private artifacts: `work/model-textures-jxsvajo9`, `work/model-textures-ujg6qy0n`, `work/model-textures-8lvk_wxc`. The island and sun GS captures exercised zero model draws and are not native-model coverage evidence.

All 38 redistributable unit/launcher tests pass. Full VU bypass, native terrain/sky/clouds and window-resolution HUD/radar/radio remain outstanding. Native textures are still opt-in pending broader motion/minification acceptance.


Texture safety review: an out-of-image region clamp now falls back instead of truncating the crop, which could change wrapping. The rebuilt third-person flight replay retains all 1,100 native texture draws and the same maximum two-level difference. Four synthetic native test executables pass, including the rejected-crop invariant. Both executables rebuilt successfully.

Next implementation gate: full native model execution needs a separate shadow path, comparing final VU data/register state and the ordered GIF packet effects against original execution. Existing `vu1_snap_take/put/diff` and kick hashes in `ps2_vu.c` verify recompiled microcode, not the new model renderer; do not count those as evidence of native renderer bypass. Start with the unlit image and preserve continuation entries and later material passes before allowing execution suppression.

Final rebuilt native-texture boot smoke: 1,800 vblanks, exit 0, 179,661 native mip/material draws, zero texture allocation/unsupported-state fallbacks. Log: work/gpu-model-texture-smoke/run.log. This establishes bounded boot stability, not locked frame rate or full-game visual acceptance.


## Unlit native execution groundwork (2026-10-15)

The AC4 unlit program now has an independent native packet producer for wholly unclipped input batches. It derives the complete ordered strip/triangle packet sequence from input constants/vertices, including EOP/sign extension and alternating output banks. An opt-in observer (`PS2_AC4_MODEL_PLAN=1`) compares each packet, final 16 KiB VU data memory, and independently predicted vector/integer registers, accumulator, Q/P/I, clip/random state and mapped continuation controls. It does not suppress any VU execution and is not a performance feature yet.

Hangar acceptance: 412 eligible activations, 805 packets, zero packet/memory/sequence/control/register mismatches; 21,308 unlit activations declined. GS, audited meshes, fast meshes and plan-observed meshes have byte-identical final visible images. Private log: `work/model-gpu-parity-tofg5zyl/plan.log`. Run `python tools/test_ac4_model_gpu.py <private-capture> --fields 8 --unlit-plan`. The test rejects missing/nonzero mismatch reports and zero coverage. Synthetic tests cover wrapped addresses, output banks, strip/split ordering, unsupported/clipped input, unchanged failed output, missing packets, memory corruption and altered vector/integer registers.

The third-person flight capture has zero unlit activations and correctly fails this coverage gate (`work/model-gpu-parity-4adeswvc`). A 1,800-vblank opening smoke exits normally but likewise has zero unlit coverage (`work/gpu-unlit-plan-smoke/run.log`); it is boot stability evidence only. The existing lit GPU path is still exercised there.

Remaining before execution bypass: port/verify arithmetic flags and pending pipeline state, finish all continuation controls, support clipping/culling and later material passes, then validate native submission and fallback. The current plan is derived from the AC4 program layout, not AC5 guest addresses. No claim of full program replacement or AC5 parity yet.

Validation for this stage: both executables rebuilt; all four synthetic native test programs and all 40 redistributable unit/launcher tests passed. Default play remains unchanged. No public push performed.


## Unlit arithmetic and continuation validation (2026-10-15)

The independent unlit model now reconstructs delayed arithmetic flags using a four-slot pipeline model. This covers status/MAC, sticky status propagation, queued flag contents and the ending pipeline slot. Register comparison now also checks Q/P pending values/readiness, TOP/ITOP/CMSAR, TPC, CC and running state. No original instruction decoder is used by this predictor. It still observes original execution; bypass is not enabled.

Rebuilt hangar acceptance passes for all five modes (GS, audited mesh, fast mesh, native material and unlit plan). All 412 supported activations/805 packets match final memory, vector/integer/accumulator registers, continuation controls and arithmetic pipeline state. Private evidence: `work/model-gpu-parity-uynn5afk/plan.log`. The 21,308 declined unlit activations remain uncovered. Immediate-Q mode (`PS2_VU_QLAT=0`) independently passed the same state and image checks: `work/unlit-immediate-q-rns2orh7/run.log`.

Synthetic checks exercise delayed commits, negative zero, masks and sticky-flag propagation, plus a manually specified split-batch register fixture and a 381-cycle native schedule. GPU acceptance now rejects missing or nonzero arithmetic-pipeline mismatch reports. The native schedule's total instruction count is not yet compared to original execution; the final slot and all queued flags are compared. This is captured-subset evidence, not proof over all inputs.

Next: integrate a separately selectable native execution path for the validated unclipped unlit subset, check actual cycle accounting and packet submission against the existing baseline, and keep clipping/unknown-state fallback. Then expand coverage to clipped unlit cases, lit programs and other scene geometry. Default play remains unchanged, and full AC5 parity remains incomplete.

This stage rebuilt both executables successfully; all four native test programs and all 41 public unit/launcher tests pass. Shared Git metadata was read only; no commits or pushes performed.


## First native unlit execution path (2026-10-15)

`play.cmd -NativeUnlit` now executes accepted unclipped unlit activations without running their original VU instructions. It implies model meshes; combine with `-ModelMaterials` or `-ModelTextures` to exercise those independently selectable paths. Original execution remains the default, capture sessions disable the option, and `PS2_AC4_MODEL_PLAN=1` or VU recomp verification retains original execution for comparison. Unknown/lit/clipped inputs and batches exceeding the runtime instruction guard fall back before guest state or packets are modified.

This implementation still constructs and submits ordered GIF packets through the existing callback. It is VU execution bypass for a subset, not full GIF bypass or native support for all scene geometry. The native state includes vector/integer/accumulator, Q/P and pending values, arithmetic flags and their queue, continuation fields and output memory. The independent instruction schedule is now checked against the original run's actual count, not just its final pipeline slot.

Initial hangar bypass test: 412 native activations, 805 submitted packets and 154,165 original VU instructions replaced; same coverage as the independent shadow comparison. All six initial comparison modes have identical visible final images, and the shadow run reports zero instruction-count mismatches. Private logs: `work/model-gpu-parity-m2g5rnlg`. These are instruction counts and bounded rendering checks, not measured frame-rate improvements. Broad interactive gameplay acceptance remains pending.

Synthetic tests cover actual ordered callback submission, expected final controls, read-only decline for clipped/unknown inputs, verification-mode exclusion and the instruction-guard boundary (380 instructions declines; the 381-instruction fixture executes). Native execution remains opt-in while coverage expands.


Final rebuilt bypass acceptance: seven modes now include native execution combined with native material shading. Both bypass modes replace 412 activations/154,165 original instructions and submit 805 packets; all seven final PPMs match exactly. Evidence: `work/model-gpu-parity-gjzrp9d2`. The guard-limited path was included in this build. All four native test executables and all 44 public unit/launcher tests pass.

Next coverage target: the 21,308 declined unlit hangar activations, beginning with wholly culled triangles and then partial clipping. Empty-packet submissions and their VU continuation state must be preserved; zero visible geometry does not imply a program can simply be skipped. Lit models, native terrain/sky/clouds, direct GIF bypass and window-resolution HUD/radar/radio are still separate incomplete requirements.

Native-unlit-enabled opening smoke completed 1,800 vblanks with exit 0 (work/gpu-native-unlit-smoke/run.log). It exercised zero unlit activations, so this proves fallback/boot stability only. The hangar replay is the native bypass evidence. Default play unchanged; no push performed.


## Whole-batch native culling (2026-10-15)

The unlit adapter now supports batches whose triangles are all wholly culled. It computes homogeneous clip flags, selects the original common-plane branch, preserves the empty EOP packets and alternating output banks, and reconstructs the cull path's registers, accumulated clip history, arithmetic pipeline and instruction count. The original far-W bypass is respected. Mixed visible/culled batches and partial intersections remain on the original VU path.

Expanded hangar shadow verification: 19,907 supported activations and 36,406 ordered packets; zero packet, final-memory, register, flag, control or cycle mismatches. Actual native execution matches that coverage and replaces 2,914,106 original instructions. This includes 19,495 wholly culled activations in addition to the earlier 412 unclipped activations. The remaining 1,813 unlit activations decline. These numbers describe this capture only, not whole-game coverage.

All seven final visible PPMs match exactly, including native execution combined with native material shading. Evidence: `work/model-gpu-parity-8vcc53kb`. Reproduce with `python tools/test_ac4_model_gpu.py <private-hangar-capture> --fields 8 --culling --materials`; `--culling` requires nonzero independent shadow culling coverage and matching actual native cull counts.

Synthetic checks cover all six cull planes, original branch-delay state, empty-packet payloads, exact 81+2*plane instruction counts, unchanged memory outside the packet header, partial-intersection rejection and far-W fallback. Both executables rebuilt; all four native test programs and all 45 public unit/launcher tests passed. No frame-rate improvement is claimed without measurements. Default play remains unchanged; no push performed.

Next: mixed unlit batches and partial clipping, followed by lit-program replacement and native terrain/sky/clouds. Direct GIF bypass and window-resolution HUD/radar/radio remain outstanding. Whole-batch culling is an expansion of native model execution, not completion of AC5 parity.


## Partial-clipping packet and scratch kernel (2026-10-15)

An independent AC4 unlit clipping kernel now processes the six homogeneous planes in original order. It preserves previous/current edge order, inside/outside interpolation direction and separate arithmetic rounding; intersections recompute clip coordinates from interpolated model positions. UVs, colours and normals are requantized before projection. The kernel records writes to the two scratch buffers and handles polygons that become empty. Capacity/invalid-numeric cases decline without publishing partial output.

Shadow coverage is selected from input clip flags and the original near/far-W branch, rather than relying on the eventual vi10 register. This increased observed clipping coverage from 2,184 to 2,438 packets by including additional empty polygons. All 2,438 packets and all recorded scratch writes match the original exactly, with zero declined comparisons. These are partial-clipping checks, separate from the 19,907 complete native execution activations.

Final combined test: `python tools/test_ac4_model_gpu.py <private-hangar-capture> --fields 8 --clipping --culling --materials`. All seven final PPMs match; existing native execution still covers 19,907 activations/2,914,106 original instructions. Private evidence: `work/model-gpu-parity-hhz33i9c`. The `--clipping` gate requires positive comparison coverage, exact packets and scratch data, and no declined comparisons. Synthetic tests cover every plane, interpolated boundary vertices/UVs/colours, empty results, scratch requantization and invalid-input rejection.

Partial-clipping execution bypass remains disabled. Its complete scratch-writing program also leaves live vector/integer registers and arithmetic/pending state; those still need to be reconstructed and shadow-verified before integrating mixed/partially clipped activations into native execution. The packet kernel is not a claim that this state work is complete. Lit programs, terrain/sky/clouds, direct GIF bypass and window-resolution HUD/radar/radio remain outstanding.

Validation: both executables rebuilt successfully; four native test programs and all 46 public unit/launcher tests passed. Default play unchanged; nothing pushed.


## Clipping integer continuation (2026-10-15)

The clipping kernel now returns its final polygon size and stopping plane, from which a separate helper reconstructs all 16 integer registers at XGKICK. It accounts for scratch-buffer swaps, early-empty polygons (including the wrapped count-minus-one register), output-bank parity, projected-record prefetch and the outer strip index before its post-kick increment. Unchanged registers are checked against activation input rather than copied from observed output.

All 2,438 clipping submissions match integer state, packets and scratch writes in `work/model-gpu-parity-1m0dkp5x/plan.log`. The final frame remains identical in the four-mode comparison. The `--clipping` verifier now rejects missing or nonzero integer-state mismatch evidence. Synthetic tests cover each stopping plane and polygon sizes zero, one and two, plus a visible four-vertex polygon and preserved registers. Both executables rebuilt; four native test programs and all 47 public unit/launcher tests passed.

Still incomplete: clipping's temporary vector registers, accumulator, Q/P and delayed arithmetic state, followed by assembly of mixed activations into native execution. Integer-state agreement alone does not authorize partial-clipping bypass; it remains disabled. Existing native unlit coverage and default play are unchanged.

Implementation note for the next state step: the clipper loads decoded attributes for all three vertices, retains interpolation temporaries between edges, and prefetches a record beyond each polygon. That prefetch can read unwritten scratch contents, so vector prediction must preserve the activation's scratch input and apply independently predicted writes in order. Reusing observed final scratch values would weaken the comparison. The float attributes are requantized before projection; empty polygons skip that projection path and retain different temporary registers.

## Clipping vector continuation (2026-10-15)

The independent clipper now traces written vector registers, the accumulator, I and Q when produced by an intersection or final projection. It seeds scratch memory from activation input and applies its own predicted writes in order, including edge-level writes before speculative prefetches. It never seeds predictions from observed final scratch. Early-empty polygons retain a different register mask; untouched registers and Q when no division occurs are not covered by this new trace.

All 2,438 partial-clipping packets match traced vector state, integer state, packet bytes and scratch writes with zero declined comparisons. Four replay modes produce the same visible final frame in `work/model-gpu-parity-o2incg64`. The acceptance gate rejects absent or nonzero vector mismatch reports. Synthetic tests additionally exercise all six clipping planes, visible/empty trace masks and unwritten scratch-prefetch preservation. Both executables rebuilt; four native test programs passed.

Partial-clipping bypass remains disabled: delayed arithmetic flags, pending Q/P state, cycle accounting and complete mixed-activation continuation still require independent validation. This is a state-verification milestone, not expanded native drawing coverage or measured performance improvement. Lit programs, terrain/sky/clouds, direct GIF bypass and window-resolution HUD/radar/radio remain outstanding.

Validation completed: all 48 public unit/launcher acceptance tests passed; Git diff whitespace check passed. Default play remains unchanged; nothing pushed.

## Clipping scalar continuation and pipeline drain (2026-10-15)

The shadow verifier now carries Q/P, pending-value storage and clip history from activation input through ordinary, culled and clipped triangles. Clipping-generated divisions replace Q; empty polygons without divisions retain the preceding triangle's value. Immediate-Q mode preserves pending storage. Unusually long inherited deadlines are declined until exact cycle accounting can establish their completion. Predictions never copy observed kick state.

The final MAC flags, current zero/sign status and all four flag-pipeline valid bits are also checked at each clipping kick. This is a drain check, not full sticky-status, pipeline-slot payload or cycle validation. Those remain required before actual partial-clipping bypass.

In `work/model-gpu-parity-k9tm0hp2`, all 2,438 partial-clipping submissions match packets, scratch, integers, vectors and the new scalar/drain checks, with zero declines. Four final visible PPMs remain identical. Both executables rebuilt; four native test programs and 49 public unit/launcher tests passed. Synthetic scalar tests cover inherited pending values, empty-result carry, immediate division storage and unsupported deadlines. The clipping acceptance gate rejects missing or mismatched scalar/drain evidence. Native bypass coverage and default play are unchanged; nothing pushed.

Immediate-Q replay also passed all 2,438 clipping checks and existing unlit state checks with identical final image: work/unlit-immediate-q-v__0atfp.

## Mixed clipping instruction counts (2026-10-15)

The independent clipping kernel now counts its path through setup, plane selection, edge classification, crossing interpolation, alternating scratch buffers, requantization, projection and packet submission. The observer combines this with ordinary/culled triangle costs and activation entry/exit. Branch-target padding and delay slots are included explicitly; no guest opcode decoder is used.

All 1,468 activations containing partial clipping match original executed instruction counts exactly in `work/model-gpu-parity-dtm_zou4`. The 2,438 packet/scratch/register/scalar checks also pass, and all seven final visible PPMs match with existing native unlit/material modes. Existing bypass still covers 19,907 activations and 2,914,106 replaced original instructions; clipping bypass is not yet enabled. Both executables rebuilt, four native test programs and all 50 public unit/launcher tests passed.

The clipping acceptance gate now requires nonzero mixed-activation cycle coverage with zero mismatches. Synthetic cases pin instruction costs for polygons emptied on each of the six planes. Full sticky arithmetic status, inactive pipeline-slot payloads, and complete mixed-activation register/memory continuation remain to be reconstructed before native clipping can replace the original execution. No new performance claim or default change; nothing pushed.

## Mixed clipping arithmetic continuation (2026-10-15)

The native calculations now feed an independent four-slot arithmetic scheduler across ordinary, culled and partially clipped triangles. Scheduled results include clip-plane classification, edge interpolation, attribute shifts, scratch requantization and final projection. The schedule uses the independently validated instruction counts, preserving separate multiply/add rounding and inherited sticky status. It does not execute guest instructions or copy final arithmetic state from original execution.

All 1,468 mixed clipping activations match complete status, MAC, final slot index and all four pipeline-slot payloads/valid bits in `work/model-gpu-parity-fazbv5gx`. The 2,438 packet/scratch/vector/integer/scalar checks and instruction counts also match; four final visible PPMs are identical. Immediate-Q mode independently passes the same gates in `work/unlit-immediate-q-g7mcky_d`. Both executables rebuilt; four native test programs and 51 public unit/launcher tests passed. Synthetic tests seed nonzero sticky/pending flags and exercise empty results at all six planes. The acceptance gate now rejects missing or mismatched arithmetic-state evidence.

Next: assemble complete mixed-activation memory and preserved register state, including transitions between clipped, ordinary and culled packets, then shadow-verify that complete result before enabling native execution. Partial-clipping bypass remains disabled. Lit programs, terrain/sky/clouds, direct GIF bypass and native-resolution HUD/radar/radio remain outstanding. Default play is unchanged; nothing pushed.

## Mixed activation memory and packet order (2026-10-15)

The shadow path now seeds a full 16 KiB VU memory image from activation input and applies independently generated ordinary, culled and clipped packets plus scratch writes. It compares the entire final image, including untouched memory. Packet order comes from an independent triangle counter; the observed integer index and output address are checked rather than used to select predicted inputs or destinations.

All 1,468 mixed clipping activations have identical full memory, with zero ordered-packet or sequence mismatches in `work/model-gpu-parity-b8fxa_pq`. Existing checks cover 2,438 clipping submissions, exact instruction counts and full arithmetic state; all four final visible PPMs remain identical. Both executables rebuilt and four native test programs passed. Synthetic cases cover integer-colour round trips, empty packet length, alternating banks, wrapped payloads, preserved surrounding memory and incorrect packet/address rejection. The clipping acceptance gate rejects absent or mismatched full-memory/sequence evidence.

Remaining prerequisite for actual mixed native execution: assemble all live/preserved registers across clipped, ordinary and culled packet transitions and compare the complete final VU state. Current masked clipping-register checks are not a substitute for that whole-activation result. Partial-clipping bypass remains disabled; default play is unchanged and nothing pushed.

Validation completed: all 52 public unit/launcher tests and the Git diff whitespace check passed.

## Complete mixed register continuation (2026-10-15)

The shadow predictor now starts from activation register input and carries state through every ordinary, culled and clipped triangle. Each path replaces only the registers it writes. This includes prefetched records, the clipping scratch temporaries, preserved registers and the outer strip-loop index. The complete final integer/vector/accumulator, Q/P and pending state, I, clip/random, execution and control state are compared; arithmetic status/MAC/pipeline remain covered by the separate full-state check.

All 1,468 mixed clipping activations match the complete register result in `work/model-gpu-parity-fdnzvybx`, as well as full memory, ordered packets, instruction counts and arithmetic state. Four final visible PPMs are identical. Both executables rebuilt; all four native test programs passed. Synthetic tests exercise untouched register preservation through ordinary/culling preludes, speculative record conversion, and detection of preserved-register and pending-state differences. The acceptance gate rejects missing or mismatched complete-register evidence.

Next is implementation and validation of actual mixed native execution: compute the entire activation before guest mutation, reject unsupported/overlapping input cases, respect the instruction guard, submit packets in order, and publish the verified final state. This shadow milestone alone does not expand bypass coverage. Direct GIF bypass, lit models, terrain/sky/clouds and native-resolution HUD/radar/radio remain outstanding. Default play is unchanged; nothing pushed.

Validation completed: 53 public unit/launcher tests passed. Immediate-Q complete clipping/state parity also passed in work/unlit-immediate-q-329gvykj with identical final image. Git diff whitespace check passed.

## Native mixed clipping execution (2026-10-15)

`-NativeUnlit` now executes supported mixed/clipped unlit batches natively. It computes all packets, scratch writes and final state before touching guest memory, checks numeric validity, disjoint input/scratch/output regions and the instruction limit, then submits original-format GIF packets in order. A rejected batch makes no guest writes or submissions. Packet formats continue through the existing GIF handler; the inherited model-mesh tag restriction is unnecessary for this VU replacement and does not restrict mixed execution. This is not direct GIF bypass, and clipped geometry still uses compatibility GS drawing.

Delayed-Q combined acceptance: `work/model-gpu-parity-fqimoe83`. All 1,468 shadow-eligible mixed activations execute natively, emitting 2,594 packets and replacing 3,628,673 original instructions. Together with existing whole-batch execution this is 21,375 activations and 6,542,779 replaced instructions in the eight-field hangar capture. All seven final visible PPMs match; shadow checks still cover complete memory/register/arithmetic state and ordered packets. These counts establish coverage, not measured FPS improvement.

Both executables rebuilt; four native test programs and 54 public unit/launcher tests passed. New synthetic execution tests cover clipped-to-ordinary bank order, final memory/register publication, and no mutation/submission when the instruction guard or invalid projection rejects the batch. The GPU verifier now requires actual mixed activation/packet/instruction counts to match positive shadow-eligible counts. Default play is unchanged; nothing pushed.

Next: broader native execution acceptance, remaining unlit fallback cases and lit-model program replacement; native terrain/sky/clouds, direct GIF bypass and window-resolution HUD/radar/radio remain required for AC5 parity.

Immediate-Q combined execution also passed all seven modes with identical coverage and final images: work/model-gpu-parity-21ww0hyi. Opening smoke completed 1,800 vblanks with exit 0 in work/gpu-native-mixed-smoke; it exercised zero unlit/mixed activations, so it establishes boot/fallback stability only. Git diff whitespace check passed.

## Remaining captured unlit batches (2026-10-15)

The mixed predictor now verifies every split unlit activation declined by the earlier whole-batch planner, including batches with no partial-clipping result. All 1,813 such activations match complete memory/register/arithmetic state and instruction counts. Native execution no longer requires at least one clipped packet; it retains the same preflight/no-mutation guarantees. A synthetic non-clipped packet-format case verifies that the existing GIF handler can receive the original tag without imposing the GPU model adapter's tag restriction.

Combined replay passed in `work/model-gpu-parity-go4zu8yc` (delayed Q) and `work/model-gpu-parity-4vn3je__` (immediate Q). Each executes 19,907 whole-batch plus 1,813 mixed-path activations: all 21,720 recognized eligible unlit activations in this capture, 39,630 packets and 6,663,878 replaced original instructions. All seven final visible images match in each mode. This is capture-specific coverage, not proof of every unlit path or whole-game parity. Both executables rebuilt; four native test programs passed. Default play unchanged; nothing pushed.

Next model target: the lit program has two three-qword material headers and two output streams. TOP+23.y bit 2 controls packet chaining; TOP+29.x, TOP+23.z and strip parity affect later material selection. The existing first-pass projection/lighting evaluator does not by itself cover the second stream or continuation state. A private listing is available at `work/lit-disasm.txt`, reconstructed from the local captured program; it is ignored and must not be redistributed. Begin with independent packet production and shadow comparisons before replacing lit execution. Terrain/sky/clouds, direct GIF bypass and native-resolution HUD/radar/radio remain outstanding.

Validation completed: all 54 public unit/launcher tests and the Git diff whitespace check passed.

## Lit second material stream (2026-10-15)

A new independent lit secondary-vertex evaluator derives environment texture coordinates from the transformed model normal and reciprocal projected W. The secondary colour is 128 in every channel for upward-facing normals; downward-facing normals clear RGB while retaining alpha. The observer also predicts both material headers and the first/second tag chaining bits from activation constants. It compares second-stream geometry with ADC masked, matching the existing first-stream observer's fog convention. Original lit execution remains enabled.

Hangar combined acceptance (`work/model-gpu-parity-0vh7cyu2`) matches 40,716 secondary vertices and both stream headers, with zero attribute/header mismatches or rejected eligible vertices. It skips 10,708 clipped submissions; this is not full lit-program coverage. All seven final visible images match, and complete unlit/mixed native execution checks continue to pass. Both executables rebuilt, four native test programs and 55 public unit/launcher tests passed. Synthetic cases cover normal direction, reciprocal-W scaling, alpha preservation and rejection without output mutation.

Use `--lit-plan` with the private GPU verifier to require nonzero matching lit second-stream/header evidence. It can run independently of unlit coverage, or alongside `--clipping --culling --materials`. Missing evidence, zero coverage, mismatches and rejected eligible vertices fail the gate. Remaining lit work includes ADC/backface suppression, clipping, complete packet/scratch production and continuation-state validation before native bypass. Default play and existing native bypass coverage remain unchanged; nothing pushed.

Third-person flight acceptance also passed: 14,036 secondary vertices and stream headers match, all five final images identical (work/model-gpu-parity-jq73js67). This capture exercises lit models but no unlit execution. Git diff whitespace check passed.

## Lit backface suppression / ADC (2026-10-15)

The independent lit geometry calculation now predicts ADC/backface suppression from projected triangle winding, material controls, strip parity and the near/far-W branch. It reproduces float fog addition before FTOI4 instead of ORing bit 15 afterward; the two differ at rounding boundaries. Both streams' complete fourth geometry lane are compared, and the secondary geometry comparison no longer masks ADC. Original lit execution remains enabled.

Hangar replay `work/model-gpu-parity-f7gnp0_u` matches 40,716 secondary vertices, both headers and all ADC lanes, including 640 marked vertices, with zero mismatches. All five final visible PPMs match. Synthetic tests cover strip parity, material reversal, disabled suppression, degenerate triangles, invalid counts and a fractional-fog rounding case. Both executables rebuilt; four native test programs and 56 public unit/launcher tests passed. The lit acceptance gate requires ADC evidence and rejects any mismatch; zero marked vertices does not establish active suppression coverage and must be reported accordingly.

This supersedes the earlier ADC-masked lit check. Remaining work: complete lit packet production, clipped polygons and scratch writes, then full continuation-state validation before enabling native lit execution. Existing unlit native execution and default play remain unchanged; nothing pushed.

Third-person flight also passes full lit secondary/ADC comparison: 14,036 vertices, 240 marked, zero mismatches, five identical final images (work/model-gpu-parity-0b069rr4). Git diff whitespace check passed.

## Complete lit two-stream packets (2026-10-15)

An independent packet builder now composes both complete lit material streams from constants and input records: material setup headers, tag chaining, STQ, colour, geometry and ADC. It preserves the integer-colour float round trip and computes the second stream even when the first tag ends GIF submission, matching the VU's memory writes. Output and size are published only after all vertices succeed.

Hangar evidence `work/model-gpu-parity-c0c5zp2o` matches all 13,572 eligible complete packets word-for-word (40,716 vertices per stream), with five identical final visible images. The `--lit-plan` gate now requires nonzero complete-packet coverage and no mismatches, alongside existing secondary/header/ADC checks. Both executables rebuilt; four native test programs and 57 public unit/launcher tests passed. Synthetic coverage includes both chaining modes, material header order, lit/unlit primary colour selection, alpha preservation, maximum packet size and failure on the last vertex without modifying output.

This remains unclipped packet generation under original execution. Next: clipped lit polygon output and scratch writes, then complete continuation-state verification and native execution. The unlit clipping kernel already produces requantized UV/colour/normal records that can be evaluated for reuse, but the lit program's headers, lighting, ADC and state must still be verified independently. Default play and existing native execution coverage remain unchanged; nothing pushed.

Third-person flight also passes complete-packet comparison: 3,796 packets (14,036 vertices per stream), zero mismatches and five identical final images in work/model-gpu-parity-p9pb2g7z. Git diff whitespace check passed.

## Lit clipped packet/scratch generation (2026-10-15)

The shared clipping calculation now supports the lit program's explicit +0.001 interpolation-factor bias. It is applied after the absolute edge factor and before attribute interpolation. Unlit calls retain zero bias and their validated schedule/state; biased calls cannot request the unlit state/flag trace. Lit output uses requantized clipped attributes, the clipped material tags and fixed polygon winding for ADC. Empty polygons produce the original empty tag.

Hangar evidence `work/model-gpu-parity-0ook5m59` matches 2,022 clipped lit packets and all masked clipping scratch writes (VU qwords 32..111), with zero mismatches or declines. The capture has zero empty results in this subset; empties are covered by synthetic tests, not claimed as replay coverage. All seven final visible images match, and existing unlit native execution/state checks still pass. Both executables rebuilt; four native test programs and 58 public unit/launcher tests passed. Synthetic cases exercise biased intersections on all six planes, clipped tag selection, empty output and failure without output mutation.

`--lit-clipping` implies `--lit-plan` and requires positive clipped packet/scratch coverage with no mismatches or declines. This does not yet verify lit winding scratch qwords 0/1, full activation memory or continuation registers/flags/cycles. Those remain required before native lit execution. Default play and existing bypass coverage are unchanged; nothing pushed.

Flight clipping attempt `work/model-gpu-parity-bvdc3v59` correctly failed the positive-coverage gate: it contains zero clipped lit submissions, not a mismatch. Its 14,036 nonclipped lit vertices and complete packets still pass the existing lit gate, and all five saved nonuniform final images match. Do not count this capture as lit clipping acceptance. Git diff whitespace check passed.

## Complete lit activation memory and packet order (2026-10-15)

The lit observer now independently chooses triangle order and output banks from saved inputs, predicts ordinary/clipped/culled packets, applies clipping scratch writes and predicts winding scratch qwords 0/1. It checks every emitted packet and the complete 16 KiB final VU memory, including untouched regions. Its activation lifetime is independent of the mesh observer, so unsplit packets are checked after the mesh observer finishes. `--lit-plan` now requires positive full-memory/order evidence with zero memory, sequence or packet mismatches and zero declines.

Flight evidence `work/model-gpu-parity-brdowp6t` passes 3,796 complete activations/packets with zero mismatches or declines and five identical final visible images. Flight still provides no clipped-lit coverage. Synthetic winding tests cover the final two projected vertices, disabled-winding preservation, invalid counts and rejection of nonfinite input without partial output. All four native renderer test executables and 59 public unit/launcher tests pass; both executables rebuilt.

This is an observer-only step toward native lit execution. Full lit continuation registers, flags and instruction counts still need independent validation. Default rendering and existing native bypass are unchanged; no new performance claim and nothing pushed.

Combined hangar acceptance `work/model-gpu-parity-6hsx5ofp` passes 13,800 complete lit activations and 24,280 packets, including 2,022 clipped packets, with zero memory/sequence/packet mismatches or declines. All seven final visible images match. Existing whole and mixed unlit native execution retains matching shadow coverage and state (21,720 activations total). These are captured-scene checks, not proof for all missions or every intermediate frame. Git diff whitespace check passed.

## Lit integer registers and continuation (2026-10-15)

The lit shadow predictor now carries all 16 integer registers between triangles, checks them at every XGKICK, and checks final integer state, stopped execution and continuation PC 0x10c0. Predictions use saved input data and preceding predicted state, not observed output registers. Ordinary and clipped polygons include prefetch pointer advancement, both material stream pointers, winding/parity and winding-loop counter. Culled triangles preserve untouched registers; their vi1 result includes the following FCOR in the branch delay slot, including the final-plane exception.

`--lit-plan` now requires integer coverage to equal the complete memory-verified packet/activation counts with zero mismatches or declines. The first hangar attempts exposed the cull delay-slot difference and are retained privately as failed evidence; they are not acceptance. Flight evidence `work/model-gpu-parity-z457qb2a` passes 3,796 activations/packets and five identical final images. Synthetic cases cover retained culled registers, sequential culls, clipping scratch input pointers, disabled winding, strip parity, inherited vi14/15 and adjacent/final-plane cull delay slots. All four renderer executables and 60 public unit/launcher tests pass.

Empty polygons produced by partial clipping still explicitly decline integer prediction; the captures have not exercised that branch. Vector/accumulator/scalar state, flags and exact instruction counts remain to be modeled and validated before native lit execution. Original lit execution remains in use. Nothing pushed.

Final combined hangar evidence `work/model-gpu-parity-lqpawup3` passes all 24,280 lit kick integer comparisons and 13,800 final continuations, with zero mismatches or declines. Full-memory and clipped-packet gates also pass, all seven final visible images match, and whole/mixed unlit native execution retains matching coverage and state. Git whitespace check passed. These results remain capture-specific.


## Lit scalar continuation state (2026-10-15)

The lit shadow predictor now checks Q/P, pending Q/P, drained deadlines, I, 24-bit clip history and unchanged random state at each kick and activation end. Visible polygons use their independently projected final vertex for Q; culled triangles retain previous Q. Split triangles append their three independently calculated clip codes. I accounts for the immediate instruction in the winding branch delay slot. Unusually long inherited scalar deadlines and empty partially clipped polygons explicitly decline; no original output state is adopted.

Delayed-Q hangar `work/model-gpu-parity-1eg634dk` passes the scalar acceptance gate for all 24,280 packets and 13,800 activations. Flight `work/model-gpu-parity-we_o59ud` passes all 3,796 packets/activations with five identical final images. Immediate-Q hangar `work/model-gpu-parity-tq9efe7e` passes the full lit gates plus existing whole/mixed unlit native checks with seven identical final images. All mismatch/decline counts are zero. These are capture-specific results, not full-game or every-frame proof.

The lit-plan gate requires scalar packet/activation coverage to equal memory and integer coverage. Synthetic tests check pending-value draining, immediate versus delayed Q, preservation of I/random/clip values and explicit rejection of long inherited deadlines. Both executables rebuilt, four native renderer test programs and 61 public unit/launcher tests passed. Vector/accumulator state, MAC/status pipelines and exact instruction counts remain required before native lit execution. Default rendering remains unchanged; nothing pushed.

## Lit vector and accumulator continuation (2026-10-15)

The lit observer now independently predicts every vector register and the accumulator at each kick and activation end. It includes both lighting/material streams, winding cross-product intermediates, the accumulator's preserved W lane, normal-matrix reloads, one-record-ahead prefetch/conversion and inherited untouched registers. Culled triangles retain the preceding state except for the independently predicted clipping prelude and empty output. Clipped polygons expose only residual vector values from the shared polygon kernel; they do not reuse the unlit register schedule, flags or cycle model. Residual outputs are published atomically with successful lit packet production.

The full lit gate now requires vector coverage to equal memory/integer/scalar coverage, with zero skips and mismatches. Initial ordinary-only evidence deliberately excluded clipping; it is superseded by full hangar coverage in `work/model-gpu-parity-dodngup9`: 24,280 packets, 13,800 activations, zero mismatches/skips. Final flight build `work/model-gpu-parity-0i1807op` passes all 3,796 packets/activations with five identical final images. Empty partially clipped polygons remain explicitly unsupported for continuation prediction, and were not exercised by these captures.

Both executables rebuilt. All four native renderer tests and 62 public unit/launcher tests pass. Synthetic coverage checks preserved registers, matrix and prefetch state, corruption detection in untouched registers/accumulator, six-plane clipping residuals and atomic failure behavior. Exact MAC/status pipeline state and instruction counts remain to be validated before native lit execution. Default play remains unchanged; nothing pushed.

Final combined hangar acceptance `work/model-gpu-parity-as4xn3j2` passes all memory/register gates for 24,280 lit packets and 13,800 activations, including 2,022 clipped packets. All seven final visible images match; whole/mixed unlit native execution retains matching coverage/state. Git whitespace check passed. These are capture-specific checks, not every-frame/full-game proof or a performance claim.


## Lit instruction-pair accounting (2026-10-15)

The lit predictor now independently counts executed instruction pairs through initialization, split-triangle clip tests, material headers, lighting loops, winding/ADC branches, output banks and continuation. The shared polygon kernel exposes preparation counts separately from its unlit projection schedule. Lit interpolation includes its additional divide-wait/bias padding; its longer clipping prelude is counted independently. This predicts the runtime instruction guard count, not physical PS2 cycles or host frame time.

Default-Q hangar `work/model-gpu-parity-bnxlt_a_` matches all 13,800 activation counts and all other lit gates, with five identical final visible images. Flight `work/model-gpu-parity-g5t01q9u` matches all 3,796 activation counts, registers and memory, also with five identical images. Earlier attempts that omitted lit-specific prelude and crossing padding are retained privately as failed evidence, not acceptance. The lit-plan gate now requires instruction-count coverage to equal complete register/memory coverage, with zero mismatches or declines.

Synthetic tests cover material branching, odd output banks, winding reversal and the winding-loop work difference. Empty partially clipped polygons still decline continuation prediction and are not exercised by these captures. MAC/status and pending arithmetic flags remain to be modeled and validated before native lit execution. Default play is unchanged; nothing pushed.

Immediate-Q combined hangar acceptance `work/model-gpu-parity-7w_gc02q` also passes every lit count/register/memory gate and existing whole/mixed native unlit checks, with seven identical final images. Both executables rebuilt; all four native renderer test programs and 63 public unit/launcher tests pass. Git whitespace check passed. Capture comparisons cover final images and observed activations, not every frame or all missions.

## Lit arithmetic flag pipeline (2026-10-15)

The lit observer now predicts MAC/status and all four pending arithmetic flag slots through the visible lighting, projection and winding loops, plus the split-triangle prelude and cull path. It preserves the runtime's delayed flag scheduling and inherited status bits. OPMULA, conversions and min/max do not write flags in this runtime; OPMSUB does. The acceptance gate requires full activation coverage and zero mismatches/declines, including saved payloads of drained pipeline slots.

Default-Q hangar `work/model-gpu-parity-572dql4h` passes all 13,800 flag comparisons and the full memory/register/count gates with five identical final visible images. Earlier runs exposed a cross-product prediction error and an incorrectly modeled OPMULA flag write; those failed artifacts are not acceptance. Synthetic tests check pipeline draining, preserved inherited status bits, negative winding history, instruction alignment and the clipping eligibility guard. Four native test programs and 64 public unit/launcher tests passed.

Clipping flag coverage currently requires both sticky Z/S bits already set at the clipping boundary, including every pending valid slot. The visible loop overwrites transient clipping slots; other inherited bits are preserved. Unsaturated clipping explicitly declines, as do empty partially clipped polygons for continuation prediction. These guards must remain before any native bypass; the captures exercised no declines. General clipping flag scheduling and empty-clipping continuation remain gaps. Native lit execution is still disabled pending integration/preflight validation; default play remains unchanged and nothing pushed.

Final flight `work/model-gpu-parity-crpvgt9_` passes all 3,796 activations, including flag slots, with five identical final images. Immediate-Q combined hangar `work/model-gpu-parity-vwjjmi2x` passes all 13,800 lit activations, existing whole/mixed native unlit checks and seven identical final images. Both executables rebuilt; the updated 27 evidence-parser tests pass. Git whitespace check passed. These are capture-specific results, not full-game coverage or a performance claim.


## Opt-in native lit execution (2026-10-15)

`play.cmd -NativeLit` now replaces supported lit activations with the independently validated packet/register/flag calculation and also enables the existing native unlit/model-mesh path. `-ModelMaterials` and `-ModelTextures` remain separately selectable. Default play remains compatibility rendering; inherited native-lit environment settings are cleared unless requested, and capture mode always disables this path.

Every packet, clipped scratch write and winding scratch record is prepared before changing guest state or issuing GIF callbacks. Preflight rejects unsupported continuation state, nonfinite input/constants, overlapping layouts and insufficient instruction allowance. Commit preserves original GIF packet order and restores predicted final VU state. Empty partially clipped polygons and unsaturated clipping flags retain the original VU path. This is native CPU execution of model programs with the existing GPU mesh eligibility, not full native GPU coverage or GIF bypass.

Default-Q hangar `work/model-gpu-parity-0d3c3wdz` executes all 13,800 shadow-verified lit activations / 24,280 packets natively, replacing 8,977,782 original instruction pairs. Flight `work/model-gpu-parity-od_ab7wn` executes all 3,796 lit activations, replacing 1,394,560 instruction pairs. Each compares seven final visible images, all identical. These are scene-specific coverage counts, not FPS measurements or complete gameplay proof.

Synthetic tests verify ordered dual-stream submissions, final memory/continuation state, refusal on a tight instruction budget without any guest mutation/callback, and a nonfinite final input vertex without partial submission. Launcher tests check explicit opt-in, inherited environment cleanup, capture exclusion and flag forwarding. Both executables rebuilt; four renderer tests and 66 public unit/launcher tests passed. Nothing pushed.

Final immediate-Q combined acceptance `work/model-gpu-parity-ofbpe6xy` passes native-lit activation/packet/instruction totals exactly against shadow evidence, alongside existing whole/mixed unlit gates. All nine final visible images match, including native-lit plus native materials. Updated evidence-parser tests and Git whitespace check pass. Broader live gameplay and native GPU coverage remain separate work; no frame-time claim is made.


## Native GPU secondary material pass (2026-10-15)

The model mesh adapter now retains two verified material candidates within one XGKICK. Primary and secondary submissions are matched independently against GS geometry/colour/STQ before consuming each candidate. A suppressed primary can be skipped when the following draw matches the secondary; candidates are cleared at packet boundaries. The secondary vertex-shader mode transforms raw normals into environment-map coordinates and reproduces the normal-dependent colour, while retaining the shared projection/fog/depth handling. Original GS state and fragment fallback remain available.

Hangar `work/model-gpu-parity-r0znfoy2` exercises 10,422 native secondary draws, expanding total model-space draws from 14,444 to 24,866. All seven final visible images remain identical, including native lit execution and native materials. GPU coverage still excludes clipped/unsplit geometry beyond the existing three-vertex adapter; native CPU VU coverage is broader. This draw-count change is not a measured FPS improvement.

Synthetic tests cover two-pass ordering, single consumption, suppressed-primary matching, resetting shader mode for the next primary, and clearing both candidates at packet boundaries. The shader and both executables were rebuilt. The new `--second-pass` replay gate requires positive secondary GPU coverage with identical counts across mesh modes. Nothing pushed; default compatibility rendering is unchanged.

Flight `work/model-gpu-parity-t517z1rs` passes the positive secondary-material gate with 924 added secondary draws, expanding model-space draws from 1,100 to 2,024. All seven final images match. The hangar's secondary counts also match across every mesh mode. Both executables rebuilt after correcting unused-candidate accounting; all four native renderer test programs and 67 public unit/launcher tests pass. Git whitespace check passed. Final-image/capture evidence does not substitute for broader live gameplay validation.

## AC4 terrain preparation identified and verified (2026-10-15)

Compared the local AC5 `rn_vp_terrain.c` with AC4's private captured programs. AC5 builds template-based patches with positive 80-unit heights and a clip-W transition. AC4's recognized images `ba581f12988de18b` / `6632fc9c7a80a952` instead decode packed records into 1024-unit X/Z coordinates, -80-unit heights plus a patch origin, two UV sets and float colours. Their executed terrain code matches through the 0x1050 continuation; whole-image signatures differ in leftover code beyond that path. No AC5 guest addresses/templates are reused.

The independent AC4 preparation kernel reproduces integer conversion, UV scale/offset and the five even-vertex distance morphs. Morph factors use squared XYZ distance, threshold and scale, with clamping and separate multiply/add rounding. Neighbor pairs are (1,5), (1,3), (3,5), (5,7), (7,1). The observer compares every prepared input record against original VU memory at the first submission and never changes guest memory or claims draws.

Visible flight `work/model-gpu-parity-2ogibh50` matches 1,906 patches / 10,564 vertices, zero mismatches or declines: 490 ten-vertex mode-2 morphed patches and 1,416 four-vertex mode-4 patches. Existing model native/secondary-material gates pass and all seven final images match. The `--terrain-prep` gate requires complete positive comparison coverage, both morphed/non-morphed modes and zero mismatch/decline counts.

Synthetic tests cover signed coordinates/height, both UV sets, origin offsets, near/far morph limits, unchanged odd vertices and atomic failure. Both executables rebuilt; four renderer tests and 68 public unit/launcher tests pass. This verifies preparation only: terrain projection, packet/material passes, clipping, GPU adapter and eventual VU/GIF bypass remain to implement. Terrain still renders through the original path; no performance claim and nothing pushed. Private microcode/listings remain ignored local artifacts.

## Terrain projection and two-stream packets (2026-10-15)

The terrain reference now composes the complete ordinary packet from decoded/morphed records: three setup records, the primary tag and its STQ/colour/XYZF vertices, followed by the secondary tag and its distinct STQ/colour/XYZF vertices. The secondary RGB is 128 and alpha is modulated by fog using the VU's exact 0x3b808080 constant; substituting rounded `1/255` changes boundary results. Both streams are produced even when the primary EOP prevents submission of the second. Output bank selection is independently checked.

Initial flight evidence `work/model-gpu-parity-mwntxy2x` matches all 1,906 packets and banks with zero mismatch/decline counts. The `--terrain-prep` acceptance gate now requires complete packet coverage as well as prepared-record comparison and both patch modes. Synthetic tests cover setup/tag order, chaining/EOP, dual UV streams, secondary alpha's 254-versus-255 boundary, fog packing and failure without partial output. Mode-1 clipped-fan terrain is explicitly rejected by the packet builder; the visible-flight capture covers mode 2 and 4.

This is independent packet prediction under original execution, not yet a native terrain draw or VU bypass. Next is the GPU patch adapter using these validated positions/material streams, followed by clipped fan coverage and continuation-state verification for native VU execution. Default rendering remains unchanged; nothing pushed.

Final exact-alpha build `work/model-gpu-parity-nlj4r5ph` passes the complete terrain preparation/packet gate for all 1,906 patches and 10,564 vertices. Existing lit-native and secondary-GPU gates pass; all seven final visible images match. Both executables rebuilt, four renderer test programs and 69 public unit/launcher tests pass. Git whitespace check passed. These remain capture-specific checks, not full-game terrain coverage.

### Terrain topology preparation (2026-10-15)

Added an atomic native terrain topology builder for ordinary four-, nine- and ten-vertex strips/fans. It preserves GS queue order, material-pass identity, EOP termination and ADC suppression. Only the three active GIF register slots are checked: real terrain tags carry nonzero unused register bits. Unsupported tags decline without publishing partial triangles. Synthetic checks cover fan versus strip order, both materials, suppressed vertices, ignored register bits, maximum sixteen-triangle output and invalid second-pass atomicity.

Private flight evidence: `work/model-gpu-parity-kwhtaah9`, eight fields. All 1,906 independently prepared packets remain exact; topology preparation yields 13,504 triangle candidates with zero declines. The strengthened `--terrain-prep` gate requires topology coverage of every prepared patch and positive triangles. All seven comparison-mode final PPMs remain byte-identical (SHA256 `66357448cd87cdc488b5c652ac35cbb896bd2493b8e871482e0dd4df34f295cc`). This is topology preparation, not measured GPU terrain draw coverage or every-frame visual validation. Original terrain VU/GIF rendering remains active. Next: upload the decoded terrain records and submit these triangles through a dedicated GPU projection/material path, then verify positive native draw counts and image parity.

### Experimental native terrain GPU submission (2026-10-15)

Connected ordinary terrain topology to a bounded, XGKICK-scoped native mesh candidate queue. Raw decoded world positions, floating source colours and separate material UVs are uploaded; shader program 7 performs projection, fixed-point GS packing, fog and secondary alpha. Exact per-triangle attribute matching guards claims, with GS fallback on unsupported state or arena allocation failure. This still executes original terrain VU and GIF; it is not terrain execution bypass.

Use the private capture test with `--terrain-mesh` (implies `--terrain-prep`) to exercise it. Normal launch/capture helpers clear `PS2_AC4_TERRAIN_MESH`; no public gameplay switch is exposed pending acceptance. The evidence gate requires positive primary and secondary GPU counts across tested modes, and retains strict byte-identical image comparison.

Flight evidence `work/model-gpu-parity-11loa9ev`: 6,752 primary plus 6,752 secondary GPU draws in each experimental mode; no attribute/state/capacity fallbacks. Image acceptance **fails**: two final-image pixels at x=748,y=1990..1991 differ by at most one colour level. No tolerance waiver was added. Removing reciprocal refinement increased this to 50 pixels (`work/model-gpu-parity-i67mgas1`); that experiment was reverted. Investigate remaining projection/STQ/alpha rounding before claiming parity or promoting this path. Tests cover decoded record layout, independent UV streams, queue exhaustion/reset, capacity fallback and maximum queue size.

### Exact native terrain fog normalization (2026-10-15)

Resolved the two-pixel mismatch reported above. Controlled diagnostic uploads localized it to normalized fog: supplying CPU STQ, RGBA or position alone left the same two green-channel differences; supplying only CPU normalized fog removed them. Supplying the CPU fog byte but normalizing in the shader reproduced them. Floating residual correction did not resolve this GPU result. All diagnostic CPU-output uploads were removed.

`mesh.vert` now constructs correctly rounded fog/255 using bounded integer significand arithmetic and bit conversion, with explicit endpoints. Native GPU projection, fog calculation/quantization, both UV streams and material alpha remain active. The renderer test runner extracts the actual shader helper, adapts only GLSL intrinsic/cast syntax for a C harness, and compares all 256 bytes bit-for-bit against double division rounded to float; no duplicate algorithm is used as the oracle.

Final flight evidence: `work/model-gpu-parity-524og8g8`, eight fields, `--terrain-mesh --materials --native-lit --second-pass`. All seven mode final PPMs are byte-identical; each terrain-enabled mode records 6,752 primary and 6,752 secondary terrain draws. Native model coverage remains 2,024 draws (924 secondary). Final hangar regression: `work/model-gpu-parity-77gqyob_`, seven byte-identical final images, 24,866 model draws (10,422 secondary). All five synthetic renderer executables and 71 public tests pass. These remain final-capture-image checks, not every-frame or full-game acceptance, and establish no FPS improvement.

Terrain stays developer opt-in through the capture comparator until broader scene acceptance and clipped-path support. Default launchers remain unchanged. Next: broaden terrain coverage and implement validated original-terrain VU/GIF bypass; sky/clouds and native-resolution HUD/radar/radio remain incomplete.

### Broader island coverage and clipped-terrain fixture (2026-10-15)

Added `--terrain-only` to the capture comparator. This explicitly substitutes mandatory positive terrain GPU evidence for the unrelated model-draw gate, and implies terrain mesh plus packet/topology checks. Strict image equality and both terrain material-pass draw counts remain required. Version-2 DMA capture headers are now required before launching replay; GS-only captures cannot establish native geometry coverage. Texture replacement sampling is explicitly disabled in these geometry comparisons.

`airbase_look_down.gscap` is GS-only and cannot test this path. `ground_sun.gscap` is DMA but has no supported terrain activations in this view, so it fails positive terrain coverage. Neither counts as a terrain acceptance pass.

Island turn, eight fields: `work/model-gpu-parity-o4d7lvmj`. Visually inspected final image shows the banked island coastline and ocean. All four final PPMs match, SHA256 `0bd7a3a8cbfa0f93fe6f8253cd6aaa87bd61ebc622cd664da60c5cf8b8ea7265`. 4,070 prepared patches / 24,482 vertices match the VU; 16,342 primary and 16,342 secondary native terrain draws. Native fragment material coverage is zero for these captured states; GS fragment fallback is retained, and `--materials` correctly fails its coverage gate.

Longer island turn, 120 fields: `work/model-gpu-parity-chd0pa09`. **Full terrain acceptance fails**, exposing mode-1 clipping. 60,872 recognized activations include 129 mode-1 patches; 60,750 reach the first-kick observer, seven reject ordinary packet generation, and 122 have no observed packet. The latter must be verified as culling/empty outputs rather than assumed. 60,743 ordinary packets are exact and generate 242,488 primary plus 242,488 secondary GPU draws. All four final PPM hashes nevertheless match (`1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`) through retained fallback. This does not waive missing clipping/continuation coverage or prove every frame. Next use this capture to implement/validate clipped fans and empty-output continuation before terrain VU bypass.

All five synthetic renderer executables and 72 public tests pass. Default gameplay remains unchanged; no publish.

### Mode-1 terrain fan rejection verified (2026-10-15)

Implemented an independent common-plane culling predictor from decoded positions and the AC4 clip matrix. Mode 1 walks a fan anchored at vertex zero, with homogeneous XYZ comparisons against absolute W; all three vertices must share an outside plane for trivial triangle rejection. Invalid nonfinite inputs decline atomically. No native execution is enabled by this observer.

The 120-field island replay (`work/terrain-culling-probe/run.log`) now accounts for all 129 mode-1 activations: 122 independently predicted empty, 122 observed empty, zero prediction mismatches, seven remaining partial cases, zero invalid inputs, and 129 end callbacks. This resolves the earlier uncertainty about activations without packets: for this capture all 122 are intentional common-plane rejection. The remaining seven emit geometry and need clipped polygon packets/continuation. Original execution and all GS fallback remain. Full terrain acceptance still fails for those seven unsupported clipped outputs; the culling result does not relax that gate.

Synthetic checks cover six planes, exact boundary equality, fan anchoring, negative W, invalid-input atomicity, owner isolation and exactly-once observation. The parser requires positive, complete, internally consistent culling evidence and rejects missing/mismatched reports. Five renderer executables pass; public-suite results recorded below. Next implement polygon clipping and packet sequencing for the seven partial cases, then validate full guest state before bypass.
`tools/run_tests.py --suite all`: 73 tests passed. The observer probe final image hash remains identical to the preceding 120-field baseline: `1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`.

### Mode-1 clipped terrain packet plan (2026-10-15)

Implemented six-plane clipping of decoded terrain fan triangles with AC4's strict edge classification, inside-to-outside interpolation ordering, per-operation float rounding, and clip-coordinate recomputation. Both UV records, source colours and positions are interpolated. Completely inside triangles preserve input order; common-plane rejects emit no packet. Packet plans preserve both material streams and alternate output banks only for emitted polygons. The packet builder now accepts polygon vertex counts 3..10; ordinary mode-1 generation remains guarded.

The 120-field island probe (`work/terrain-culling-probe/run.log`) independently builds all 129 mode-1 plans, including the 122 empty activations and seven partial activations. All **22 emitted packets** match the original byte-for-byte, with zero declined plans and zero packet/order/bank-sequence mismatches. Final image remains `1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`. Original terrain execution remains authoritative; this does not yet compare all clipping scratch memory or continuation registers/scalars/flags/cycles, and does not enable clipped GPU draws or terrain bypass.

Synthetic checks cover clipping a triangle to a four-vertex polygon, interpolating both UV streams and constant colours, preserving an entirely inside triangle, emitting nothing for a common-plane reject, and refusing nonfinite attributes atomically. Evidence parsing rejects incomplete plans, zero packets, any mismatched packet/order, or insufficient culling evidence. Next: submit these independently generated clipped polygons through the native mesh adapter and then extend continuation-state validation toward bypass.

### Clipped terrain GPU path and complete capture accounting (2026-10-15)

Connected the independent clipped polygon records to the bounded terrain GPU queue. Each packet is checked against original VU output and its expected bank before queueing. Both material streams retain native projection, fog and colour/UV handling. Clipped draws have independent per-stream counters. Ordinary and clipped paths share triangle preparation; fan topology accepts polygon counts 3..10.

Preparation comparison now also runs at activation end for culled terrain, so previously empty activations have their decoded input checked rather than disappearing from coverage. The acceptance gate reconciles ordinary packet counts with mode-1 plans, requires complete clipped topology and positive clipped GPU coverage when mode-1 is present, and still rejects packet mismatches, unsupported topology or missing sequences. No unsupported cases were silently waived.

120-field island-turn acceptance: `work/model-gpu-parity-ilhnvrk_`. All 60,872 activations / 365,264 decoded vertices compare exactly. 129 mode-1 plans include 122 culled activations and 22 packets from seven partial activations. Each GPU mode submits 242,525 primary and 242,525 secondary terrain triangles, including **37 clipped triangles per stream**. All four final images are identical, SHA256 `1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`. Compatibility material fallback remains active for these island states; native material acceptance is separately established only for the earlier flight subset. This tests all observed packets but only the final image, not every intermediate rendered frame.

Five renderer executables and 76 public tests pass, including polygon counts 3..8, clipped draw counter isolation and negative combined-coverage cases. Terrain remains developer opt-in. Native terrain VU/GIF bypass is still incomplete: full scratch/continuation registers, scalar/flag pipelines and instruction counts must be predicted and checked first.

### Whole-memory terrain continuation prediction (2026-10-15)

Added an entry-snapshot-based prediction of all 16 KiB of VU memory for ordinary terrain and common-plane-culled mode-1 fans. It independently applies decoded/morphed input writes and both ordinary packet streams, preserving every other memory word. Culled fans predict only input preparation, with no output-bank writes. This observer runs in plan mode, not ordinary gameplay. Partial clipping remains explicitly excluded because its low-memory polygon scratch writes require additional modeling.

120-field island probe: `work/terrain-culling-probe/run.log`, **60,865 built / 60,865 ended / zero whole-memory mismatches / seven explicitly declined partial-clipping activations**. The existing 129 mode-1 plans and 22 clipped packets remain exact, and the final image hash is unchanged. The acceptance parser checks that the only declines equal the independently observed partial-clipping activation count; it rejects missing end callbacks, any memory mismatch, unaccounted exclusions or zero coverage. This is ordinary/culled memory-state evidence, not proof of complete continuation state or terrain execution bypass.

Synthetic tests cover wrapped TOP-relative writes, untouched memory, entry snapshot immutability, culled output-bank preservation and atomic partial-clipping rejection. Five renderer executables and 77 public tests pass. Next extend memory prediction to clipped polygon scratch and then predict integer/vector/scalar/flag/cycle continuation.

### Complete clipped terrain scratch-memory prediction (2026-10-15)

Extended the clipping kernel with optional private scratch-state reconstruction: initial five-qword vertex records, alternating scratch banks, per-plane writes including surviving contents after empty results, compacted output records, and the separate direct-inside path. The full memory builder applies all fan triangles sequentially and alternates packet banks only for emitted polygons. It publishes the result only when the entire activation succeeds; unsafe overlapping scratch/input layouts and excess scratch capacity remain guarded. No original VU output is used to construct the prediction.

The 120-field island observer now builds and compares **all 60,872 full 16-KiB memory results, zero mismatches, zero exclusions**, including the seven partial-clipping activations previously declined. Packet and culling evidence remain unchanged. The acceptance gate now requires full memory coverage and rejects any exclusion; its earlier seven-case allowance was removed.

Synthetic checks verify the final strided and compacted scratch layouts, preservation outside scratch, wrapped ordinary writes, and atomic rejection after an earlier fan triangle has already produced temporary output. Five renderer executables and 77 public tests pass. This completes captured memory-write prediction, not native execution: integer/vector/scalar/flag state and cycle counts still need independent prediction before terrain VU/GIF bypass.
Combined 120-field GPU/observer acceptance: `work/model-gpu-parity-va0evign`. All four final images match (`1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`); 242,525 primary and secondary terrain draws each, including 37 clipped draws per stream. Complete packet/topology/culling/memory gates pass.

### Terrain integer continuation, ordinary and culled paths (2026-10-15)

Added independent predictions of all sixteen VU integer registers at ordinary packet submission and at ordinary/fully-culled activation exit, preserving entry values for untouched registers. The culled path includes the next-plane FCOR in the taken branch delay slot: a successful rejection branch does not necessarily leave VI1 equal to one. An initial observer mismatch exposed this detail; the corrected predictor and six-plane synthetic checks now cover it.

120-field island probe `work/terrain-culling-probe/run.log`: **60,865 exact exits, 60,743 exact packet-submission register sets, zero sequence mismatches, seven explicit partial-clipping exclusions**. Full memory still covers all 60,872 activations without exclusions. Final image remains unchanged. This is not complete VU state: clipped integer continuation and vector/scalar/flag/cycle prediction remain required before native terrain execution.

Synthetic checks include preserved registers, TOP-relative pointer arithmetic, 16-bit material flag loading, kick-versus-exit loop counters, every common clipping plane and atomic rejection of unsupported partial fans. Acceptance parsing rejects missed exits/kicks, any register/order mismatch, unaccounted declines and absent reports. Five renderer test executables pass.

### Complete captured terrain integer continuation (2026-10-15)

Extended the integer plan across every mode-1 fan triangle, preserving registers from earlier clipped/inside/culled triangles and recording each packet submission. Clipping now reports independent plane-loop control metadata alongside its geometry; the register predictor derives pointer and counter continuation from it, including empty-plane exit state, six-plane completion, retained VI10/VI11 values, branch-delay condition results and alternating output banks. Actual VU registers are compared only after prediction, never used to construct it.

120-field island acceptance `work/model-gpu-parity-7_ptma41`: **60,872 exact integer exits and 60,765 exact packet-submission register sets, zero exclusions and zero end/kick/order mismatches**. Full-memory, culling, clipped packet and topology checks also pass. Four final images remain identical (`1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`); native GPU counts remain 242,525 per material stream, including 37 clipped draws per stream.

Synthetic tests cover an eight-packet fan, bank alternation, per-triangle loop counters, clipped-to-inside register inheritance and atomic failure late in a fan. Acceptance parsing now requires all activations and all ordinary plus clipped packet submissions, with no partial-clipping allowance. All five renderer executables and 78 public tests pass. Vector/scalar/flag-pipeline state and instruction timing remain unproven; terrain VU/GIF bypass is still disabled. Next validate those remaining continuation components.

### Terrain scalar continuation (2026-10-15)

Added independent Q/P, pending Q/P, I, R and 24-bit CLIP predictions at every terrain packet submission and exit. The predictor drains bounded inherited Q/P deadlines, tracks each fan triangle's three CLIP operations, incorporates clipping intersection divides (including polygons that become empty), preserves values through common-plane culling and replaces Q/I on emitted polygons. Q-latency mode controls whether a new divide also replaces the pending-Q payload. Unusually long inherited deadlines remain an explicit guard pending exact timing proof. Arithmetic MAC/status flag pipelines are separate and remain unvalidated.

120-field island evidence: `work/model-gpu-parity-mvl7qq8d` (normal Q latency) and `work/terrain-scalar-qlat0/run.log` (disabled Q latency). Both match **60,872 exits and 60,765 packet submissions**, with zero scalar mismatches, sequence mismatches or exclusions. Normal-latency combined GPU comparison passes all current preparation/packet/topology/culling/memory/integer/scalar gates; four final images remain identical (`1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`).

Synthetic checks cover both latency settings, inherited Q/P commits, unchanged pending payload in immediate mode, CLIP-history rollover, preserved R, atomic long-deadline rejection, and a polygon that disappears after clipping yet leaves its final reciprocal and I value. Five renderer executables and 79 public tests pass. Terrain still executes its original VU/GIF program; vector/ACC, arithmetic flag-pipeline and instruction-count prediction are next before bypass.


### Complete captured terrain instruction accounting (2026-10-15)

Added independent executed instruction-pair predictions for ordinary patches and all fan paths: common-plane rejection, direct-inside triangles, clipping edge transitions, per-plane scratch-bank changes, early-empty polygons, scratch compaction, both material-chain branches, alternating output banks and entry trampolines. Counts include branch delay slots; WAITQ counts as an executed instruction, not elapsed host time. The geometry kernel supplies edge/plane schedule counts without consulting observed VU execution counts.

Combined 120-field island acceptance: `work/model-gpu-parity-p952g2u0`. **60,872 activations, 25,632,736 predicted and observed instruction pairs, zero count mismatches and zero exclusions.** The disabled-Q-latency probe (`work/terrain-scalar-qlat0/run.log`) matches the same totals. Preparation, packets, topology, full memory, integer and scalar gates remain exact. All four final images match SHA256 `1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`; primary/secondary GPU terrain draws remain 242,525 each, including 37 clipped draws each. This compares final captured images, not every frame or gameplay performance.

Five renderer executables and 80 public tests pass. Synthetic count cases cover both entry points, tag chaining, morph modes, all six common-plane branches, direct-inside fans with alternating packet banks, atomic failure late in a fan, and exactly-once end accounting. Acceptance rejects missing, incomplete, mismatched or excluded instruction evidence. Terrain VU/GIF bypass remains disabled until vector/ACC and arithmetic flag-pipeline continuation are independently reconstructed and validated. Native sky/clouds and window-resolution HUD/radar/radio are still outstanding; this does not establish AC5 parity.


### Terrain vector/ACC prediction for ordinary and culled paths (2026-10-15)

Added independent predictions of all 32 vector registers and the accumulator, preserving untouched entry registers. Ordinary paths reconstruct distance-morph residual vectors, projected packet registers, final matrix/constant registers, and raw one-vertex lookahead loads. Fully culled fans retain the final triangle's three clip transforms and decoded attributes, plus the pre-W accumulator. Partially clipped fans remain an explicit exclusion until their intermediate clipping vectors are reconstructed; no terrain execution bypass is enabled.

Combined island acceptance `work/model-gpu-parity-15pj5r_n`: **60,865 exact vector/ACC exits, 60,743 exact packet-submission states, zero end/kick mismatches, seven partial-clipping exclusions**. All 60,872 activations still pass complete memory/integer/scalar/instruction checks. All four final images remain identical (`1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`), with unchanged native GPU draw coverage. This is captured final-image/state evidence, not all-frame/full-game acceptance or a measured performance gain.

Five renderer executables and 81 public tests pass. Synthetic vector cases exercise preserved registers, final packet registers, lookahead load ordering, accumulator partial sums, final culled-fan attribute selection and atomic rejection of a partly visible fan. The acceptance parser requires every ordinary/fully culled exit and ordinary packet submission and permits only the independently counted partial-clipping exclusions. Next reconstruct clipped vector/ACC state, then arithmetic MAC/status pipelines before enabling native terrain execution.


### Complete captured terrain vector/ACC continuation (2026-10-15)

Extended the clipping kernel with optional native vector-state tracing: initial triangle attributes and clip transforms, per-edge distances, intersection factors and attributes, accumulator partial sums, shifted edge records, scratch lookahead loads, plane-direction changes and compacted output records. The fan plan carries state across clipped, inside and culled triangles and records every emitted packet state. Projection register reconstruction is shared with ordinary terrain. Scratch lookahead follows final-edge writes, including the boundary that can reach the other scratch bank. Predictions use private entry-memory copies and publish only after the entire fan succeeds.

Combined 120-field island acceptance: `work/model-gpu-parity-iqsi4f20`. **60,872 exact vector/ACC exits and 60,765 exact packet-submission states, zero end/kick mismatches and zero exclusions.** All existing memory/integer/scalar/instruction gates still pass. Four final images match SHA256 `1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`, with 242,525 primary and secondary terrain draws each, including 37 clipped draws each. Final-image equality is not every-frame or full-game acceptance.

Five renderer executables and 81 public tests pass. Added synthetic checks for clipped-to-inside temporary-register inheritance, final per-packet state, atomic failure after an earlier triangle emitted geometry, and a polygon disappearing through clipping while retaining edge/plane vector state. End validation also rejects missing or excess packet callbacks. The acceptance parser now requires vector coverage identical to the complete integer coverage; the former partial-clipping exclusion allowance is removed. Terrain still runs the original VU/GIF execution. Arithmetic MAC/status/pipeline reconstruction and verification remain before enabling native terrain bypass; broader AC5 parity work remains outstanding.


### Terrain arithmetic flags, ordinary and culled paths (2026-10-15)

Added native arithmetic scheduling for input decoding, mode-2 distance morphing, projection/material loops and fully culled fan transforms. It reconstructs MAC/status and all four flag-pipeline payloads/validity bits from inherited entry state, without requiring sticky flags to be pre-saturated. Conversion and min/max instructions follow the runtime's non-flag-writing semantics. Terrain's instruction count includes E and its delay pair, so its final slot index is `(total-1)&3`; an initial replay discrepancy exposed and corrected the accidental extra two-pair offset.

Combined 120-field island acceptance `work/model-gpu-parity-0om3u3sq`: **60,865 exact arithmetic flag/pipeline exits, zero mismatches, seven partially clipped activations explicitly excluded**. All 60,872 memory/integer/scalar/vector/instruction states retain their complete coverage. All four final images match SHA256 `1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`; GPU draw coverage is unchanged. These are state and final-image checks, not host performance measurements.

Five renderer executables and 82 public tests pass. Synthetic tests cover initially unsaturated sticky flags, retained unrelated status bits, final pipeline payloads, drain validity, entry-trampoline slot rotation, culled counts and atomic rejection of partial clipping. Acceptance permits only the independently counted partially clipped exclusions and rejects missing reports, incomplete exits or mismatches. Next extend flag scheduling to clipping edges and emitted polygons; terrain VU/GIF bypass remains disabled until that remaining state is validated. Full AC5 parity remains incomplete.


### Complete captured terrain arithmetic flag continuation (2026-10-15)

Extended arithmetic scheduling through clipping transforms, axis selection, edge classification, both intersection directions, interpolation, projection, and plane-direction changes. The clipping kernel optionally emits flag events using the same independently derived instruction-pair offsets as its control schedule; the main plan carries inherited pipeline state across all fan triangles. Projection scheduling is shared with ordinary patches. Internal schedule totals must agree with the independent instruction-count plan. No pre-saturated sticky-flag guard is required.

Combined 120-field island acceptance `work/model-gpu-parity-7hkir290`: **60,872 exact MAC/status/pipeline exits, zero mismatches and zero exclusions**. Full memory, integer, scalar, vector/ACC and instruction-count checks remain exact. Four final images match SHA256 `1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`; terrain GPU coverage remains 242,525 draws per stream, including 37 clipped draws per stream. A separate Q-latency-disabled replay (`work/terrain-scalar-qlat0/run.log`) also passes all flag/vector/scalar checks. These checks cover the capture and final images, not full-game acceptance or measured FPS.

Five renderer executables and 82 public tests pass. Added clipped-path schedule/count consistency, entry-trampoline pipeline rotation, empty-clipped-polygon drain state and atomic late failure checks. The acceptance parser now requires flag coverage for every prepared activation and rejects any exclusion; the prior seven-case allowance is removed. Next assemble a guarded native terrain execution plan and verify actual bypass against the original run, including continuation fields not yet covered by these component observers. Original terrain VU/GIF execution remains active; full AC5 parity is still incomplete.


### Atomic full terrain continuation plan (2026-10-15)

Assembled the independently validated memory, integer, vector/ACC, scalar, flag and instruction components into a single atomic terrain execution plan. It predicts the end PC (`0x1050`), stopped state, preserved control fields, pending scalar payloads and memory metadata as well as arithmetic registers. Packet counts from integer/vector/scalar plans and instruction totals from the count/flag plans must agree. No guest memory or register mutation occurs during construction, and a failed component leaves the caller's output unchanged.

Combined island acceptance `work/model-gpu-parity-7n6tlmq9`: **60,872 full continuation states, 60,765 packets and 25,632,736 instruction pairs, zero mismatches and zero declines**. This checks assembled state against original execution, beyond the separate component reports. Four final images remain identical (`1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`), with unchanged native GPU draw counts. It does not yet exercise actual terrain VU/GIF bypass or prove full-game acceptance.

Five renderer executables and 83 public tests pass. Synthetic plan tests verify input memory/register immutability, preserved control/pointer fields, final PC/running state and unchanged output on late invalid geometry. The combined acceptance gate requires complete component evidence plus equal activation, packet and instruction totals; missing component reports and any full-state mismatch fail. Next add ordered native packet submission and an opt-in execution commit, then compare actual native execution against this original-run evidence. Working defaults remain unchanged; no source was pushed.


### Ordered terrain submission plan (2026-10-15)

Added per-packet sparse memory commits to the atomic terrain plan. Each submission stores changed qwords and its output-bank address, including input preparation, clipping scratch and packet writes since the preceding submission. Construction validates the final cumulative memory against the independently built final snapshot and requires the same packet total as the register plans. A bounded 256-qword commit capacity rejects atomically instead of truncating writes. Culled trailing triangles still contribute to the final continuation snapshot.

Combined island acceptance `work/model-gpu-parity-4tzt0839`: **60,765 packet boundaries, zero complete intermediate-memory mismatches and zero address/order mismatches**. All 60,872 full continuation checks remain exact. All four final images match SHA256 `1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`, with unchanged native GPU draw coverage. No actual terrain execution bypass is enabled yet.

Five renderer executables and 83 public tests pass. Added exact-capacity, overflow atomicity, unchanged-qword elision and wrapped packet-address tests. Full continuation acceptance now also requires complete ordered submission evidence, rejecting missing, incomplete, memory-mismatched or out-of-order packets. Next connect these validated commits to an opt-in native terrain execution path and test actual execution against the original replay. This is preparatory correctness evidence, not an FPS improvement or AC5 parity completion.


### Experimental native terrain VU execution (2026-10-15)

Connected the atomic plan and ordered sparse writes to an opt-in execution commit, `PS2_AC4_TERRAIN_NATIVE=1`. It enables terrain GPU submission and bypasses the recognized terrain VU programs only after all planning succeeds and the instruction guard permits the complete activation. Original GIF/GS callbacks still run in packet order; final memory and full continuation state are committed afterward. Missing callback, insufficient instruction budget, wrong owner, stale plan, disabled native mode or observer mode reject before guest-visible mutation. Unrecognized programs continue through their existing paths. Enabling only terrain bypass does not enable native model bypass implicitly. Each new activation clears the prior terrain plan, including unrecognized programs and fallback cases.

Normal play/capture launchers clear the experimental switch. This is a developer replay option, not a default promotion. Use `python tools/test_ac4_model_gpu.py <capture> --terrain-only --native-terrain --fields 120` for island-only evidence, or combine `--native-terrain --native-lit --materials --second-pass` on the third-person flight fixture. The acceptance tool requires native activation/packet/instruction totals to equal complete original-run continuation evidence and retains final-image and GPU coverage checks.

Actual native-run evidence:

- Island, 120 fields: `work/model-gpu-parity-xqv_2rpi`; **60,872 native activations, 60,765 packets, 25,632,736 original VU instruction pairs replaced**. All five final images match `1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`; terrain GPU coverage remains 242,525 draws per stream, including 37 clipped draws each.
- Third-person flight: `work/model-gpu-parity-tfsfhdy9`; **1,906 native terrain activations/packets, 744,376 original VU instruction pairs replaced**. All eight final images match `66357448cd87cdc488b5c652ac35cbb896bd2493b8e871482e0dd4df34f295cc`; terrain GPU coverage is 6,752 draws per stream, alongside unchanged model/secondary coverage. Native model and terrain modes were tested separately in this comparison; simultaneous native execution needs its own combined mode.

Five renderer executables and 83 public tests pass. Native commit tests cover preflight rejection without callbacks or mutation, exact-budget success, ordered memory callbacks, complete final state, double-commit rejection and stale-plan clearing. Launch environment tests ensure inherited terrain-native settings do not leak into ordinary play. This does not prove every-frame/full-game acceptance or a performance gain: current native planning deliberately repeats validated component construction and still needs consolidation/profiling. Remaining scope includes GIF bypass, broader scene/material/texture acceptance, sky/clouds, and native-resolution HUD/radar/radio. No source was pushed.


### Combined native execution and variable mesh GIF payloads (2026-10-15)

Compared the local AC5 native submission path (`runtime/src/rn/rn_vp.c`, `pkt_submit_range`), which drives GS register/native-vertex interfaces from its program output, with AC4's existing triangle payload specialization. Extended AC4's specialized packed ST/RGBA/XYZF decoder from exactly three vertices to supported three-through-ten-vertex mesh payloads. It preserves register writes, ADC, Q latch and GIF continuation while avoiding generic payload decoding. Tags and setup still use the generic parser; this is partial GIF parser bypass, not full direct mesh submission. Unsupported formats, partial payloads and oversized counts retain fallback.

Synthetic generic-versus-specialized comparisons cover 3/4/9/10 vertices, both EOP states, wrapped memory, alternating ADC flags, Q/GIF state and atomic insufficient-payload rejection. Five renderer executables and 83 public tests pass.

Added a simultaneous `native-combined` replay mode when both `--native-terrain` and `--native-lit` are requested. It validates each program family's native totals against the original full observer evidence. Third-person capture `work/model-gpu-parity-4jeqy2j5`: **1,906 terrain activations/packets and 744,376 terrain instruction pairs**, plus **3,796 lit-model activations/packets and 1,394,560 model instruction pairs**, execute natively together. All nine final images match `66357448cd87cdc488b5c652ac35cbb896bd2493b8e871482e0dd4df34f295cc`; primary/secondary GPU coverage remains unchanged. The combined mode uses the usual GS fragment path; simultaneous native-material mode remains a separate extension.

Island 120-field comparison `work/model-gpu-parity-550iyvql`: complete terrain state/submission gates pass, native execution still covers all 60,872 activations, and all five final images match `1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`. The legacy `ac4 model GIF` counter now includes terrain-sized payloads: 645,378 payload qwords bypass generic decoding in fast/native island runs. This is a work counter, not an FPS measurement. Defaults are unchanged; complete GIF/tag/setup bypass, sky/clouds, HUD/radar/radio and broader acceptance remain outstanding.


### Whole recognized mesh GIF packet bypass (2026-10-15)

Added atomic whole-packet preflight for bounded packed A+D setup and ST/RGBA/XYZF geometry streams. After validating every tag through EOP, the specialized path applies PRE, setup register writes, geometry register writes, Q/ADC handling and final GIF continuation directly, with equivalent tag counters. Unsupported layouts, incomplete payloads, missing EOP, excess tag count or packets without geometry reject before GS writes. Observer/audit modes retain generic decoding. The GS renderer, texture/state handling and unsupported packet fallback remain; this bypasses generic GIF parsing, not GS itself.

Synthetic generic-versus-specialized tests compare register order/values, complete GIF/Q state and tag counters for a wrapped two-stream packet with setup and PRE. An unsupported final tag rejects before earlier valid setup writes. Existing vertex-payload tests still cover 3/4/9/10 vertices and EOP/wrapping. Five renderer executables and 84 public tests pass. Replay acceptance now requires positive whole-packet coverage and equal packet/qword totals across fast/native modes; the obsolete payload-only counter requirement was replaced rather than allowing zero work.

- Flight: `work/model-gpu-parity-6ih_pril`, **3,054 packets / 98,354 qwords** bypass generic parsing in each fast/native mode. All nine final images match `66357448cd87cdc488b5c652ac35cbb896bd2493b8e871482e0dd4df34f295cc`, including combined native terrain and lit execution.
- Island: `work/model-gpu-parity-omjo5u0g`, **60,765 packets / 2,488,155 qwords** bypass generic parsing. Complete terrain continuation/submission/native totals remain exact and all five final images match `1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`. The new whole-packet coverage gate was also applied to retained fast/native logs.

These are captured-state/final-image and work-coverage results, not every-frame/full-game or FPS evidence. Broader model/material/texture coverage, sky/cloud native programs, HUD/radar/radio and gameplay acceptance remain. Defaults remain unchanged and no source was pushed.


### AC4 sky identification and native band encoder (2026-10-15)

The sky/cloud addresses in inherited `rn_screen.c`, `rn_intent.c` and the IDA symbol export list are AC5 addresses, not verified AC4 symbols. AC4 deliberately returns before installing those hooks. Do not use those labels as AC4 coverage evidence. The unidentified VU image `a27b926f29d60f74` contains extrusion/face tests suggestive of shadow geometry; `896ee6651d46e334` contains normal transforms and lighting. Neither is established as sky/cloud coverage.

AC4's own generated EE code identifies the dome band producer at `0x00111138`, haze producer at `0x00111310`, shared projected-band packet writer at `0x00111050`, and dome orchestration at `0x001115C0`. The dome loops over 29 rings and 32 segments, projecting through `0x00110F38` (ring), `0x00110FE0` (apex), and `0x0010FFB8` (point). Unlike AC5's fixed ring/color addresses, AC4 consumes an object argument: matrix at +0x40, projection bounds at +0xc0, segment vectors at +0xd0, ring pairs at +0x2e0, and colours at +0x3c4. The observed caller uses scratchpad base 0x70000000 and alternating projected-row scratch at +0x4a0. These are static identifications, not yet live hook verification. Haze depth differs from AC5 and must be adapted independently.

A read-only four-field flight replay (`work/sky-stream-probe/run.log`) matches recorded GS fidelity for every replayed field. Field 2's stream dump contains a 429-qword VIF DIRECT pass before VU geometry, with untextured interpolated triangle strips, RGBA/XYZ3/XYZ2 writes and disabled depth writes. This is consistent with the identified dome producer; producer-to-packet association still requires a live hook/capture. No renderer bypass was enabled for that replay.

Added a standalone native projected-band encoder in `runtime/include/rn_ac4_sky.h`. It preserves visible adjacent segments, four-corner clip rejection, closure bit 32, colour/depth payloads, REGLIST ordering and empty-band output. Insufficient capacity rejects without changing output/count. It does not project vertices, replace guest execution, or draw meshes yet. Public synthetic tests cover packet values/order, maximum size, wrap closure, empty/invalid inputs, capacity atomicity and 1,000 randomized visibility/clipping cases.

A private independent oracle compiled the original generated AC4 band writer and compared **2,000 cases byte-for-byte and by returned end pointer**, all exact (`work/sky-stream-probe/guest-band-oracle.log`). Game-derived oracle source stays ignored under work/. All six renderer test executables pass. Next validate AC4 projection and live packet ownership, then connect an opt-in native sky mesh path. Full parity remains incomplete; defaults and published checkout are unchanged.

The full public suite also passes: 84 tests; git diff --check is clean.


### Live AC4 sky audit and projection fixture (2026-10-15)

Added opt-in read-only EE hooks (`PS2_AC4_SKY_AUDIT=1`) for the identified AC4 band, dome and haze writers. Guest prologue signatures must match before hooks install, and partial registration rolls back. The band observer snapshots original projected rows/visibility/colours/depth before the original writer runs, then checks the entire emitted packet and returned pointer. It does not alter guest state or substitute geometry. Optional `PS2_AC4_SKY_SNAPSHOT=<private path>` saves one bounded dome object/packet fixture; keep this game-derived output under ignored work/.

Seven renderer executables and 89 public tests pass. New observer tests verify guest-state/memory immutability, output and pointer mismatch detection, address bounds, disabled/mismatched-signature handling and hook rollback. `tools/check_ac4_sky_audit.py <log> <snapshot>` requires positive dome/band coverage, complete exact matching with no declines, and a valid bounded fixture. It does not require haze coverage and must not be reported as haze acceptance when haze count is zero.

The first 8,500-field route ended at weapon selection, so it provided no sky coverage. The extended route reached the dome writer and saved a 1,344-byte original packet plus 0x440-byte object at `work/sky-flight-audit/dome.bin`. That process disappeared without a final audit summary; it is not a completed live acceptance run. The repeat completed normally (10,400 fields, exit 0). Its final report records **59,013 bands, all matched, zero mismatches and zero declines; 1,339 dome calls and 647 haze calls**. The acceptance checker passes, and the saved final frame visually confirms flight gameplay. This validates live band encoding for both producers, not native projection or GPU drawing.

Fixture inspection corrects the earlier colour-layout shorthand: the colour array begins at object **+0x3c8**; the band loop's pair pointer is **+0x3c4 + 4*ring**, selecting previous/current colours when ring > 0. The initial +0x3c4 value overlaps the last ring height and is not the first colour. Segment vectors are four-component vectors at +0xd0, with X/Z scaled by 65536 and Y=-65536; the matrix is +0x40. Object +0 is not an AC5-style screen matrix. Directly adapting the AC5 intent structure without conversion would be incorrect.

An independent float32 projection probe (`work/sky_projection_probe.py`) reconstructs scaled matrix columns, ordered multiply/adds, reciprocal W and 12.4 X/Y conversion. All **40 emitted vertices** in the captured first dome match their original colour/packed-X/Y pairs exactly. This is one-fixture membership evidence, not full clipping/order/continuation or native GPU acceptance. The next implementation must preserve AC4 projection/clipping and depth differences while attaching native geometry to the correct packets. Working defaults remain unchanged.


### Native AC4 sky point projection (2026-10-15)

Implemented native ring-column scaling and projected-point reconstruction in `runtime/include/rn_ac4_sky_project.h`. AC4 scales matrix columns before multiplying its pre-scaled segment vectors; separate float32 roundings and runtime flush/clamp arithmetic are preserved. The helper reconstructs packed 12.4 X/Y and the point writer's returned clip mask, including strict 0/4096 screen-boundary rejection and unchanged X/Y when W rejects before a write. Unsupported nonfinite inputs decline atomically. This computes geometric outputs only, not the original EE/VU0 continuation state, and is not connected to native drawing or guest bypass yet.

Eight renderer test executables and 89 public tests pass. Public tests cover exact screen/viewport boundaries, clip bit mapping, negative/zero W, matrix-column scales and unchanged outputs on unsupported input. A private oracle compiles original AC4 `0x0010FFB8` with the actual runtime arithmetic/flag helpers and build-compatible `-fno-strict-aliasing`: **50,000 varied cases exactly match packed coordinates, returned clip/rejection and preserved X/Y** (`work/sky-stream-probe/guest-projection-oracle.log`). The initial harness omitted that required aliasing flag and miscompiled guest vector/register alias accesses; correcting the harness resolved the discrepancy without changing the native algorithm.

The native C helpers also match all **40 emitted vertices** in the saved live dome fixture, including colours, packed coordinates and non-rejected projection (`work/sky-stream-probe/native-projection-fixture.log`). This is vertex membership evidence, not complete ring/band ordering or every-frame GPU image parity. Next build complete dome/haze geometry plans, verify producer ownership/submission order and enable an opt-in GPU sky path. AC5 parity is still incomplete and defaults remain unchanged.


### Complete native dome packet plan (2026-10-15)

Added `runtime/include/rn_ac4_sky_dome.h`: object decoding, all 29 projected rings, apex replication, adjacent-ring visibility/culling, ordered setup/PRIM/band packets, colours and the ring-15 depth transition. Construction is atomic on invalid input/capacity and preserves only unused high-half PRIM payload padding. It computes original packet output, not guest EE/VU0 continuation or native GPU drawing.

Full-packet comparison exposed an important EE detail: the bit-31 SLL used for ordinary ring visibility sign-extends into the 64-bit mask. That can select a rejected ring-closing endpoint. Its coordinates depend on the corresponding alternating projected-row scratch buffer. The plan preserves this mask behaviour and accepts both initial row buffers. The live observer now snapshots the dome object, actual depth argument and both scratch rows before the original producer, then compares complete generated packets and byte length afterward. This avoids inferring depth or using overwritten scratch state.

A private oracle compiles the original dome producer and its original projection/ring/band helpers with runtime-compatible arithmetic and aliasing settings. **200 varied complete dome packets match byte-for-byte and by returned pointer**, including randomized projection/rings/colours/depth and seeded scratch (`work/sky-stream-probe/guest-dome-oracle.log`). The earlier 1,344-byte fixture also matches the complete plan; its missing depth argument was explicitly inferred as 0xb5, so the live observer uses the actual entry argument instead.

Nine renderer executables and 90 public tests pass. Added full packet ordering, colour/depth split, unchanged padding, behind-camera output, late invalid input atomicity, object bounds and bit-31/stale-closing-coordinate regression cases. Observer tests check entry snapshot immutability and independence from later scratch changes. `check_ac4_sky_audit.py --dome-plan` requires one positive complete report with every dome matched and no declines; missing/duplicate/partial/mismatched reports fail.

The initial live dome-plan run (`work/sky-dome-plan-audit`) completed with **1,370 complete dome packets exact**, zero mismatches/declines, and **60,895 exact bands** across 1,370 domes/671 hazes. That route did not expose the synthetic stale-row edge; the corrected repeat (`work/sky-dome-plan-final`, 10,400 fields, exit 0) passes the strict `--dome-plan` gate with **1,367 complete dome packets exact**, zero mismatches/declines, and **60,779 exact bands** across 1,367 domes/670 hazes. Haze full-packet planning and GPU native sky submission remain outstanding; defaults are unchanged.

### Experimental raw GPU sky dome (2026-10-15)

`PS2_AC4_SKY_MESH=1` now queues AC4-specific dome inputs with their original DMA packets. The consumer matches each triangle against the planned original coordinates, depth and colours before submitting raw segment/ring inputs to vertex shader program 8. Unsupported state, mismatched geometry or exhausted capacity retains GS fallback. The shader performs the ring/matrix projection; rejected closing endpoints retain original stale coordinates explicitly. Original EE production still runs, and GS fragment shading remains in use. This is not guest execution bypass or complete native sky/haze coverage. Public launch defaults remain unchanged.

The completed 10,400-field flight run in `work/sky-native-capture` matched all 1,175 dome packets and 56,368 bands. It delivered 1,172 intents and submitted **150,704 raw GPU triangles**, with **one rejected claim and 56 unused triangles**. Investigate that conservative fallback before default promotion. The eight-field DMA capture replay submits **1,440 native sky triangles**, with zero rejected/unused triangles. Final frame SHA256 is `ff98d340b5b112375fdc27d0d4772620943292ce661479aecfd3a561029fc9d5`, identical with the feature disabled.

New acceptance tool `tools/test_ac4_sky_gpu.py` compares each requested captured field with sky disabled/enabled and requires positive native coverage, no fallback, nonuniform images and byte-identical output. Run with `work/sky-native-capture/flight.gscap --first-field 2 --fields 8`: **all seven visible fields pass** (`work/sky-gpu-parity-r2wll08d`). Field one is a uniform display-buffer priming frame and is explicitly excluded, not counted as visual evidence. These results do not establish other-camera/full-game parity or performance improvement.

All ten renderer test executables and 90 public tests pass. New tests cover raw GPU records, triangle permutation, mismatch/capacity fallback, atomic mesh-reference output and correspondence of every planned reference with packet order, colour, XY and depth. Both native executables rebuilt successfully after replay completion. Next investigate the live fallback and extend camera/scene coverage, then native haze/clouds and remaining window-resolution HUD work. AC5 parity remains incomplete; no publishing performed.

### Sky fallback diagnosis and acceptance hardening (2026-10-15)

Added bounded diagnostics distinguishing state, vertex, capacity and store fallback. A completed 10,400-field diagnostic run (`work/sky-native-diagnostic`) validates 1,192 full dome packets, emits 1,189 intents and draws 152,246 native GPU triangles. Its single fallback occurs at **intent 523, triangle 0/50**, on vertex matching: planned packed XY 68417846, depth 173, colour 80815b4b; actual first vertex (-80.125,-61.5625), normalized depth 8.22544189e-06 and RGBA (91,107,148,128), offsets (1792,1824), zmax 16777215. This is not buffer exhaustion. Packet ownership/order is the next hypothesis to trace; it is not yet established or fixed. The conservative fallback remains correct policy.

Added real serialized-intent/planner/GPU-consumer integration tests and rejection tests for missing, duplicate, zero and partial native coverage reports. All ten renderer executables and **93 public tests pass**. Repeated visible replay fields 2-7 passed in `work/sky-gpu-parity-k9tt3pg8`; field 8 initially failed because the drive filled and truncated the screenshot. Redundant successful intermediate images were removed, freeing about 395 MiB. Field 8 then passed independently in `work/sky-gpu-parity-iupj8v3h`. The acceptance tool now records per-field SHA256 evidence, deletes identical intermediate image pairs, and retains final/differing images to limit storage. No claim of full-game sky coverage, performance gain or AC5 parity.

### Sky packet ownership instrumentation (2026-10-15)

Queued AC4 dome intents now fingerprint their original packet bytes; DMA handoff diagnoses changed bytes or a handoff starting inside the recorded range. This is diagnostic only: matching/fallback policy is unchanged. Added an actual intent-transport test for unsorted ranges, split DMA, one-shot emission, claim closure and overwritten-packet detection. All eleven renderer test executables pass.

The first instrumented 10,400-field route (`work/sky-ownership-trace`) completed with **1,195 dome packets exact**, **1,192 native intents / 152,568 GPU triangles**, and **zero rejected or unused triangles**. No ownership warning occurred. This run did not reproduce the earlier intermittent mismatch and therefore does not prove it fixed or establish the ownership hypothesis. A second instrumented route is running under `work/sky-ownership-repeat`; inspect its process result and final report before drawing conclusions. Defaults remain unchanged and parity remains incomplete.

### Native haze packet planning and ownership repeat (2026-10-15)

The second instrumented sky route (`work/sky-ownership-repeat`) completed with **1,184 full dome packets exact**, **1,181 native intents / 151,860 GPU triangles**, and zero rejected/unused triangles or ownership warnings. Two diagnostic runs are clean, but neither reproduced the earlier mismatch; do not claim a fix. Retain the conservative fallback and diagnostics while broadening coverage.

Added `runtime/include/rn_ac4_sky_haze.h`: AC4-specific complete haze packet planning for rings 14 through 20, six adjacent bands and eight alpha/depth layers per visible band. It preserves the haze setup registers, per-layer ALPHA writes, sign-extended visibility, alternating scratch rows and ordered PS2 depth arithmetic. Inputs include the near/far distances and camera depth coefficients separately from the dome object. It remains disconnected from rendering and guest execution; no haze GPU or bypass claim.

A private harness compiles the original AC4 haze producer and its original ring/projection/band helpers. **200 varied complete haze packets match byte-for-byte and by returned pointer** (`work/sky-stream-probe/guest-haze-oracle.log`). Synthetic tests cover setup, layer ordering, colours/depth, capacity/late-input atomicity and invisible output, linking the actual runtime division helper. Next add a read-only live haze input snapshot/whole-packet comparison, then feed validated geometry to the native sky consumer. Game-derived oracle source remains private under work/.

### Live haze audit and experimental GPU connection (2026-10-15)

The haze observer now snapshots original object/scratch, near/far distances via GP-0x551c, and camera depth coefficients before producer entry. The after hook compares the complete packet and end pointer without modifying guest state. Six hooks register atomically with rollback. Tests cover input bounds, snapshot independence, context/memory immutability, matching and corrupted outputs. The checker now accepts `--haze-plan` and requires positive exact coverage with no missing/duplicate/partial reports.

Completed `work/sky-haze-live-audit`: **652 full haze packets exact, 1,137 full dome packets exact and 54,907 bands exact**, no mismatches or declines; strict dome+haze checker passes. All twelve renderer tests and 94 public tests passed for the observer implementation.

The haze planner now optionally emits raw ring/segment references in exact band/layer order with atomic capacity handling. Synthetic tests compare every reference against packet colour, XY and depth. Added experimental intent kind 9 for AC4 haze, using shader program 8 with the original per-layer GS state. It shares PS2_AC4_SKY_MESH=1 and remains disabled by normal launch defaults. Separate native haze counters distinguish haze GPU evidence from dome coverage. This connection is newly built and not yet capture-validated: `work/sky-haze-native-capture` is running the 10,400-field route. Check its completion and replay off/on image parity before claiming native haze success. No guest execution bypass or full AC5 parity claimed.

### Native haze GPU evidence and stale-packet guard (2026-10-15)

Completed `work/sky-haze-native-capture`: 647 complete haze packets and 1,111 dome packets exact. Native consumer processed **644 haze intents / 370,944 haze GPU triangles**, and **1,751 total sky intents / 515,104 total GPU triangles**. The earlier first-triangle fallback reproduced once (92 unused triangles). This time the ownership fingerprint proves that the queued packet changed before DMA: record 466 range 004d70b0..004d7de0, expected fingerprint 082f4b5c19ce7261 versus actual 06a3d61541c13227. The following first-triangle mismatch is an obsolete description attached to reused guest packet memory, not a shader arithmetic mismatch.

DMA handoff now rejects changed packets or mid-range starts before emitting the native intent, ends any preceding claim and leaves that transfer on GS. Dedicated transport tests verify unchanged packets are emitted, overwritten/partial packets are not, and counters/claim state remain consistent. This deliberately preserves fallback for unowned content; it does not claim native coverage of the overwritten replacement packet. All twelve renderer test executables pass and the public suite passes **95 tests**.

`tools/test_ac4_sky_gpu.py --haze` requires a positive, unique haze report consistent with total native sky coverage, in addition to zero rejected/unused triangles and exact nonuniform images. All **seven visible captured fields (2-8)** pass in `work/sky-gpu-parity-h4vbr_i9`. Field 8 has **8 haze intents / 4,608 haze GPU triangles**, **16 total intents / 6,048 total triangles**, zero fallback, SHA256 `a01317e336db72cdee7f33ff9568afd472ae2b965b8a8a4ad97f57a59232846b`. Identical final off images in three prior test folders were removed only after verifying their hashes match retained on images; logs and manifests remain.

Both executables rebuilt with the ownership guard. A fresh 10,400-field flight run is active in `work/sky-haze-ownership-fixed`; verify its final report before calling the live guard validated. Remaining gaps include broader camera/scene coverage, native guest/GIF bypass for sky, clouds and window-resolution HUD/radar/radio. Normal defaults remain unchanged; no push.

### Ownership-guard run and AC4 cloud helper identification (2026-10-15)

`work/sky-haze-ownership-fixed` completed: **649 haze / 1,114 dome packets exact**, **645 haze intents / 371,520 haze triangles**, and **1,754 total native sky intents / 516,132 triangles**, zero rejected/unused triangles. No stale packet occurred in this run, so it validates ordinary guard-enabled operation but not a live stale rejection; overwritten-packet rejection is covered by the transport regression test.

The connected Ghidra bridge currently has no program loaded and cannot list project files in its headless mode. Continued from the local ELF/generated source. `work/find_cloud_helpers.py` maps ELF program segments and searches AC5 helper signatures. Unique matches identify AC4 sprite projection **0x00114ED8** and sprite-row emission **0x001128D8**. The plane-vertex signature matches 0x00116060 and another helper 0x001F2264; call-graph inspection identifies **0x00116060** as the sky-plane candidate used by **0x00116320**. The sprite projector is called by **0x001152D8**. Both paths connect to the sky dispatcher 0x00111B28 through nearby sky routines. Signatures alone are not semantic/native acceptance.

Plane vertex inputs: a3 world float4, VF4..VF7 projected matrix columns, VF3 clip bounds, VF1.x reciprocal scale, a0 packed XYZ output, optional a1 ST/a2 Q outputs, t0 source UV pair, t1 explicit depth from plane object +0x130. The routine packs projected X/Y with the caller's explicit depth, not projected Z. It returns a clip mask or -1; projection/optional write and continuation semantics remain to be reproduced and oracle-tested.

Added opt-in `PS2_AC4_CLOUD_AUDIT=1` read-only counters on sprite projection and plane vertex before/after hooks, with exact code signatures and atomic rollback. Observer tests cover disabled/mismatched/failed registration, context/memory immutability and rejected vs non-rejected vertices. All twelve renderer tests pass. A fresh route is running at `work/cloud-discovery-live`; inspect its completion/coverage before claiming these paths occur in this mission. No cloud geometry is replaced yet, no defaults changed, no push.

### Cloud discovery and native plane-point reconstruction (2026-10-15)

`work/cloud-discovery-live` completed with **142,065 sprite projections, 388,610 plane vertices and 384,577 non-rejected plane vertices**, confirming both identified paths execute on the flight route. It also exercised the ownership guard: **one stale packet kept on GS**, zero partial packets, and **1,668 sky intents / 489,182 GPU triangles with zero rejected/unused triangles**. This provides live evidence of stale-intent rejection before native submission, in addition to the regression test.

Added `runtime/include/rn_ac4_cloud_project.h` to reconstruct the plane helper's packed XY/caller-supplied depth, returned clip mask and optional ST/Q outputs, retaining values on paths where the original does not write them. No guest state or rendering integration yet. Independent original-helper comparisons exposed a delayed MAC detail: the early rejection observes VF0 multiplied by bounds.z, not projected W. Negative W can still produce accepted screen geometry. Screen coordinates exactly 0 or 4096 reject after XYZ is written, while optional ST/Q remain untouched. Subnormal bounds.z flushes to zero and rejects before XYZ.

Private original-helper oracle (`work/sky-stream-probe/guest-cloud-point-oracle.log`): **200,000 varied points match exact clip, XYZ, optional ST/Q and preserved outputs**, including varied pipeline slot, positive/negative/zero W and positive/negative/zero/subnormal clip bounds. All thirteen renderer executables pass; synthetic regressions cover clip-axis masks, edge write ordering, negative-W acceptance, optional writes and invalid-input atomicity. Next integrate read-only per-call live comparison, then plan cloud-plane packets/native meshes. Sprite projection and complete cloud draw coverage remain separate work. Defaults unchanged; no publishing.

### Live cloud-plane projection acceptance (2026-10-15)

The cloud observer now snapshots original VF4..VF7 matrix, VF3 clip bounds, VF1.x scale, world position, explicit depth, optional UV and original destination values before each plane helper call. It compares the original return mask and actual XYZ/ST/Q afterward without modifying guest memory/registers. It checks optional pointer bounds and preserves outputs on rejection/null-output paths. Synthetic tests verify observer immutability, independent output writes, corrupted ST detection and invalid UV-pointer decline.

Completed `work/cloud-point-live` (10,400 fields, exit 0): **425,542 plane calls matched, zero mismatched and zero declined**; **421,223 non-rejected plane vertices**, plus 155,001 observed sprite calls. New `tools/check_ac4_cloud_audit.py` passes and rejects missing/duplicate/partial/zero/inconsistent reports. This validates plane projection only, not sprite projection or complete native cloud drawing. All thirteen renderer executables and **97 public tests pass**.

Next bind verified raw plane inputs to original emitted quads. AC4 producer **0x00116320 takes packet start in a0**, unlike inherited AC5 tap which uses a1-64; do not reuse AC5 packet-address assumptions. Its accepted quads use 128-byte packets: GIF tag e400000000008001, register sequence 00512512512512f0, PRIM 0x5c, one unused NOP halfword pair, then four ordered ST/RGBAQ/XYZ triples. Its world point helper uses t1 depth from producer object +0x130. Emitted triangles must be associated with exact projected coordinates and texture outputs before raw GPU replacement; whole producer output/continuation and sprite path remain outstanding. Defaults unchanged; no push.

### Complete cloud packet-to-world-input mapping (2026-10-15)

Added `rn_ac4_cloud_packet.h`, a strict 128-byte AC4 quad matcher. It verifies GIF tag/register sequence/PRIM and associates each ordered corner with an independently verified raw point using exact XYZ, ST and Q bits. Conflicting raw inputs for the same packed output reject atomically; identical duplicates are allowed. This avoids deriving approximate UV by dividing packed ST by Q or guessing which raw point produced a corner.

The observer now brackets producer 0x00116320, records up to its 400x4 point limit, checks the setup AD register sequence and maps the entire output. Invalid point inputs, mismatches, overflow, unsupported packet layout and incomplete mapping invalidate the batch. No mesh drawing or guest mutation yet. Five cloud hooks validate signatures and roll back atomically.

Completed `work/cloud-packet-live` (10,400 fields, exit 0): **1,169 batches, all 1,169 mapped, zero declined, 85,645 quads**. All **418,170 plane point calls match**, with no declines/mismatches. The extended `check_ac4_cloud_audit.py --packets` passes and requires positive complete batch/quad evidence. All fourteen renderer executables and **98 public tests pass**; targeted observer tests additionally cover whole-batch mapping, immutable context/memory and malformed-packet rejection. Both executables rebuilt after completion with invalid-point batch rejection made explicit. Next implement the opt-in raw GPU cloud-plane consumer and shader, then capture comparisons. Defaults unchanged; AC5 parity incomplete; no push.

### Experimental cloud GPU adapter and transport correction (2026-10-15)

Added opt-in `PS2_AC4_CLOUD_MESH=1`, intent kind 10 and shader program 9. The consumer uploads independently mapped world positions, matrix, UV/reciprocal scale, colour and explicit depth; it requires exact original triangle XYZ/ST/Q/colour and supported GS state before claiming. Original GS fragment state is retained. Synthetic tests cover raw records, triangle mapping, state/attribute/capacity rejection and malformed intents. `test_ac4_sky_gpu.py --haze --clouds` requires positive separate cloud coverage, zero native rejects/unused triangles and exact images for every requested visible field. No guest execution or cloud GIF bypass yet.

First live run `work/cloud-native-capture` completed with 1,162 fully mapped batches / 85,048 quads and 415,242 exact point calls, but **no native cloud draws**. Pending intent storage filled: 2,301 records refused for space. The field-2 replay correctly failed coverage acceptance; identical fallback images are not a native pass.

AC4 lacks the inherited AC5 flush hook. Added bounded in-place pending-record reclamation: retire delivered and fingerprint-invalidated records, retain intact pending records, preserve active claims, and sort by allocation offset before overlapping compaction. Tests cover reclamation after DMA sorting and open-claim preservation. `work/cloud-reclaim-capture` completed with **zero space refusals**, 1,815 sky intents / 527,826 GPU triangles, zero rejected/unused; cloud coverage still absent. Tracing caller 0x00116918 shows that DMA begins at its 48-byte state prefix before the cloud producer's start. Added strict prefix validation (two AD writes, CLAMP_1 and SCISSOR_1) before widening cloud ownership. Full packet fingerprints remain required.

All fourteen renderer tests and 99 public tests pass. Both executables rebuilt. `work/cloud-prefix-capture` is running with the prefix correction; inspect its final reports and require per-field replay parity before claiming native cloud success. Prior final flight frames are losslessly archived under `work/retained-sky-flight-frames.zip` (eight frames) and `work/retained-cloud-flight-frames.zip` (four frames), verified byte-for-byte before originals were removed. Captures/logs remain available. Defaults unchanged; no push; AC5 parity remains incomplete.

### Cloud-plane GPU capture acceptance (2026-10-15)

`work/cloud-prefix-capture` completed 10,400 fields, exit 0. **1,184 complete cloud batches / 86,236 quads mapped**, **422,995 exact point calls**, zero mismatches/declines. Native drawing: **1,032 cloud intents / 172,218 GPU triangles**, zero rejected/unused. Sky/haze: 1,850 intents / 536,808 triangles, also zero rejected/unused. Two stale packets stayed on GS; no space refusals. Strict cloud packet/point checker passes.

`work/sky-gpu-parity-5vxn20__` passes all **seven visible fields (2-8)** with `--haze --clouds`: each field has positive native coverage and byte-exact nonuniform off/on images. Field 8 reports 8 cloud intents / 1,824 cloud triangles, 16 sky intents / 6,048 sky triangles (including 4,608 haze triangles), zero rejected/unused. Final SHA256: `24e0c51535d3d2773cc97acc7655c996cc77cc367cf9df4726ff5caca629ee5a`.

This establishes cloud-plane GPU rendering for the tested route/capture, not full-game coverage or native guest execution. Next: sprite-cloud projection and clipping (0x00114ED8 / 0x001128D8), then producer continuation/GIF bypass and native-resolution HUD/radar/radio. Preserve separate acceptance gates and keep normal launch defaults unchanged until broader validation.

### Native sprite-cloud projection reconstruction (2026-10-15)

Added `rn_ac4_cloud_sprite.h`, an AC4-specific reconstruction of 0x00114ED8: camera distance, matrix projection, size expansion, visibility rejection, sequential clipped UV interpolation, packed corner coordinates, explicit depth floor/clamp and return W. Rejected sprites retain output fields the original leaves untouched; invalid native inputs decline atomically. This is output reconstruction only, not guest register/pipeline continuation or GPU drawing.

Private original-helper oracle `work/sky-stream-probe/guest-cloud-sprite-oracle.log`: **200,000 exact 32-byte outputs and return values**, with varied pipeline slots, negative/zero W and depth, rotated projection, arbitrary UVs, size sign, depth floor and differing scalar/vector clipping bounds. Public synthetic tests check independently specified clipped UV/XY results, preserved bytes, rejection writes and invalid-input atomicity.

The cloud observer now brackets sprite projection, snapshots guest inputs/output bytes, and compares all outputs without mutation. Six hooks register atomically. Tests cover immutable snapshots, output corruption, pointer bounds and registration rollback. `check_ac4_cloud_audit.py --sprites` requires positive complete sprite-call coverage, rejecting absent/duplicate/partial/mismatched/declined reports. All **15 renderer executables and 100 public tests pass**. Live comparison is running under `work/cloud-sprite-live`; inspect completion and strict checker before accepting sprite projection in gameplay.

Next packet association must account for sorted sprite records: producer 0x001152D8 projects into 32-byte entries and later calls row emitter 0x001128D8 with corner XY/depth and clipped UV values. The row emitter splits horizontally at 0x400 subpixel boundaries and emits reglist 0x53d3 sprites. Do not blindly use the inherited AC5 billboard claim (which does not validate each primitive). No default changes or push.

Live result: `work/cloud-sprite-live` completed 10,400 fields, exit 0. **154,770 sprite calls match all output bytes and return values**, zero mismatches/declines. Existing plane coverage also remains exact: 425,319 points, 1,192 complete batches and 86,451 quads. Strict `--packets --sprites` checker passes. This validates sprite projection on the route; sprite packet association, GPU drawing and guest execution bypass remain outstanding.

### Sprite-row reconstruction and sorted-input association (2026-10-15)

Added `rn_ac4_cloud_row.h`, a complete reconstruction of row helper 0x001128D8. It splits at 0x400 subpixel X boundaries, reproduces ordered PS2 UV interpolation/truncation, preserves full upper XY/depth/UV lanes and builds the reglist 0x53d3 packet. Zero/reversed widths emit no bytes. Insufficient capacity leaves output/count untouched. Private original-helper oracle: **200,000 exact complete packets, untouched tails and returned pointers** (`work/sky-stream-probe/guest-cloud-row-oracle.log`). Public tests independently specify split boundaries/UVs, maximum width and atomic capacity failure.

Read-only row hooks are restricted to the cloud producer return address 0x00115AB0. Completed `work/cloud-row-live`, 10,400 fields, exit 0: **8,379 rows / 32,425 pieces exact**, zero mismatch/decline. Sprite projection (155,001 calls) and plane projection/packets remain exact. Strict `--sprites --rows --packets` evidence passes. These are packet reconstruction results, not native sprite draws.

Added `rn_ac4_cloud_sprite_packet.h` for exact association of row corner XY/depth and UV with original projected records after sorting. Different world inputs with identical packed output reject as ambiguous; identical duplicates are allowed, and rejected/nonexistent records never match. The observer now bounds collection to producer 0x001152D8, resets between batches, invalidates on failed point evidence/overflow and reports mapped/unmapped rows. Ten signature-checked hooks register atomically. Synthetic tests cover association ambiguity and observer collection/failure lifecycle. All sixteen renderer executables pass. Both executables rebuilt; `work/cloud-sprite-mapping-live` is running. Check its completion and `--mapping` before wiring GPU submission. Four prior final frames were losslessly byte-verified and archived in `work/retained-cloud-validation-frames.zip`; source captures/logs remain. Defaults unchanged; no push.

### Verified sprite association and DMA boundary support (2026-10-15)

`work/cloud-sprite-mapping-live` completed 10,400 fields, exit 0: **8,283 rows mapped, zero unmapped**, with all 32,025 emitted pieces byte-exact. Sprite projection (153,153 calls), plane projection (416,183 calls) and complete plane packets also match. Strict `--sprites --rows --mapping --packets` acceptance passes. All 102 public tests passed for these evidence gates.

Sprite rows share a DMA transfer with earlier unrelated cloud draws, unlike the standalone plane packet. Added reserved sprite intent kind 11 and `rn_dma_chunk_limit`: only sprite boundaries subdivide a transfer. AC4 DMA submission preserves source byte order and VIF/GIF stream state, recording intent/capture/graphics events in the same order for each chunk. Existing sky/model intents retain unsplit transfers. The replay executable has the corresponding no-op guest-DMA stub. Transport regressions exercise unsorted row ranges, prefix/gap/suffix data, exact opening/closure, consecutive delivery and unchanged non-sprite transfers. All sixteen renderer executables pass; both executables rebuilt with `work/build_sprite_transport.py` (includes changed ps2_hw and ps2_replay objects).

No sprite intents are queued yet, so native sprite GPU coverage is still absent. Next queue a validated row description at its 32-byte state prefix (TEX0/RGBAQ tag before row helper output), retain packet fingerprints, add the raw-world GPU consumer and validate all captured fields. GS sprite expansion snaps X and adjusts S before handing six vertices to `rn_claim_prim`; the new consumer must account for this and preserve per-piece texture rounding. Do not treat CPU projection/packet evidence as GPU acceptance. Normal defaults and public checkout unchanged; no push.

### Experimental sprite GPU connection: numerical parity pending (2026-10-15)

Added `PS2_AC4_SPRITE_MESH=1`, per-row kind-11 descriptions and shader program 10. Row queueing requires exact packet evidence, unambiguous raw inputs, a valid 32-byte TEX0/RGBAQ prefix and an unchanged packet fingerprint at DMA handoff. The consumer reconstructs expected sprite pieces and validates all six incoming vertices (XYZ, colour, STQ and consistent rounding metadata), retains GS fragment/texture state and submits four world-position records plus six indices per piece. The vertex shader performs matrix projection, extent expansion, clipping, packed XY, horizontal subdivision and GS X snapping. Native CPU packet reconstruction supplies the clipped/snapped UV values; original EE producers and generic GS parsing still run.

Synthetic tests cover full piece consumption, raw upload, incorrect UVs, malformed/nonfinite input, unsupported shuffle state and storage/capacity fallback. All **17 renderer executables and 103 public tests pass**; the additional launcher isolation test for the new flag passes. Both executables and mesh shader compile.

First capture `work/cloud-sprite-native-capture` completed 10,400 fields. All 8,067 sprite intents arrived, but the initial texture-state guard rejected them because AC4 uses a guest-rendered cloud texture. Existing sky/plane coverage remained clean. The revised consumer supports this texture through unchanged GS fragment state, still rejecting any transformed geometry that fails exact vertex comparison. Replay `work/sky-gpu-parity-rjw16qt2`, field 2: **26 sprite intents / 100 raw GPU pieces, zero rejected/unused**, plus clean sky/haze/plane coverage. However, off/on comparison **fails exact image parity: 38 colour-channel bytes differ by at most 1**, bounded X 25..1899, Y 67..1094 in the 2560x2240 image. Both images/logs are retained. Do not call this native sprite acceptance or loosen the checker to hide it.

Next investigate vertex output/interpolation rounding versus the GS shader, using this small deterministic capture. No fresh live run is needed to reproduce. Latest executables include the rendered-texture fix; the live capture predates that fix but carries the complete intents. Current shader program 10 recomputes world XY; UV values already compare exactly against GS on CPU. Depth normalization uses refined float division like the existing cloud/sky paths. Check whether projected GPU positions or raster interpolation differ before changing tolerances. Prior capture/fallback frames are byte-verified in `work/retained-sprite-first-capture-frames.zip`; row/mapping final frames are in `work/retained-sprite-mapping-frames.zip`. Disk space remains tight. Defaults unchanged; no push; AC5 parity incomplete.

### Sprite GPU parity resolved: upload-time row snapping (2026-10-15)

The 38 one-level colour differences were caused by a missing GS upload step, not the world projection. `ps2_video.c::snap_sprite_rows` snaps Y and adjusts T when internal scale exceeds 1, excluding mesh draws. Diagnostic CPU screen meshes still differed; submitting the validated vertices through ordinary GS draws matched exactly. Removing clip-distance output, changing projection/colour arithmetic, splitting triangles and duplicating vertices did not fix it. All temporary diagnostic changes have been removed from the public consumer; program 10 again receives raw world positions.

The native consumer now retains original T plus the equivalent row-snapped T. Shader program 10 chooses the appropriate T and snaps Y only when the actual render scale exceeds 1 and sprite metadata requests it. The synthetic sprite regression checks both T values independently. Added `--scale` to the GPU acceptance tool so native-resolution behaviour is exercised explicitly.

Validation after the correction:
- All 17 renderer executables pass (`python tools/run_renderer_tests.py`).
- All 103 redistributable unit/launcher tests pass (`python tools/run_tests.py --suite all`).
- Rebuilt ac4.exe, gsreplay.exe and mesh.vert.spv.
- `python tools/test_ac4_sky_gpu.py work/cloud-sprite-native-capture/flight.gscap --first-field 2 --fields 8 --haze --clouds --sprites`: all seven fields are byte-identical, positive coverage and zero rejection/unused geometry (`work/sky-gpu-parity-jyl65hq8`).
- Same command with `--scale 1`: all seven fields byte-identical (`work/sky-gpu-parity-qx7a1xow`).
- Final field in each run: 104 sprite intents / 402 GPU pieces; 8 cloud-plane intents / 1,824 triangles; 16 sky intents / 6,048 triangles including 4,608 haze triangles. All reject/unused counters zero.

To reclaim scarce disk space, the prior differing image pair is byte-verified in `work/retained-sprite-mismatch-frames.zip`; the final upscaled accepted image is in `work/retained-sprite-gpu-accepted.zip` (off/on were verified identical before deduplication). Logs and per-field hashes remain in their original directories. One intermediate acceptance attempt ran out of disk space and produced a truncated PPM; that attempt is not acceptance evidence. Successful runs above are separate complete reruns.

This establishes captured sprite GPU parity, not whole-game acceptance or eliminated guest execution. Next implement guarded native producer/continuation and GIF bypass for sky/cloud paths, then AC4-specific window-resolution HUD/radar/radio adaptation. Existing defaults stay unchanged. No public push; parity goal remains active.

### Cloud-sprite GIF parser bypass (2026-10-15)

Added a bounded fast path for AC4 reglist sprite rows in `ps2_gif_transfer`, gated by the existing opt-in sprite mesh flag. It recognizes only the exact 0x53d3 UV/XYZ3/UV/XYZ2 row tag with 1..64 pieces, preflights the entire available row before any writes, then issues the same ordered GS register writes without generic GIF format/register dispatch. Partial transfers, unfamiliar tags, active parser continuations, diagnostics and shallow capture retain the generic path. This does not yet bypass GS vertex assembly or replace the EE producer.

Synthetic comparison extends the existing mesh GIF executable: 1-, 2- and 64-piece rows, every truncated prefix, wrong register/tag and active-parser rejection; compares all register values, Q latch, final GIF parser state and tag counters to generic parsing. All 17 renderer executables pass. The public evidence checker now supports `--sprite-gif` and rejects absent/duplicate/zero/impossible packet counters; all 104 public tests pass (`work/sprite-gif-public-tests.log`).

Replay previously fed VIF one qword at a time, preventing complete-row recognition even though live graphics already use `ps2_vif_transfer`. Replay now uses that same bounded transfer API for literal and dictionary VIF records, retaining its stall handling. The first pre-change replay did not exercise bypass and failed the new coverage gate; it is not acceptance evidence.

Rebuilt ac4.exe and gsreplay.exe. `tools/test_ac4_sky_gpu.py work/cloud-sprite-native-capture/flight.gscap --first-field 2 --fields 8 --haze --clouds --sprites --sprite-gif` passes all seven fields (`work/sky-gpu-parity-s906fk3m`). Every hash also equals the prior accepted `jyl65hq8` hashes, so changing both replay modes has not hidden a baseline change. Recorded register/primitive activity matches every field. Final counter: 144 sprite GIF rows / 1,204 qwords bypass generic parsing. This counter includes equivalent row packets beyond the 104 intent-owned GPU sprite rows and must not be presented as additional native GPU coverage. Duplicate final images were verified against the retained accepted ZIP before removal.

A new 10,400-field boot-to-flight run is in progress at `work/cloud-sprite-gif-live`, launched by `work/run_cloud_sprite_gif_live.py`. Check its exit and native rejection/unused/audit counters before calling live validation complete. No launcher defaults or public checkout changed; nothing pushed.

Next producer work must preserve continuation, not just packet bytes. Helper 0x001128D8 also changes scalar GPR/FPR values and advances VU0 timing. Existing native row packet equality alone does not prove these effects. Extend the original-helper oracle to compare the full context and untouched memory, then use a signature-checked replacement hook only after shadow validation. The same requirement applies to the larger sky/cloud producers.

Live result: `work/cloud-sprite-gif-live` finished all 10,400 fields, exit 0. Strict cloud `--packets --sprites --rows --mapping` and native sky/haze/cloud/sprite/GIF evidence checks pass. Projection: 155,001 sprites and 477,244 plane points exact; all 8,379 rows / 32,425 pieces exact and mapped; 1,370 plane batches / 95,634 quads mapped. Native drawing: 8,367 sprite intents / 32,375 pieces, 1,218 plane intents / 190,930 triangles, and 2,038 sky intents / 553,122 triangles, all with zero rejected/unused geometry. Two stale packet descriptions safely remained on GS. Parser bypass: 81,190 compatible sprite rows / 889,574 qwords (includes other same-layout rows; not native cloud coverage). This is bounded boot-to-flight validation, not a measured performance improvement or whole-game parity. Final private image: `work/cloud-sprite-gif-live/ps2_frame.ppm`.

### Sprite-row producer continuation plan (2026-10-15)

Added `ac4_cloud_row_plan` beside the existing packet builder. It calculates the complete scalar continuation for helper 0x001128D8: affected low GPR lanes (including signed pointer increments), FPR0..2, empty/reversed row behaviour, and pending VU0 flag delivery. It preserves all other context bytes and does not write guest memory. Insufficient output capacity leaves context/output/count unchanged. The helper's first five-slot VU0 advance drains pending flag writes; no vector operation schedules another write, so later advances have no observable effect.

Private original-generated-helper oracle `work/build_cloud_row_continuation_oracle.py` compares **200,000 complete contexts, packet bytes, untouched output tails and returned pointers** against actual helper execution. Inputs include randomized complete register/context contents, five initial flag slots, arbitrary pending flag masks and upper argument lanes. All pass (`work/sky-stream-probe/guest-cloud-row-continuation-oracle.log`). No generated game code is added to public tests.

Public row tests independently assert positive/negative width continuation, exact floating results, pending flag commit, preserved upper GPR lanes/unrelated FPR values and atomic capacity failure. Observer tests check correct context evidence, deliberate mismatch detection and observer immutability. All 17 renderer executables and 105 public tests pass (`work/row-continuation-tests.log`, `work/row-continuation-public-tests.log`). The cloud evidence checker now has `--continuation`, which requires full context agreement on every observed row.

The existing cloud row before/after observers now compute and compare this continuation while leaving original execution enabled. Both executables rebuilt. A 10,400-field live shadow run is in progress in `work/cloud-row-continuation-live`; check completion and the strict continuation gate before enabling a replacement. Next add an opt-in, signature-checked replacement only for validated caller/input/range conditions, preserve atomic fallback, and report native replacements separately from original-vs-plan shadow matches (comparing a committed plan against itself is not shadow evidence). Original sky/cloud producer replacement and window-resolution HUD/radar/radio remain incomplete.

The previous successful sprite GIF live frame was byte-verified into `work/retained-sprite-gif-live.zip` before removing the uncompressed duplicate to conserve disk space.

Live continuation result: `work/cloud-row-continuation-live` completed 10,400 fields, exit 0. **8,379 complete row contexts matched, zero mismatches**. Strict `--packets --sprites --rows --mapping --continuation` passes: 155,001 sprite projections, 477,244 plane points, 8,379 rows / 32,425 pieces, and 1,370 plane batches / 95,634 quads. Existing native sprite/plane/sky GPU coverage remains unchanged from the previous successful live run, with zero rejected/unused geometry; two stale descriptions safely fell back. The continuation gate is now satisfied for this route. No native helper replacement is enabled yet; proceed with the separately counted, guarded replacement next.

### Opt-in native sprite-row helper (2026-10-15)

Added developer-only `PS2_AC4_ROW_NATIVE=1`. It registers a replacement for helper 0x001128D8 alongside the existing signature-checked observer set. Replacement requires the known cloud caller 0x00115AB0, a complete prepared plan, an unambiguous mapped sprite input, unchanged full entry context, and a valid complete guest output range. It commits the planned packet and continuation only after those checks. Other callers and unsupported states retain original execution. Registration failure removes the entire hook set. Normal launchers sanitize the developer flag.

Native replacements are counted separately (`ac4 native rows`); they do not increment original-vs-plan continuation match counts. Post-call checks still verify committed packet/context integrity. `tools/check_ac4_cloud_audit.py --native-rows` requires every observed row replaced with no declines and zero original-comparison counts, and rejects simultaneous `--continuation` use. The original shadow route remains a separate prerequisite, already validated with 8,379 original calls.

Synthetic replacement tests cover no mutation when disabled/unmapped, changed entry context, invalid caller/output range, successful bounded commit, unrelated memory preservation, duplicate invocation fallback, honest shadow counters and registration rollback. The 106 public tests pass (`work/native-row-public-tests.log`). Both executables rebuilt. Live native route is running in `work/cloud-row-native-live` via `work/run_cloud_row_native_live.py`; inspect its exit, strict `--native-rows` result and final image against the accepted shadow run before claiming live acceptance. No default or public checkout changes, no push.

Native row acceptance result: the live run completed all 10,400 fields, exit 0. **8,379 helpers replaced, zero declined; zero original continuation comparisons and zero context mismatches.** The strict `--packets --sprites --rows --mapping --native-rows` checker passes, as do native sky/haze/cloud/sprite/GIF coverage gates. All 17 renderer executables pass after the final test additions. Final PPM is byte-identical to the prior original-helper/shadow route, SHA256 `f53a0707887d1a490dd3de5a1eae32886df330df6161b5e16ec56db9ce6b3c03`. This is a final-frame comparison plus full-run counter evidence, not an all-frame comparison. Native GPU counts remain 8,367 sprite intents / 32,375 pieces, with zero rejected/unused geometry; two stale descriptions retain GS fallback.

The guarded row helper now bypasses its original EE body when explicitly enabled. It does not yet replace whole sky/cloud scene producers or eliminate GS vertex assembly. Next expand native producer continuation to the sky band/dome/haze and cloud projection/batch routines, retaining original-vs-native state comparisons before replacement. Window-resolution HUD/radar/radio remains outstanding. The parity goal stays active.

### Sky-band continuation groundwork (2026-10-15)

Added `rn_ac4_sky_continuation.h` with `ac4_sky_band_plan`, combining the existing packet builder with full helper 0x00111050 scalar continuation. It preserves untouched/high register lanes, empty-band state, signed pointer advancement, visibility-shift termination, last clip-test register values, colours, quad counts and pending VU0 flag delivery. It does not write guest memory. Invalid input or capacity failure leaves context/output/count unchanged.

Private oracle `work/build_sky_band_continuation_oracle.py` compares **200,000 complete contexts and complete output buffers including untouched tails** against the original helper. Unlike the older packet-only band oracle, this test uses the real `ps2_vu0_advance`, randomized flag slots/pending masks and randomized full contexts. Empty/short masks, all-visible closure and arbitrary clip masks are covered. All pass (`work/sky-stream-probe/guest-band-continuation-oracle.log`).

The before/after sky-band observer now records full context comparisons without replacing the original helper. Public tests assert scalar outputs, preserved unrelated lanes, flag delivery, empty bands, atomic failure and mismatch detection without observer mutation. The evidence checker supports `--continuation` and rejects missing/partial/duplicate/mismatched reports. All 17 renderer executables and 107 public tests pass (`work/sky-band-continuation-tests-final.log`, `work/sky-band-continuation-public-tests.log`). Both executables rebuilt.

A 10,400-field original-helper shadow run is active in `work/sky-band-continuation-live`, using native cloud rows as well. Do not enable sky-band replacement until its full-context gate passes. For subsequent replacement, known callers return to 0x001112B4 (dome) and 0x00111538 (haze). Validate output ranges and reject overlapping projected-ring/colour inputs: the pure plan snapshots inputs, while the original writer reads them during output writes. Keep original-comparison counters separate from native replacement counts as with cloud rows. Entire scene producers and window-resolution HUD/radar/radio remain outstanding.

Prior native-row final image was verified identical to `work/retained-sprite-gif-live.zip` before removing its uncompressed duplicate for disk space. No defaults changed or public push.

Live shadow result: `work/sky-band-continuation-live` completed 10,400 fields, exit 0. **60,895 full sky-band contexts matched, zero mismatches**; strict `--dome-plan --haze-plan --continuation` passes (1,370 domes, 671 hazes). Existing native row gate passes: 8,379 replacements, zero declines. Native sky/haze/cloud/sprite/GIF coverage also passes.

However, final-frame parity is **not yet accepted**: compared with the retained prior native-row frame, both PPMs are complete 2560x2240 images but 77,233 channel bytes differ, all by at most 1. Bounds X 0..2559, Y 1573..2239. New SHA256 `2fe0f0d8d0862e444a9bf3c635326e8b029e1442e51384ee78a78a17ab11264c`; private `frame-comparison.json` records the result. This is an observer-only change and all full guest contexts/packets compare exactly, but those facts do not explain or waive the image difference. Preserve both images (prior image is in `retained-sprite-gif-live.zip`) and investigate repeatability/timing/render-state differences before enabling sky-band replacement. No acceptance tolerance was loosened. No live processes remain from this run.

### Sky-band final-frame repeatability investigation (2026-10-15)

Unchanged-build rerun `work/sky-band-repeat-live` completed 10,400 fields, exit 0. All 60,895 band contexts again match. The final image **exactly matches the older accepted original/native-row image**, SHA256 `f53a0707887d1a490dd3de5a1eae32886df330df6161b5e16ec56db9ce6b3c03`, and differs from the immediately prior shadow run by the same 77,233 one-level channel values. Thus the difference is not a consistent output change from the new observer. Do not claim its exact root cause is proven.

Evidence: accepted older run and unchanged-build repeat both record 1,234,138,665 GS writes, 24,318,151 primitives and six dropped presentation fields. The differing shadow run records 1,234,044,054 writes, 24,308,088 primitives and seven dropped fields. Targeted sky/cloud call counts match in all runs. The renderer may replace a queued field under normal asynchronous presentation; existing `PS2_LOCKSTEP=1` waits instead. Host/guest scheduling and rendered-field history are plausible confounders, so uncontrolled whole-run final-image comparisons alone cannot establish a deterministic renderer regression.

Prepared and launched `work/run_sky_band_lockstep_live.py`, output `work/sky-band-lockstep-live`, with the same route and native cloud rows, but `PS2_LOCKSTEP=1`. This is the original-sky-helper baseline for a later guarded native-band test, not evidence of replacement yet. Check the live process/session before rebuilding or repeating. The current sky-band observer still leaves original execution enabled. Next implement the guarded native sky-band replacement with known caller, disjoint/bounded output/input and unchanged entry checks, keeping its counts separate; compare the paired controlled runs.

Prior differing image is byte-verified in `work/retained-sky-band-shadow.zip`; repeat's identical image was verified against `work/retained-sprite-gif-live.zip` before duplicate removal. Comparison JSON/logs remain in each directory. Defaults unchanged; nothing pushed; full AC5 parity remains incomplete.

### Guarded sky-band replacement implementation (2026-10-15)

Added developer-only `PS2_AC4_SKY_BAND_NATIVE=1`, registering helper replacement after the existing signature checks. Only dome/haze callers 0x001112B4 and 0x00111538 are eligible. Before committing, it verifies the full entry context, prepared plan, bounded output range, disjoint output/input ranges, and unchanged copies of both rings and colour words. Changed input/context, recursion, stale invocation or unsupported caller falls back without writes. The planned packet/context are committed together. Replacement counts are separate from original-comparison counts; post-call integrity checks remain active.

Synthetic tests cover disabled mode, changed ring memory and context, overlap rejection, successful commit, no self-counted shadow evidence and replacement-registration rollback. All 17 renderer executables pass (`work/sky-band-native-tests.log`). `tools/check_ac4_sky_audit.py --native-bands` requires complete positive native coverage with zero declines/mismatches and zero original comparison counts, rejects combined native/shadow flags, and has negative evidence tests. Public suite is running in `work/sky-band-native-public-tests.log`.

The lockstep baseline is still running on the prior executable; native band code is not yet built into the live game. Wait for its confirmed terminal status before rebuilding. Then run the same route/settings with only the native band flag added and compare complete evidence and final image. No defaults or public checkout changed; no push.

Lockstep baseline completed 10,400 fields, exit 0, with no dropped fields. All 60,895 sky-band continuations match; dome/haze packet evidence passes. Final hash is the original accepted `f53a0707887d1a490dd3de5a1eae32886df330df6161b5e16ec56db9ce6b3c03`. Image archived and byte-verified in `work/retained-sky-band-lockstep.zip`; hash/log/snapshot remain in `work/sky-band-lockstep-live`.

All 108 public tests pass. Rebuilt ac4.exe and gsreplay.exe with the guarded native sky-band code. Paired native test is now running in `work/sky-band-native-live` via `work/run_sky_band_native_live.py` (lockstep plus native cloud rows and native sky bands). Active shell session 39009, initial child PID 25056. Do not rebuild/restart while it is live. Next inspect its terminal status, require `check_ac4_sky_audit.py ... --dome-plan --haze-plan --native-bands`, existing cloud native-row/GPU gates, no dropped fields and exact final PPM comparison against the lockstep archive. Native sky-band live acceptance remains pending.

HUD adaptation inspection while the native sky-band run completes: `rn_intent_init` returns early for AC4 after sky/cloud observer registration, so none of AC5's 2D group-writer hooks execute. `rn_claim_prim_slow` also returns from its AC4 branch before the inherited active-element `claim_2d` logic, and AC4 `rn_claims_init` does not enable `on_2d`. Thus enabling the overlay renderer alone is not a verified AC4 HUD implementation. The existing `ps2_video.c` overlay scanner/promoter and safety checks are reusable; AC4-specific writer addresses, packet boundaries/ownership and lifecycle must be established first. Native caption rendering must retain existing strings/timing and GS fallback, not reuse AC5 guest addresses. This is source inspection, not completed HUD coverage.

### Native sky-band live acceptance (2026-10-15)

`work/sky-band-native-live` completed all 10,400 fields, exit 0, with lockstep enabled and no dropped presentation fields. Strict `--dome-plan --haze-plan --native-bands` passes: **60,895 sky-band helpers replaced, zero declines; zero original-comparison counts and zero context mismatches**. Dome/haze packet checks remain exact (1,370 domes, 671 hazes). Cloud `--packets --sprites --rows --mapping --native-rows` passes with 8,379 row replacements and zero declines. Native GPU sky/haze/cloud/sprite/GIF coverage passes; two stale descriptions safely retain GS fallback.

The final PPM is byte-identical to the original-helper lockstep baseline, SHA256 `f53a0707887d1a490dd3de5a1eae32886df330df6161b5e16ec56db9ce6b3c03`; result recorded in `work/sky-band-native-live/comparison.json`. Both controlled runs have zero presentation drops. This is final-frame parity plus full-route state/packet evidence, not all-frame or whole-game acceptance. The experimental sky-band helper now bypasses its original EE body when requested. Full dome/haze/cloud scene producers, broader material/texture acceptance and window-resolution HUD/radar/radio remain outstanding. No running process remains from the paired validation. Defaults unchanged; no public push; parity goal active.


### AC4 screen-writer discovery (2026-10-15)

The AC5 primitive writer/rectangle/line signatures do not occur in the AC4 ELF. Do not transplant the AC5 addresses or enable its ownership table for AC4. Static inspection found shared sprite rows at 0x00112820 (untextured), 0x001128D8 (textured), and 0x00112A10, plus several packet-writing candidates. The 0x00227xxx/0x00228xxx callers include full-screen/state-setup operations, so screen coordinates or sprite tags alone are not HUD evidence. Candidate 0x0020CF40 updates a packet cursor in its control object; it is not the AC5 return-end primitive API. Generated functions can contain multiple earlier entry labels; analysis must follow the entry goto instead of treating the entire generated C body as a single original routine.

Added developer-only `PS2_AC4_2D_AUDIT=1`, a read-only census of 11 candidate writers. It validates four entry words per site before installing any hooks, rolls back registrations on failure, and aggregates at most 128 writer/return-site pairs with first-call argument samples. Overflow is explicit. No packet ownership, geometry replacement, text handling, or guest mutation is performed. Normal launchers sanitize this flag with the other PS2 developer environment variables.

Tests cover disabled operation, signature rejection, rollback at every registration position, repeated initialization, unchanged full context, first-sample preservation, separation by caller/writer, invalid site handling and bounded overflow. All 18 renderer executables and 108 public tests pass (`work/2d-audit-tests.log`, `work/2d-audit-public-tests.log`). Both executables rebuilt (`work/2d-audit-build.log`). Controlled 10,400-field route running in `work/2d-audit-live` with lockstep and accepted native sky-band/cloud-row settings; wait for terminal status and inspect its census and final image before claiming live acceptance. These are discovery candidates, not completed native HUD coverage.

The duplicate native sky-band final PPM was byte-compared against `retained-sky-band-lockstep.zip` before removal to conserve disk. No defaults changed and nothing pushed.


Census live result: `work/2d-audit-live` completed 10,400 fields, exit 0. It observed **132,848 calls / 18 writer-return-site pairs, zero overflow**. Candidate 0x00112A10 from return-site 0x0022FC18 accounts for 89,593 calls; its producer is 0x0022F810, called by 0x00230048. It is still unclassified, not proven text/HUD. Several other candidate sites were never invoked. Known cloud row caller 0x00115AB0 retains 8,379 calls. Use the complete census log to prioritize caller/packet inspection.

Final image acceptance is pending: full-sized PPM differs from retained baseline in 361,448 channel bytes, maximum delta 2, SHA256 `2d0f6b92a1ebe6bb66962e11daec8598f7ae61ae994eefec9bb7f2ef7291cc99`. Do not dismiss this as observer-only noise or claim matching images. Image byte-verified into `work/retained-2d-audit-live.zip` before uncompressed removal. Prepared same-executable control `work/run_2d_audit_control_live.py` with only the census flag disabled; compare it before enabling any 2D ownership. No HUD replacement or defaults changed; parity remains incomplete.


### Screen-sprite packet reconstruction (2026-10-15)

The same-executable census-disabled control completed 10,400 fields, exit 0. Its final image is also different from the older baseline: 149,996 changed channel bytes, maximum delta 2, hash `3235f309ff3a36d8997e5b745a81b87e5bff0eea6d43f459f84fd4219cd4672f`. It differs from the census-enabled run by 223,716 channel bytes, maximum delta 2. This shows variation is not confined to census-enabled runs, but does not prove the cause or waive image acceptance. Control image archived and byte-verified in `work/retained-2d-audit-control.zip`; per-run comparison JSON retained.

Static inspection identified shared packet open/close/allocation helpers 0x00104100, 0x00104118 and 0x001041E8. The fixed allocation helper takes an ordering table, bucket and qword count. These reveal additional producers missed by the sprite-tag search. Census expanded to 15 sites and 512 bounded caller pairs, including 0x0015FEE0, a textured screen-rectangle helper. The dispatcher 0x0015EAE0 calls many 0x00160xxx..0x0016Dxxx screen routines that use that helper; semantic HUD ownership still requires live packet/visual confirmation.

Added `rn_ac4_screen_sprite.h`: a pure 64-byte GIF packet reconstruction of 0x0015FEE0. It preserves the original coordinate narrowing, display-flag 3/4 width scaling, expanded rectangle/UV adjustment, depth, reglist padding and overflow semantics. It does not emulate allocator side effects or replace guest CPU execution. Private oracle `work/build_screen_sprite_oracle.py` executes both the original helper and its actual allocator, and checks 200,000 randomized packet outputs plus untouched tails against the reconstruction. All match. This is packet equivalence, not full CPU continuation or completed native HUD rendering.

Public synthetic tests independently cover normal/scaled/expanded rectangles, edge overflow, padding, output tails and atomic insufficient-capacity rejection. All 19 renderer executables pass (`work/screen-sprite-tests.log`). The newly added test initially had two mistyped expected packed constants; those were corrected after checking the actual bit layout, without changing the oracle-matched builder.

Both executables rebuilt (`work/2d-allocator-build.log`). Expanded live census is running via `work/run_2d_allocator_live.py`, output `work/2d-allocator-live` (shell session 44483, initial child PID 40224). Do not rebuild while live. Next inspect terminal status and census, then add a guarded original-vs-plan packet observer for the screen helper before any native 2D ownership. No defaults changed; no push; parity goal active.


Further comparison evidence: both census and disabled-control differences from the retained baseline are confined to X 0..2559, Y 1573..2239. All three have 10,400 delivered fields and the same native helper counts, but GS command totals differ: baseline 1,234,138,665 register writes / 24,318,151 primitives; census 1,233,997,606 / 24,317,715; disabled control 1,234,082,239 / 24,317,976. Thus identical input-field budgets plus lockstep do not establish identical full command streams. Exact cause remains unproven. For upcoming 2D renderer acceptance, capture a command stream once and compare both renderer modes on that identical stream, alongside separate live ownership/state evidence; do not relax pixel equality to hide this confounder.


Expanded census completed all 10,400 fields, exit 0: **2,186,831 calls / 206 writer-caller pairs, zero overflow**. Helper 0x0015FEE0 is live: **20,868 calls across 26 return sites**. Most callers execute 671 times (the flight segment), with bucket 1 screen elements, bucket 2 central/side elements and one bucket 4 caller. Key producers are 0x00162958, 0x001633C8, 0x00163818, 0x00163C08, 0x00160598, 0x001609D8 and 0x00161250. Complete machine-readable census: `work/2d-allocator-live/census.json`. This supplies concrete live screen-helper coverage for the packet-shadow/native-ownership next step, without assigning unverified semantic names to elements. Final image hash `66f80e0cb160532c27c1abeb03e5853e2ba576f1069ed63a0e7e012104973e3d` retained in the run directory; no image-parity claim for this discovery run. No live test/build process remains.


### Screen-sprite live packet shadow (2026-10-15)

Added opt-in `PS2_AC4_SCREEN_SPRITE_AUDIT=1`. It validates the original helper and allocator signatures, snapshots only bounded main-RAM input/output locations, and compares the original helper's completed 64-byte GIF packet with the pure reconstruction. It additionally verifies the allocator's DMA header, VIF DIRECT count, output cursor and helper return value. Eight nested invocations are tracked independently; unsupported memory, excess nesting, unmatched returns and unfinished calls are explicitly counted. Hook registration is atomic. No guest writes, execution replacements or native ownership are introduced.

Synthetic tests cover complete-context/memory immutability, independent expected packet bytes, deliberate packet/header/cursor/return corruption, invalid input/output pointers, nesting overflow, unpaired returns and registration rollback. All 20 renderer executables pass (`work/screen-shadow-tests.log`). New strict evidence checker `tools/check_ac4_screen_shadow.py` requires one complete report with positive calls, every call matched and zero mismatches/declines/unpaired/open calls; negative evidence tests pass. All 110 public tests pass (`work/screen-shadow-public-tests.log`). Both executables rebuilt (`work/screen-shadow-build.log`).

Live 10,400-field route running in `work/screen-shadow-live` via `work/run_screen_shadow_live.py` (shell session 42353, initial child PID 42976). Wait for terminal status and run the strict checker. Next native handoff must remain opt-in, attach a packet hash and exact DMA boundaries, retain inherited 2D material/target safety rejection, and avoid applying AC5 layout addresses/reanchoring rules to unclassified AC4 elements. Validate native vs GS using one captured command stream, in addition to live packet ownership evidence. No defaults changed or public push.


Screen shadow live acceptance: all 10,400 fields completed, exit 0. **20,868 calls, 20,868 packets/allocations matched, zero mismatches, declines, unpaired returns or open calls.** Strict `check_ac4_screen_shadow.py` passes. Original helper and allocator executed throughout. Image archived and byte-verified in `work/retained-screen-shadow-live.zip`; this acceptance is packet/allocator evidence, not a whole-run pixel-parity claim.

### Opt-in screen-sprite native 2D handoff (2026-10-15)

Added developer-only `PS2_AC4_SCREEN_2D=1`. It enables the verified original-helper shadow and queues a native 2D group only when the complete packet/allocation check passes. Each group carries its original caller, exact 64-byte packet range and a packet-content hash. DMA splitting now respects these group boundaries, and stale/partial transfers retain GS fallback. The existing 2D target/material safety checks decide whether each claimed group can draw natively. All groups preserve original AC4 placement (`RN_G_WORLD` disables inherited AC5-specific reanchoring); this flag does not assert that every element is a world marker. No original EE helper is bypassed by this handoff.

Transport tests exercise successful group emission/closure, exact boundaries, caller/placement flags and stale-packet rejection. Shadow tests require a matching packet before the queue callback. All 20 renderer executables pass (`work/screen-2d-tests.log`). Added `tools/test_ac4_screen_gpu.py` to compare GS, native-in-place and native-overlay on the same private DMA stream at 1x; it requires visible images and positive, consistent 2D coverage, and separately requires positive overlay composition/lifted draws. Negative evidence tests reject missing/contradictory/unfinished/overflowed results. All 112 public tests pass (`work/screen-2d-public-tests.log`).

Both executables rebuilt (`work/screen-2d-build.log`). Live route/capture running in `work/screen-2d-capture` via `work/run_screen_2d_capture.py`, shell session 44768, initial PID 9176. Two DMA fields captured from field 10,000; 1x internal and overlay scale conserve disk and establish base-resolution shading parity first. Do not rebuild while live. Inspect live shadow and 2D counters, then run the GPU checker. Live/GPU acceptance remains pending. Normal defaults unchanged, no public push, no full HUD/radar/caption parity claim.


First screen-native live result: 10,400 fields completed, exit 0; 20,062 original screen packets matched, no shadow failures. Transport produced 20,031 closed groups, but **zero groups accepted natively**: every group was rejected by AC5's slot-0 target restriction. This is successful conservative fallback, not native HUD acceptance. Added bounded `PS2_AC4_SCREEN_STATE_LOG` diagnostics to identify AC4's actual target before adapting that restriction and the overlay's slot-0 compositing assumptions.

The first two-field capture is **invalid/truncated and excluded from acceptance**. Its header declares stream offset 22,888,856 plus 2,357,199 stream bytes, but the file is only 22,892,576 bytes. F: ran out of space during recording even though the capture footer claimed completion. Replay read zero complete fields; no GPU parity result is valid for it. SHA256 `45eb2ce7b3c040fb921efeb662520b64b987f06c4afdf544edf6a236e90509f9`. Copied and hash-verified to `C:/Users/satsp/Documents/Codex/2026-10-08/ba/ac4-renderer-evidence/screen-2d-flight.gscap` before removing only the F: duplicate. Original path pointer is `work/screen-2d-capture/capture-path.txt`.

GPU test preflight now verifies complete header/stream/index lengths, the truncated marker and requested field availability before starting Vulkan. Negative truncation tests pass; all 113 public tests pass (`work/screen-capture-public-tests.log`). The new test initially failed only during sandbox temporary-directory cleanup; its temporary files now live under the verified worktree work directory. No test checks were weakened.

Fresh recording now runs directly in `C:/Users/satsp/Documents/Codex/2026-10-08/ba/ac4-renderer-evidence/screen-2d-complete`, via `work/run_screen_2d_complete.py` (shell session 28801, initial child PID 8236). Same opt-in native mode with bounded target diagnostics; source/build still on F:. Check terminal status, full capture integrity and target logs. Do not rebuild while this run is live. Native-in-place and window-resolution overlay both remain unaccepted until positive coverage and valid replay comparisons are obtained.


Fresh C: capture completed 10,400 live fields, exit 0, and passes the complete-file preflight: **25,270,943 bytes, two fields**, SHA256 `17a6e01f9a39d1314c29218e43e6598b12e863138f86bcad656396013ea1f473`. Use `C:/Users/satsp/Documents/Codex/2026-10-08/ba/ac4-renderer-evidence/screen-2d-complete/flight.gscap`, not the earlier truncated file. Live original screen-packet shadow still passes. No running process remains from this capture.

`work/screen-target-complete` replays both captured fields successfully (12,236 primitives, 335,362 GS register writes, 1,890 transfers). There are 64 closed screen groups, all conservatively rejected because the target is not slot 0. Target diagnostics establish **rt=1, texture not a render target, framebuffer 512x448, Z test enabled but Z writes disabled, no DATE or frame mask** for sampled screen sprites. Frame-2 draw trace begins with a full-screen transfer into rt0 sampling `texrt=2` (slot 1 + 1 encoding), and ends with screen/HUD content drawn into rt1. Presentation uses rt0. This is concrete evidence that AC4's scene-to-display composition uses rt1 -> rt0, whereas the inherited AC5 overlay assumes rt0 -> another presentation target.

Next adapt the native 2D target policy and overlay scene-source/composite/read-hazard checks together for this verified AC4 mapping, retaining all other rejection rules and conservative fallback. Do not merely remove the target restriction, and do not claim native/window-resolution coverage until the valid capture yields positive 2D/overlay counters and exact base-resolution comparison. The opt-in path still draws zero accepted native 2D elements at this checkpoint. No default changes or push.

### AC4 screen target adaptation and overlay diagnostics (2026-10-15)

Adapted the 2D scene-target policy and overlay source/read-hazard predicates together: AC4 uses scene slot 1, AC5 retains slot 0. Other material, depth, mask and ownership restrictions remain. Both game variants compile and exercise the actual predicates in `tools/test_2d_target_policy.py`.

Fixed an overlay presentation crash: the diagnostic cache indexed the standalone `ov_comp` target as though it belonged to the `rts` array. It now has a dedicated cache entry, covered by tests for every regular target and the overlay. Temporary crash instrumentation was removed from the replay executable.

Replay now drains the published/in-progress render before reporting counters or reading the final image. A separate active-render flag avoids accepting an older frame's completion while the last field is still rendering. Tests cover idle, queued, active and stopped-renderer cases. Added replay `--hidden` (still Vulkan), skipped modal window redraws for hidden tests, and made the screen GPU checker use hidden lockstep replay. Normal gameplay defaults are unchanged.

**Native in-place screen sprites pass the captured comparison:** 64 groups, 64 accepted whole, 64 accepted primitives, zero fallback/unclosed groups; exact GS image SHA256 `02618856d1d22d3ec76c3795dc44f29ed4ae4d1fd5afcb346fcf0a0b286c4921`. This covers this helper/capture, not all HUD/radar/radio producers.

**High-resolution overlay acceptance still fails.** Latest deterministic evidence is `work/screen-gpu-parity-v_c94ute` from the complete C: capture. It renders two fields, composes one overlay with 20 lifted draws, and retains 27 native 2D draws in place for hazards. Overlay SHA256 is `3c919ae5be4271213d1498d61262dcf223985e8700110c6d82a8efd4bf1b0b64`, differing from GS. The checker correctly fails exact comparison; no tolerance was loosened. Earlier matching overlay hash in `work/screen-gpu-parity-60k4zsv4` has differences bounded by x=24..483/y=27..420 and maximum channel delta 103, concentrated around text/HUD edges, so this is not rounding noise.

`work/composite-trace/run.log` records the actual scene-to-display transfer: first strip XY `(0,0.5)..(64,448.5)`, UV `(0,0)..(64,448)`, Q=1, sprite rounding flag `0x20000000`; draw flags `0x02040004`, blend `0x048`, depth `0x119`. The current inherited overlay adds lifted content after this offset/filtering stage. This is a concrete composition-order discrepancy to resolve, not evidence that the changed pixels are acceptable. Next preserve AC4's scene/display transformation when composing native 2D (including the previous-field overlay/texture lifetime), and rerun strict 1x parity before window-resolution/live acceptance. Do not claim AC5 parity or enable this overlay by default yet.

Validation: both executables rebuilt (`work/screen-hidden-build.log`), all 22 renderer executables pass including new drain/target tests (`work/screen-drain-renderer-tests.log`), all 113 public tests pass (`work/screen-drain-public-tests.log`). Final hidden GPU test fails only at the deliberate exact-overlay comparison (`work/screen-hidden-gpu.log`). No public checkout edits or push. F: has under 10 MB free; keep future larger captures on the existing C: evidence directory and never delete unrelated data.

### Pre-display composition parity (2026-10-15)

Added developer-only `PS2_AC4_SCREEN_PRECOMPOSE=1`. At the original scene-to-display transfer, it promotes the previous scene's lifted list, composites those draws over the original scene target, and makes the original copy sample the resulting image. It invalidates/rebinds the affected draw state before resuming the original display list. Consequently AC4's actual copy geometry, filtering and destination writes still execute. This is currently restricted to equal overlay/internal scales and no open colour-clip pass; it is groundwork for window-resolution output, not completion of that feature. Explicit flag value 0 disables the new branch. Normal launchers/defaults remain unchanged.

The strict checker now accepts `--precompose`, records that choice in comparison results, and requires a positive dedicated pre-display-transfer counter in addition to positive lifted draw coverage. New evidence-negative tests reject absent, zero or duplicate transfer reports.

**All three captured modes now match exactly with precomposition enabled.** `work/screen-gpu-parity-gosnzhli` contains GS, native-in-place and native-overlay PPMs with the same SHA256 `02618856d1d22d3ec76c3795dc44f29ed4ae4d1fd5afcb346fcf0a0b286c4921`. Native coverage is 64/64 whole elements and primitives, zero unclosed/fallback groups. The overlay reports **one actual pre-display transfer, 20 lifted draws**, with 27 hazard-retained draws. This preserves the original exact comparison; no image tolerance or coverage gate was relaxed. An earlier prototype run `work/screen-gpu-parity-9oi5lvo0` also matched; the final run additionally validates the dedicated counter and corrected explicit flag parsing.

Both executables rebuilt (`work/screen-precompose-build.log`); all 114 public tests pass (`work/screen-precompose-public-tests.log`). Renderer tests are in `work/screen-precompose-renderer-tests.log`. Twelve earlier diagnostic PPMs were compressed to `work/retained-screen-overlay-diagnostics.zip`, byte-compared after reopening the archive, then only those verified PPM duplicates removed; logs and PNGs remain. No unrelated files were deleted.

Next extend the pre-display path to a separate high-resolution display destination and replay the verified original copy at that resolution. Preserve subsequent display-target reads/writes or conservatively retain unsupported sequences. Merely raising the source overlay scale while copying into the original low-resolution target does not satisfy window-resolution output. Validate 1x remains exact, then higher-scale geometry/texture placement, dynamic texture lifetime, multiple transfers and live HUD/radar/radio coverage before exposing defaults. Full AC5 parity remains incomplete.

### Separate native display destination (2026-10-15)

The experimental precompose path now has a second, independently sized Vulkan colour/depth target for the final display copy. The original transfer is rendered into the regular GS target and replayed at overlay scale into this native display target. Presentation and screenshot readback explicitly select the new target; its diagnostic cache has its own slot (33), covered alongside all other targets. No global internal-resolution variable is temporarily changed. The previous equal-overlay/internal-scale restriction is removed.

A later write to the display target cancels native display presentation, retaining the updated regular target. Fields with more than one scene-to-display copy keep their new HUD draws in GS, because the overlay upload currently supports one immutable vertex list per field; a preceding pending list can still compose once. The actual transfer-count predicate is tested for zero/one/multiple/wrong-source copies in both game configurations. This conservative limitation remains an explicit coverage gap, not a claim of complete menu/effect support.

Final 1x GPU validation passes unchanged exact-image requirements in `work/screen-gpu-parity-a16yp9yk`: GS, native in-place and precomposed native display all have SHA256 `02618856d1d22d3ec76c3795dc44f29ed4ae4d1fd5afcb346fcf0a0b286c4921`. The checker now also requires positive native display-copy and presentation counters with nonzero scale/dimensions, rejecting missing/zero/duplicate evidence.

`work/screen-display-2x` tests **1x internal scene plus 2x native display**, using the same complete two-field capture. The log proves one copy and one presentation from the separate **1280x896 target**, cropped by AC4's display rectangle to a **1024x896 screenshot**. Overlay coverage remains 20 lifted draws, with 64/64 accepted screen groups and no unclosed groups. `frame.png` was visually inspected: the scene, HUD, radar and radio caption are visible with expected broad placement. This is positive rendering/size evidence, not a strict higher-resolution visual-parity or complete producer-coverage claim. Script/config/log are retained; no whole-screen image-resize operation creates the game output.

Both executables rebuilt (`work/screen-display-final-build.log`); all 22 renderer executables pass (`work/screen-display-renderer-tests.log`), all 114 public tests pass (`work/screen-display-public-tests.log`), and diff whitespace validation passes. Nine earlier precompose PPMs were archived and byte-verified in `work/retained-screen-precompose-parity.zip` before removing those duplicate files. Normal defaults and the public checkout are untouched.

Next validate higher-scale placement/filtering against suitable reference crops, test 4K/automatic scaling and live sequences (radio transitions, radar changes, menu/mission transitions), and inspect unsupported transfer/lifetime cases. Use C: for larger evidence because F: remains nearly full. AC5 parity is still incomplete; do not expose the experimental precompose path as an accepted default yet.

### Automatic resolution and first live native display route (2026-10-15)

Added reusable `tools/test_ac4_screen_resolution.py`, documented in TESTING.md. It runs baseline/native hidden Vulkan replays from a complete capture, saves the settings/config/logs/images, and checks actual native display copies/presentations, output dimensions and target extent. Its unit tests reject wrong scale, undersized targets and zero presentation. This explicitly does not substitute resolution checks for visual/audio/whole-game acceptance. All **115 public tests pass** (`work/screen-resolution-public-tests.log`).

The test passes at 1920x1080 (automatic 3x; output 1536x1344, target 1920x1344) and 3840x2160 (automatic 5x; output 2560x2240, target 3200x2240), with internal scene resolution held at 1x. Captured outputs use AC4's display rectangle; presentation then applies its widescreen mapping. Evidence is under `C:/Users/satsp/Documents/Codex/2026-10-08/ba/ac4-renderer-evidence/screen-resolution-96k6u2kk` and `screen-resolution-5l8u90zh`. Earlier private runners also passed in `screen-display-1080` and `screen-display-auto`; the 5x image was inspected.

Live `screen-native-display-live-auto` completed **10,400 fields, exit 0**. All **20,155** audited original screen packets matched; **20,124/20,124** screen groups/primitives accepted with zero unclosed groups. It reports **646 pre-display copies and presentations**, 652 overlay compositions, 3,998 lifted draws and 11,058 hazard-retained draws. The final 2560x2240 image was inspected and contains the later radio line "You are now under my command", advanced timer and changed HUD values. This is live changing-content evidence, not proof of complete caption semantics or audio sync. Final target allocation was 3200x4480 because the retained internal target height had grown to 896; the display crop remains 2560x2240. Monitor that allocation/performance in later tests.

The live route also logged **two five-second no-vblank stalls at fields 5620 and 7121**, before flight overlay activation. Both traces show DMAC status 004/enabled 000, EE held by thread 0 and thread 1 waiting on semaphore 3, with CD/SIF completion calls in the ring. The run subsequently recovered. Do not dismiss these as harmless, attribute them to the compositor without a control, or claim smooth full-route acceptance. The final flight interval measured 58.1 fields/s including periodic screenshot work; this is not a performance benchmark.

A matching screen-disabled control is currently running via `work/run_screen_native_display_control.py`, shell session **23758**, initial child PID **13656**, writing to `C:/Users/satsp/Documents/Codex/2026-10-08/ba/ac4-renderer-evidence/screen-native-display-control`. Same 10,400-field route, screenshot cadence and original-helper audit; only native screen/precompose flags disabled. Poll that exact session before restarting anything and do not rebuild while it runs. Compare stall fields/traces and final counters/images before attributing a regression. No defaults changed or push performed. Full renderer parity remains active and incomplete.

### Stall control and display-target fallback (2026-10-15)

The screen-disabled control above finished **10,400 fields, exit 0**, without either stall. Its 20,620 screen-helper calls all matched; the 6000/7200-field intervals were 59.9 fields/s with zero drops. A same-executable native-enabled repeat through **7,500 fields** also finished exit 0 without stalls (`screen-native-display-repeat-7500` on C:, script `work/run_screen_native_display_repeat.py`). It reported **zero screen-helper calls**, and the intervals spanning the earlier stall points again ran at 59.9 fields/s with zero drops. Both sessions are terminal. This does not identify or fix the first run's stalls: retain them as intermittent/unresolved, with no reproduced screen-compositor regression established. No game/replay process remains from these runs.

Code review identified a separate correctness issue when the additional native display target cannot be prepared: pre-display composition was gated on that target's availability and could fall back to the older, incorrect post-display ordering. It now still composes into the regular GS display via the original copy; only the extra high-resolution copy/presentation requires a ready native target. Added diagnostic `PS2_AC4_SCREEN_DISPLAY_FALLBACK=1` and `tools/test_ac4_screen_gpu.py --display-fallback` to exercise this path explicitly. The checker requires positive pre-display composition but **no** native display-copy/presentation report in forced fallback mode; ordinary precompose acceptance still requires that report.

Both forced-fallback and restored native-display captures pass exact 1x GS comparison: `work/screen-gpu-parity-mt2wou8g` and `work/screen-gpu-parity-m6_61ig7`, respectively. Each GS/in-place/overlay image has SHA256 `02618856d1d22d3ec76c3795dc44f29ed4ae4d1fd5afcb346fcf0a0b286c4921`. Forced fallback logs one genuine pre-display transfer and no native display presentation. Both executables rebuilt (`work/screen-display-fallback-build.log`); all 22 renderer executables and 115 public tests pass (`work/screen-display-fallback-renderer-tests.log`, `work/screen-display-fallback-public-tests.log`); diff whitespace check passes.

Next validate combined native mesh/material/texture and screen modes, broader HUD/radio/radar state coverage and scale changes, retaining the intermittent loading stalls for comparison. Review the local AC5 implementation rather than counting original EE-producer elimination as a prerequisite it does not implement; the reference qualification near the top records the source evidence. Native 3D processing, VU/GIF bypass, materials/filtering, window-resolution UI and broad live acceptance remain required. No default changes or public push; parity remains incomplete.

### Combined renderer capture and native mip-atlas prototype (2026-10-15)

Combined replay evidence is now under C: `ac4-renderer-evidence/combined-screen` (private runner `work/replay_combined_screen.py`). Baseline, original-execution shadow, combined native execution/geometry/material switches plus screen precomposition, and texture-enabled modes all initially produced the accepted exact 1x image. Actual coverage: **1,060 terrain native activations/packets, 412,196 original instructions replaced**, 3,728 primary and 3,728 secondary native terrain draws, 1,060 whole mesh GIF packets/40,388 qwords bypassed, and the previously verified sky/cloud/sprite/HUD counts. Full terrain continuation, packet memory/order, integer and flag shadow comparisons are zero-mismatch. Existing terrain execution/GPU/GIF validators pass these reports.

This capture has **zero lit/unlit model activations**, zero native material draws and initially zero native mip draws. Its two VU images are terrain-oriented. Therefore identical combined images do not establish aircraft/material/mip coverage. Original aircraft fixtures are available at `F:/downloads/ac04recomptest/ac4/work/third_person_flight.gscap`, `third_person_controlled.gscap`, `third_person_late.gscap` and `third_person.gscap`; validate their headers/coverage before selecting one.

Terrain currently supplies packed GS mip atlases. Added a prototype, **only with `PS2_AC4_ATLAS_TEXTURE=1` plus model textures**, that verifies the canonical `current_mip_texture` layout (2*TW by TH+8), supported packed LOD mode, finite integer dimensions, and clamp regions bounded by the base level. It extracts level 0/its region into an independent native mip image, preserving source state and pixels; removes packed GS mip parameters only after successful allocation. Unsupported layouts/state and allocation failure retain fallback. Tests cover opt-out, base extraction, crop, out-of-base clamps, allocation failure, malformed dimensions/NaN and packed-mode rejection. All 22 renderer executables pass (`work/native-atlas-final-tests.log`); both executables rebuilt (`work/native-atlas-final-build.log`).

Opt-out regression evidence `combined-screen-atlas-gated` keeps every mode exactly at SHA256 `02618856d1d22d3ec76c3795dc44f29ed4ae4d1fd5afcb346fcf0a0b286c4921`. With the separate atlas flag enabled, `combined-screen-atlas-enabled` reports **3,728 base-level extractions/native mip draws/native material draws**, zero allocation fallback and 3,728 remaining unsupported states. Texture output changes to SHA256 `4dc81d2a62067ca081f16e2d18e4533ce635d216331756bdb7e62492a732199c`. Earlier matching prototype evidence is `combined-screen-native-atlas`, whose PNGs were inspected: changed pixels are confined to y=310..447 in this view, maximum channel delta 45, with suspicious terrain detail/rectangular patches. **Visual acceptance is not granted.** Normal launchers do not enable the new atlas flag, and existing ModelTextures behavior is preserved without it.

The inherited mesh shader's point path fetches only level 0, but sampled affected terrain states are **point=0**, packed mip `003F282B`, base 512x512, region clamp 0..63 in both axes. Thus point filtering is not the established cause here. Next inspect derivatives after region clamping and the difference between generated native mip levels and AC4's supplied GS mip content/LOD bias. Do not simply dismiss terrain changes as higher-quality filtering or widen image tolerances. Preserve exact geometry/material comparisons separately from deliberately different mip sampling; use aircraft and island/minification fixtures for broader coverage. No processes remain running from this stage, no defaults changed, no public push.


### Supplied terrain mip levels in the native material shader (2026-10-15)

Native cropped textures now have per-axis clamp-to-edge samplers rather than
always repeating. Mesh shader derivatives are taken before region clamping for
multi-level images. The crop API retains its original repeat wrapper for existing
callers; AC4 selects wrap bits from the source state. Synthetic crop tests cover
repeat, U-only and both-axis clamps. The one-shot `PS2_AC4_ATLAS_DUMP` diagnostic
can save a private decoded source atlas without modifying it.

These changes do **not** accept the experimental base-level conversion. Private
`combined-screen-atlas-clamped` remains different from GS (maximum channel delta
45), and forcing level zero in a temporary shader still leaves dark patches
(`combined-screen-atlas-lod0`, maximum delta 50). The temporary shader was restored.
Inspection of source atlas regions confirms supplied mip content; the experiment
regenerates it and changes LOD selection. Neither clamp nor derivatives alone
solved this discrepancy.

Added a compatibility mip sampling path to `mesh.frag`: it uses the same supplied
atlas levels, per-level region bounds, signed K bias, Q-based LOD and filter modes
as the existing GS shader. The material eligibility predicate admits only AC4
packed modes with valid marker, level count, mode and bit range. Unsupported
states still retain GS fragment fallback; AC5 eligibility is unchanged. This
expands native material coverage but does not claim hardware mip/anisotropy
parity for terrain.

`combined-screen-atlas-material` passes all four exact 1x images at SHA256
`02618856d1d22d3ec76c3795dc44f29ed4ae4d1fd5afcb346fcf0a0b286c4921`, now with
3,728 native material draws (previously zero without conversion). Aircraft capture
`third_person_controlled.gscap` contains **four** fields, not eight; both new
`model-gpu-parity-q9s28pe7` (before compatibility path) and
`model-gpu-parity-7abweodm` (after) pass nine exact 1x images, 2,024 aircraft GPU
draws, 924 secondary draws, 6,752 primary/secondary terrain draws, and exact native
lit/terrain continuation. `third_person_flight.gscap` had no model coverage and
correctly failed that coverage gate; it is not aircraft acceptance evidence.

Island-turn `model-gpu-parity-lb7nqyl2` passes 120 fields at 1x: six exact final
images, 60,872 native terrain activations, 60,765 packets, 25,632,736 original
instructions replaced, 242,525 primary and secondary draws including 37 clipped
draws per pass. This is final-image/captured-state evidence, not every-frame or
full-game acceptance. All artifacts above are private under the C: evidence root.

Both binaries and shader rebuilt. All 22 renderer executables and 115 public tests
pass (`work/native-atlas-material-tests.log`,
`work/native-atlas-material-public-tests.log`). Model acceptance now supports
external output directories, explicit scale, hidden lockstep replay, shared
complete-capture validation and combined native-terrain/material coverage.
The upscaled combined check is recorded below when complete. Defaults unchanged;
no public push. Remaining: validate hardware mip conversion/material appearance,
broader UI ownership and high-resolution/live transition acceptance, and retain
intermittent loading stalls as unresolved. AC5 parity remains incomplete.


Upscaled island-turn acceptance `model-gpu-parity-cl31ubj9` completes 120 fields
at 5x internal resolution. All **seven** final images are byte-identical (SHA256
`1cbdb95d7af68106785fca92e79da72f64c5c4e94d6e1951a3cd90f8a843c4d4`). The new
combined native-terrain/material mode proves 242,525 native material draws
alongside the full native continuation counts above. The final image was inspected:
it ends on the banked ocean view, so it must not be described as a final-image
island-detail acceptance. Intermediate island/clipping state coverage is proven by
the counters, not intermediate-frame image comparison.


Final aircraft regression `model-gpu-parity-928d6zbn`: four captured fields at 5x,
all **11** final images identical to established SHA256
`66357448cd87cdc488b5c652ac35cbb896bd2493b8e871482e0dd4df34f295cc`.
Combined native lit/terrain execution plus materials records 8,776 native
material draws, with the same 2,024 aircraft and 6,752 primary/secondary terrain
draw counts. No default changes or public push. All replay sessions for this
stage have exited; continue with hardware mip preservation/conversion and broader
high-resolution UI acceptance, retaining the compatibility path as the exact
sampling reference.


### Preserved supplied levels in native Vulkan images (2026-10-15)

Added opt-in `PS2_AC4_SUPPLIED_MIPS=1` alongside native model materials/textures.
It takes priority over the earlier base-only atlas experiment. A pure layout helper
accepts only bounded, aligned power-of-two crops whose footprint remains identical
at every supplied level. It extracts each supplied level into an immutable owned
chain; Vulkan uploads those levels individually instead of regenerating them.
Texture staging and memory accounting include the chain, retirement frees it,
and same-key recycling no longer replaces native multi-level allocations.
Source atlas pixels and state are not modified. Unsupported crop/layout/allocation
cases retain the original material. Conversion is gated on native material use so
a forced GS fragment draw cannot accidentally sample the new layout.

The mesh shader recognizes supplied native levels separately from packed atlases,
applies the original per-level wrap/clamp and Q-based LOD, then fetches the matching
Vulkan level. This preserves data/LOD before introducing hardware filtering. It
**does not yet establish anisotropic sampling**: this compatibility reference uses
explicit texel fetches and interpolation. The native sampler has mip/aniso support,
but binding it alone is not proof that the shader uses that filtering.

Private evidence under the C: renderer-evidence root:
- `combined-screen-supplied-mips`: all four modes match the accepted exact image
  `02618856d1d22d3ec76c3795dc44f29ed4ae4d1fd5afcb346fcf0a0b286c4921`, including
  3,728 supplied native mip draws and zero allocation fallbacks.
- `island-supplied-mips`: eight fields at 5x, all four modes match
  `0bd7a3a8cbfa0f93fe6f8253cd6aaa87bd61ebc622cd664da60c5cf8b8ea7265`, including
  16,342 supplied native mip draws and zero allocation fallbacks. This earlier
  final field contains the island view, unlike the 120-field ocean endpoint.

Synthetic checks cover per-level source positions and byte contents, destination
guards, malformed dimensions, misalignment, undersized crops, unchanged rejected
layout outputs, native-state flag propagation and allocation failure without state
mutation. Normal launchers/defaults are unchanged. Next compare native hardware
filtering with the preserved-level reference and inspect minification across
island/sun/aircraft views, then continue broader HUD/radio/live-transition gates.
No public push; full parity remains incomplete.


Supplied-level validation completed: both executables rebuilt, all 22 renderer
executables and 115 public tests passed (`work/supplied-mips-build.log`,
`work/supplied-mips-tests.log`, `work/supplied-mips-public-tests.log`), and whitespace
validation passed. The island PNG was inspected and shows the coastline/land
without the base-only experiment's rectangular patches. The final build's combined
replay again matches all four hashes exactly. No live game or replay remains from
this stage. Developer flag only; no normal-default or public checkout changes.


### Hardware filtering of preserved terrain mip images (2026-10-15)

Added developer-only `PS2_AC4_SUPPLIED_FILTER`: 0 preserves the exact reference,
1 uses hardware trilinear sampling at the original Q/K-derived LOD, and 2 uses
explicit gradients with the anisotropic sampler. Eligibility requires supplied
native levels, linear magnification and trilinear minification. Other states retain
exact sampling. Synthetic tests cover eligible modes, point/nearest-mip exclusions
and unchanged failure behavior. Normal launchers/defaults remain unchanged.

Initial major-axis gradient scaling selected finer mip artwork and reproduced the
dark patches (`combined-supplied-filter`, maximum channel delta 49). The revised
shader derives the footprint's major/minor axes and scales the minor axis to the
original GS LOD, bounding the aspect denominator at major/16. This retains the
projected direction/aspect while avoiding the erroneous finer artwork in the
sampled view. Lower configured anisotropy/device limits and temporal behavior
still need review; this is not full-game filtering acceptance.

`combined-supplied-filter-minor` retains the exact reference hash and has maximum
channel delta 3 for hardware trilinear, 6 for revised anisotropic filtering. Its
anisotropic final PNG was inspected: the conspicuous dark rectangular patches
from the rejected experiment are absent. `island-supplied-filter-minor` at 5x/eight
fields retains exact baseline hash `0bd7a3a8cbfa0f93fe6f8253cd6aaa87bd61ebc622cd664da60c5cf8b8ea7265`;
maximum deltas are 9 (trilinear) and 17 (anisotropic). The final coastline/terrain
PNG was inspected and remains intact. Deltas describe evidence, not an acceptance
tolerance or proof that every changed pixel is desirable.

New reusable `tools/test_ac4_texture_filter.py` validates a complete private capture,
uses hidden lockstep replay, records configs/logs/images, requires exact native
reference parity and equal positive supplied-level coverage with zero allocation
fallbacks. Hardware and anisotropic counters must match requested modes, and
anisotropic acceptance requires a runtime report of an enabled native anisotropic
sampler. Filtered image quality remains an explicit review step. Synthetic parser
tests reject missing/duplicate/inconsistent counters and a disabled sampler.
Private first tool run `texture-filter-usp2m704` passes 3,728 supplied draws per
mode; final sampler-evidence run and suite results follow below. No public push;
AC5 parity remains incomplete. Next evaluate multiple frames/live minification,
configuration changes, broader HUD/radio ownership and live transitions.


Final `texture-filter-0hstryh5` passes all evidence gates: exact reference hash is
unchanged, 3,728 supplied/hardware/anisotropic draws are reported in anisotropic
mode and the native Vulkan sampler reports **16.0x**. Both binaries rebuilt;
22 renderer executables and 116 public tests pass (`work/supplied-filter-tests.log`,
`work/supplied-filter-public-tests.log`); whitespace check passes. All sessions in
this stage are terminal. No FPS or whole-game visual acceptance is claimed.


### Combined live route and sampler setting lifetime (2026-10-15)

`combined-filter-display-live` (private C: evidence) completed 10,400 fields,
exit 0, with native model/terrain execution, materials, supplied mip anisotropic
filtering and automatic high-resolution screen composition enabled together.
Coverage: 6,048,640 native material/mip draws, 1,145,498 supplied anisotropic draws,
zero allocation fallbacks; 318,710 native terrain activations, 2,399,191 lit,
2,111,026 whole unlit and 702,606 mixed activations. All 18,543 original screen
helper observations match, with zero mismatch/decline/unpaired/open reports.
18,512 native screen elements close correctly; 594 pre-display copies/presentations
use the 5x 3200x4480 display allocation. The final 2560x2240 cropped screenshot was
inspected and shows flight HUD/radar and the later radio line “You are now under
my command.” No audio synchronization judgment is possible from this evidence.

This run does not prove stable 60 FPS: the interval ending at field 7200 reports
56.8 fields/s and 32 drops; the interval ending 10200 reports 59.9 with one drop.
Synthetic test compilation overlapped the run and periodic screenshots were
active, so it is not a controlled performance benchmark. No five-second no-vblank
stall was observed in this run. Earlier intermittent stalls remain unresolved.

During the run, source review found that changing anisotropy rebound every texture
to the ordinary non-mip sampler, losing native mip/clamp behavior. The separate
HUD composition/display descriptors also retained the old sampler after it was
destroyed. Fixed sampler settings as a transaction: create the ordinary sampler
and all four native wrap variants; on failure destroy only replacements and keep
old settings/descriptors; wait for GPU completion before rebinding; select the
proper native sampler by levels/clamp bits; rebind extra display targets; retire
old samplers only afterwards. Failed allocation no longer advances the cached
settings value. Anisotropy 0 now also disables it on native mip samplers, rather
than implicitly requesting 16x. Default remains 16x.

Added `tools/test_sampler_settings.py` to the renderer suite. It compiles the
actual creation/update functions against mocked Vulkan calls, checking failure
at all five allocations, failed GPU wait, unchanged-settings no-op, native mip
and per-axis wrap preservation, overlay descriptor rebinding, disable and device
limits. All **23** renderer executables pass. The live run above used the pre-fix
binary without settings changes; do not misrepresent it as an interactive
settings-toggle test. Binaries were rebuilt only after that run terminated.

`tools/test_ac4_texture_filter.py --anisotropy 4` now allows explicit lower-setting
replays. `texture-filter-95usojto` passes all coverage gates and exact GS/native
reference matching (SHA256 `9207e37e6cc506958da68a8075495487350c6069ad8e46757b92097ce2c7b992`)
with runtime sampler 4.0x. That hash differs from the 16x baseline because the
ordinary GS sampler setting also changes. Filtered output is still a visual-review
artifact, not automatic visual acceptance. Whitespace check passes. No defaults
promoted and no push. Full AC5 parity remains incomplete; next perform temporal
filtering/UI review and interactive resolution/anisotropy transitions, then extend
remaining HUD/radio producer coverage.


### Real-GPU sampler-cycle gate exposes replay history mismatch (2026-10-15)

Added serialized `ps2_vk_set_anisotropy`: validate settings, hold the renderer lock,
apply the existing transactional sampler update, and restore the requested config
on failure. Replay-only `--aniso-cycle` runs 16/off/4/16 with screenshots after each
pass; requires hidden video and four loops. Reusable
`tools/test_ac4_sampler_cycle.py` compares each pass byte-for-byte with a fresh
process at the same setting, requiring native mip and high-resolution display
coverage. Mock tests now execute the actual serialized setter and invalid/failure
paths; parser tests reject missing/out-of-order updates and missing coverage.

**GPU acceptance currently fails.** First pass matches the fresh 16x control;
passes 2..4 differ around HUD edges (maximum channel difference 96). Private
`sampler-cycle-_e899ilw` is the initial failure. A separate constant-16x/four-loop
control produces exactly the same final hash as cycle pass 4:
`7aeea22cea86f426854fceb50100685e4435b5b66d6388c67e32236a523dfeff`, versus fresh
`52cce2eccef75fc93fc2031a6be963b65c3603ee1b7db425b43398ca39b77419`. Thus the
same mismatch exists without sampler changes; do not call the transition fixed
or attribute this evidence to sampler descriptors alone.

Snapshot restore retained cross-field overlay history. Added a replay-only reset
of overlay lists/presentation flags after draining, retaining GPU allocations and
descriptors. This reduces pre-display copies from seven to the expected four
(`sampler-cycle-0v9citk_`), but **does not eliminate the image difference**. A broader
experimental target/depth metadata reset also failed (`sampler-cycle-87yjioir`) and
was removed; do not ship speculative target invalidation. Remaining investigation:
GS/texture cache epochs and ownership state restored/reused across replay loops,
including HUD content and per-field grouping. The failing checker remains strict.

No normal game/default behavior changed in this stage, no public push, full parity
remains active. The real-GPU settings gate is unresolved despite the passing
synthetic sampler tests. Preserve all failed/control artifacts for diagnosis.


### Sampler-cycle mismatch resolved: retained overlay hazard history (2026-10-15)

The remaining repeated-pass HUD difference came from `ov_hold`, the compositor's
30-frame hazard cooldown, not a texture cache epoch. Replaying the snapshot kept
that prior history and changed which regions were lifted into the high-resolution
overlay. The replay restore now clears both `ov_hold` and the temporary `ov_grid`,
as well as the already-reset overlay lists/presentation flags. Ordinary gameplay's
hazard policy is untouched; GPU texture/target allocations and descriptors remain
loaded. The actual reset helper is tested with nonzero history in every test cell.

Strict GPU acceptance now passes in private `sampler-cycle-j0zs4c6t` at the 4K
window setting: each 16/off/4/16 pass exactly equals a fresh-process control at the
same setting, including native mip textures and the high-resolution HUD display.
Hashes: 16x (passes 1 and 4)
`52cce2eccef75fc93fc2031a6be963b65c3603ee1b7db425b43398ca39b77419`, off
`35cb53f99331ee7ae8f6bb684c739b812f34736b521b63811b5234aec86514d2`, 4x
`7cbcf501c3056f9790b1506b09bd0f27abdba4d5dd911ac0764c6769ffb4b7e9`.
This supersedes the prior unresolved sampler-cycle gate; the failed artifacts are
retained as regression evidence. No tolerance was introduced and controls still
start fresh processes. Mouse/keyboard settings-menu interaction is a separate
untested gate. The checker now also supports `--height 1080` for display coverage.

Next continue temporal/live UI coverage and expose the combined experimental
renderer through a deliberate opt-in launcher preset, retaining the working
normal default. Existing launchers currently expose individual model switches but
sanitize developer-only terrain/screen settings, so captured parity alone does
not yet provide a complete user-selectable hybrid path. No push; parity remains
active and incomplete.


1080p control `sampler-cycle-5z9_webb` also passes all four exact comparisons,
including restoration to the original 16x image. Both binaries rebuilt; 23 renderer
executables and 117 public tests pass (`work/sampler-history-tests.log`,
`work/sampler-history-public-tests.log`); whitespace validation passes. All replay
sessions from this stage are terminal. Default gameplay and the public checkout
remain untouched.


### Opt-in combined hybrid launcher (2026-10-15)

Added `play.cmd -HybridRenderer`, forwarded by `build.ps1 -Action Play` to
`tools/setup_ac4.py run --hybrid-renderer`. It requires the native executable and
combines model/terrain execution, GPU meshes/materials, native model and supplied
terrain textures, anisotropic terrain filtering, native sky/cloud/sprite producers,
and automatic window-resolution screen precomposition. Saved resolution and
anisotropy remain in use. GS fallback remains available for unsupported states.
The older triangle bridge and explicit bootstrap selection are rejected when
combined with this preset; missing native builds fail instead of silently falling
back. Original capture execution overrides the preset, and ordinary play remains
unchanged. No renderer selection is persisted by the switch.

Unit tests verify all preset flags, capture override, clean ordinary-play defaults,
missing-native failure and incompatible selections. Windows acceptance executes
the actual CMD/PowerShell forwarding path with the recording helper. All **120**
public tests pass (`work/hybrid-launcher-tests.log`), and whitespace validation
passes. README and V3 notes document the opt-in command and experimental status.
This is an accessible validation preset, not a claim of completed AC5 parity.

A bounded boot test uses the actual setup.run preset environment and native binary,
with private saves/settings and a 1,800-field limit; it does not change the user's
saves. Private evidence is `hybrid-launcher-boot` under the C: evidence root. Its
command/config/log are retained. Full mission/transition validation of the public
preset remains separate from prior developer-run combined-renderer evidence.
No public checkout changes or push; goal remains active.


The bounded public-preset boot finished exit 0 after 1,800 fields. The interval
ending at 1200 fields reports 58.3 fields/s and 17 drops; it is a boot smoke test,
not locked-60 or full-mission acceptance. All sessions from this stage are terminal.


### Public preset route and native texture allocation pressure (2026-10-15)

`hybrid-launcher-flight` exited 0 at 10,400 fields, but the public preset's
`Opening=Show` changed the recorded input route. Screenshots show the attract
sequence, and there were zero screen-helper calls. This is opening/attract
stability evidence, **not Mission 1/HUD acceptance**. It reports 3,546,708 native
mip draws, 89,030 texture conversion fallbacks and intervals around 49-51 fields/s.
Those counters alone do not identify every fallback as an allocation failure or
establish its performance cause. No audio acceptance claim is made.

Code review found that the generic texture store could recycle an already-uploaded
single-level GS image for a native mip texture under pressure. Native preparation
then cannot change its Vulkan image layout and falls back. Native allocations now
use an explicit immutable-image allocation path: cold single-level images are
retired safely instead of recycled in place. Ordinary GS texture recycling is
unchanged. Owned supplied-mip bytes participate in the same budget/collection
decision as the base allocation, rather than rejecting before safe collection.

The actual allocator and native-wrap entry point are tested with a saturated
store, live-epoch protection, existing-image retirement, native cache hits and
extra mip budget exhaustion. Both binaries rebuilt. The retirement executable is
now part of the standard renderer suite. New private live evidence is
`hybrid-launcher-mission-cache`, using the preset with only the recorded route's
`GameDefault` opening override and isolated saves/settings. The run reached Mission 1 and exited 0 at 10,400 fields. It recorded 6,517,412
native mip/material draws, 1,205,130 supplied anisotropic mip draws, zero texture
conversion fallbacks, 19,411 accepted/closed screen groups, and 623 native display
presentations (5x target 3200x4480). Final HUD/radar/caption image was inspected.
Reported intervals remained about 49-51 fields/s; no locked-60 claim. Because the
route differs from the attract test, these counts are not a controlled before/after
performance comparison.
This fix does not by itself prove that every reported conversion fallback is gone.


The rebuilt supplied-mip GPU check `texture-filter-5t14ljkn` passes at 5x: exact
native storage equals the GS reference byte-for-byte (SHA256
`02238f318e679363580fb04fb950c6f9c6401a55c5679a281bd0f53cb815fc8f`), with 3,728
supplied draws in each native mode and zero allocation fallbacks. This confirms
preserved captured sampling after the allocator change. 24 renderer executables
and 122 public tests pass; the saturated-store test also passes with the actual
native-wrap entry point. Evidence logs: `work/native-cache-renderer-tests.log`,
`work/native-cache-public-tests.log`.

Added `tools/test_ac4_hybrid_route.py` with an isolated save/settings wrapper around
the actual launcher preset. Its coverage gate rejects normal-exit runs that never
reach native HUD/display output or lack model/terrain/sky/cloud/filter coverage.
Texture fallbacks and interval rates are retained, not interpreted as a visual,
audio or performance pass. The opening/attract result correctly fails this gate.
A separate 1080p route with two scripted view changes and a late DMA capture is
underway in `hybrid-launcher-camera-1080`; do not count it until reviewed.


`hybrid-launcher-camera-1080` exited 0 at 12,200 fields and passed the combined
coverage gate: 10,210,812 GPU mesh draws, 6,659,200 native material/mip draws,
2,279,022 supplied-mip draws, zero allocation fallbacks, 37,405 accepted closed
screen groups. The screenshot sequence demonstrates caption transitions and
pause/Return to Game. Interval rates were roughly 55-57 fields/s, still not locked
60. The scripted R3 presses did **not** establish camera changes; screenshots stay
in HUD view. Do not count this run as cockpit/third-person acceptance. A corrected
D-pad Down route is isolated in `hybrid-launcher-views-1080` with a late capture.


### Combined preset camera transitions and fresh third-person oracle (2026-10-15)

`hybrid-launcher-views-1080` completed 13,400 fields, exit 0. Corrected D-pad Down
presses at fields 12020 and 12800 produced a visible cockpit (saved field 12300)
and third-person aircraft (12900), with HUD/radar and changing radio captions.
The combined gate passes: 11,663,849 native GPU mesh draws, 7,210,116 material/mip
draws, 2,597,904 supplied mip draws, zero texture conversion fallbacks, 54,448
accepted/closed screen groups and no arena refusals. Screenshot review verifies
those visible scenes, not all objects or all frames. Rates were about 52-54 fields/s
with one 48.7 interval; prior faster 1080p results are not a controlled comparison.
The original save/settings files remain untouched.

Its complete four-field capture at 13100 was replayed at 3x in all eleven modes
(`model-gpu-parity-av50b48b`). Every final image is byte-identical, SHA256
`958a675a98ef2070d3a70856665916c70995299288e1746d0e86e12391c0cf6b`.
Coverage: 2,024 GPU model draws, 924 secondary model draws, 6,704 primary and 6,704
secondary terrain draws; 8,728 native materials in combined mode. All 3,796 lit
activations/packets and 1,900 terrain activations/packets match the original state,
including memory/registers/flags/continuation; native execution replaces 1,394,560
lit and 740,272 terrain instruction pairs. Whole-packet bypass covers 3,048 packets
and 97,964 qwords. This fresh third-person oracle supplements the older fixtures.
The cockpit screenshot is live visual evidence, not a separate cockpit replay oracle.

Final allocator review retains a pre-allocation guard for individually oversized
supplied mip chains. The actual supplied-mip entry point now has direct synthetic
coverage for full-store collection, live source protection, copied level bytes,
cache hits/accounting and rejecting an oversized chain before allocating or reading
pixels. This test passes; both executables rebuilt after that final guard.

Remaining: isolate cockpit/ground-object native coverage, review broader HUD/radar
producer placement and scene transitions, then decide whether the bounded evidence
is sufficient to promote the experimental preset. Full-game/locked-60/audio/focus
acceptance is not inferred from these results. No public push. The parity goal
remains active; this milestone is not a claim that every AC5 capability is accepted.

Ghidra discovery during this stage found a headless bridge with no loaded program;
project listing requires GUI mode. Static generated-code inspection remains
available: the screen helper has 47 direct call sites across 19 functions (private
`screen-helper-callers.json`). No speculative reanchoring/classification was applied.


### Caption ownership evidence and HUD placement scope (2026-10-15)

The existing overlay-region diagnostic now optionally reports draw bounds, texture,
vertex count and lift decision (`PS2_RN_OV_DETAIL=1` with `PS2_RN_OV_REGION`). It
counts ordinary trailing GS draws separately from owned native 2D groups. Rendering
and eligibility decisions are unchanged; mesh records remain excluded from the
ordinary-vertex diagnostic. This closes a measurement blind spot: caption font
batches do not pass through the sprite helper.

Private `screen-caption-tail` replays the complete two-field flight capture with
1x scene and 5x overlay/display. In both fields, draw 1223 (speaker name, texture 17,
12 vertices, bounds 224,30.5 to 277,48) and draw 1224 (caption glyph batch, texture
18, 504 vertices, bounds 141,48.5 to 373,89.5) are explicitly lifted. This is direct
native-resolution caption coverage; a visible caption alone was weaker evidence.
The renderer's safe-tail policy, inherited from AC5, permits these original GS
primitives to use the native overlay without a separate string/font rewrite.

Static group inventory found 47 sprite-helper call sites, and this capture exercises
26 return addresses. HUD placement remains a distinct issue: AC4 conservatively
sets RN_G_WORLD to preserve original positions, while AC5's centered-HUD option can
reanchor classified screen groups. AC4's widescreen patch currently changes only
projection constant 0040DACC. Do not claim that centered-HUD layout parity is
implemented. Classification must include matching lines/digits/radar components,
while leaving world-tracking target markers attached to their projected positions.


### Cockpit coverage and ordered radar mask prototype (2026-10-15)

Cockpit geometry still uses fallback. The four-field cockpit full-model gate
(model-gpu-parity-0i0wadgd) correctly fails zero verified native model draws.
Program baf5487804fd6eeb has real instruction differences and continuation 4224
instead of 4288; do not alias it to the supported lit model. Program
c767ef6355cbd14e also remains unclassified for native replacement.

Terrain image 758bae290dd231b2 differs from supported 6632fc9c7a80a952 only in
stale bytes beyond the used [0,4192) body. Added this exact whitelist identity,
retaining entry/continuation validation. Tests reject adjacent hashes and entry
4224. Cockpit terrain-only gate model-gpu-parity-v2g5wo3l passes seven exact modes:
2088 activations/packets, 818304 instruction pairs, 7440 primary and secondary
terrain draws, full register/memory/flags/packet-order agreement, 80136 GIF qwords
bypassed. This is terrain support in cockpit view, not native cockpit geometry.

Direct diagnostics found radar DATE icon draw 1214 ending the safe overlay tail.
Most radar stayed at scene resolution although captions lifted. Experimental
PS2_AC4_SCREEN_MASK=1 now allows otherwise-safe DATE only in the ordered tail,
with a high-resolution destination snapshot immediately before each masked draw.
Earlier independently reordered groups still reject DATE. Colorless mask writes
stay in the ordered sequence. Snapshot resources must exist before eligibility;
sampler changes rebind the snapshot descriptor too. The hybrid preset does not
enable this experiment. Centered-HUD placement is also still incomplete.

New tools/test_ac4_screen_mask.py CAPTURE --fields N --output-root PRIVATE_DIR
compares GS, existing overlay and mask modes. It requires complete visible captures,
positive snapshot execution, and byte-identical output at scale 1. Two loops test
warm allocation after replay hazard-state reset, not live startup/resize behavior.
--scale 3 permits visual review, not equality against differently sized GS output.

Private C: evidence:
- screen-mask-ejejyak3: flight, three modes exact, two snapshots;
  SHA256 02618856d1d22d3ec76c3795dc44f29ed4ae4d1fd5afcb346fcf0a0b286c4921.
- screen-mask-ddheksfa: cockpit, three modes exact, eight reported snapshots;
  SHA256 6ad77469b9a566e2767b9dd15408dbc16118ef35fbeb4ae241c72a6a18d9c2bf.
- screen-mask-x1za5y3z: third person, three modes exact, eight reported snapshots;
  SHA256 792a31f30ec8a444dc294bdb5dd471fbbddbe3031348e74840bb5a1261c69b85.
- screen-mask-uz9eiyds: 3x overlay visually reviewed; sharper radar lines and
  player icon present, all seven radar batches 1210-1216 lifted in warm loop.
  Frame corners/filtering and moving enemy icons still need review.

Both binaries rebuilt. All 24 renderer test executables pass, including mask
resource/predicate restrictions and snapshot sampler rebinding. Before promotion:
combined native geometry/material captures, live movement/transitions, snapshot
allocation failure during resize with pending lists, and full-overlay copy cost.
Ordinary play and public source unchanged; full AC5 parity remains incomplete.

### Combined radar/native acceptance and field lifetime (2026-10-15)

The mask harness now accepts --native-geometry. It adds native model/terrain,
material, sky/cloud/sprite and VU replacement paths while retaining original
texture filtering for exact comparison. It requires positive terrain GPU coverage
as well as actual mask snapshot execution. Final four-field third-person run:
E:/ac4-renderer-evidence/screen-mask-sd7seh5_ passes GS, existing overlay, mask,
and combined native-mask with identical SHA256
792a31f30ec8a444dc294bdb5dd471fbbddbe3031348e74840bb5a1261c69b85.
Combined coverage includes 13408 primary/secondary terrain draws, 4048 model draws,
and 17456 native material draws. This is composition compatibility, not a new
full-state VU proof or temporal filtering acceptance.

A same-field-only mask restriction was tested and rejected: AC4's copy can consume
HUD accumulated during the preceding field. That restriction produced correct
fallback images but zero mask execution, which the acceptance tool rejected.
Final implementation retains cross-field lists and additionally requires the AC4
pre-display path. Do not reintroduce same-field-only lifting as a safety fix;
resize/OOM handling must preserve pending lists and matching mask resources.
The experiment is still not enabled in the public hybrid preset.

Both binaries rebuilt. All 24 renderer executables passed before removing the
failed same-field restriction; the changed target-policy executable then passed
for AC4 and AC5 again, and final combined capture passed as above.

Storage: C: filled during validation; truncated run screen-mask-1_kpbxck and that
interrupted compiler run are invalid evidence. New output/TEMP/TMP use
E:/ac4-renderer-evidence (permission required per turn). Public/source stay on F:.
Generated screenshot hybrid-launcher-mission-cache/shot_000007.ppm was verified
byte-for-byte through gzip before removing the uncompressed file. Its valid archive
is E:/ac4-renderer-evidence/hybrid-launcher-mission-cache-shot_000007.ppm.gz, with
another copy at work/recovered-evidence.ppm.gz. The attempted C: .ppm.gz copy is
incomplete and must not be used. No game data or saves were removed. Existing
C: evidence directories were marked for NTFS compression; that did not recover
usable free space. Do not run new large captures there.

### Atomic overlay resources and live radar route (2026-10-15)

Overlay target replacement now allocates color, depth and optional DATE snapshot
as a transaction before rebinding descriptors or destroying old images. If a
window-size change fails while logical GS dimensions remain unchanged, the old
resolution and pending cross-field mask resources remain usable. Failed GPU waits
also preserve them. If logical GS dimensions change and allocation fails, overlay
is disabled and subsequent native 2D stays in GS; seamless recovery under that
condition is not claimed. Repeated failed resize allocation attempts may still
need backoff. The experimental mask path remains disabled in the public preset.

New test_overlay_targets.py executes the actual replacement/preparation functions
with mocked GPU allocation failures. It covers each image allocation, descriptor
failure, successful commit/reuse, retaining old dimensions on window resize failure,
failed waits, and rejecting mismatched logical GS target reuse. All 25 renderer
executables pass (work/radar-target-transaction-tests.log). Both game and replay
binaries rebuilt (work/radar-target-transaction-build.log).

The prior mask build completed a 13400-field live combined hybrid route with
private saves/settings, 1920x1080 window setting and automatic 3x overlay:
E:/ac4-renderer-evidence/hybrid-radar-live. Exit 0; cockpit at field 12300 and
third person at 12900 were visually reviewed in cockpit.png/third-person.png.
Radar lines, moving friendly aircraft icons, player icon and changing captions
are visible in both cameras. Coverage: 18489585 mesh draws, 10418959 native
material/mip draws, zero texture allocation fallback, zero arena overflow;
85347 overlay draws over 3597 composition frames and 2791 native display copies.
This run preceded the atomic resource change and is not live resize validation.
Concurrent synthetic compilation makes interval rates unsuitable for benchmarking.

After the resource change, E:/ac4-renderer-evidence/screen-mask-ru375unv passes all
four modes including combined native geometry/materials, with identical original-
resolution SHA256 792a31f30ec8a444dc294bdb5dd471fbbddbe3031348e74840bb5a1261c69b85.
Mask snapshots now also report total count/bytes at shutdown for measuring copy
cost; the first-eight diagnostic lines are not a total-coverage counter.
Remaining: live resize/transition testing on the final build, high-resolution edge
review, centered-HUD placement, and distinct native cockpit model adaptation.
Full AC5 parity is not complete; ordinary defaults/public checkout unchanged.


### Native cockpit direct mesh subset (2026-10-15)

Added an independent observer for exact cockpit resident program baf5487804fd6eeb,
entries 16/4224. It does not alias the existing lit program, change guest memory,
or activate its native execution state. Direct input vertices must be inside the
homogeneous clip volume and match the original packet's address/header/count.
The cockpit kernel preserves its extra MULi.z by 16 before common FTOI4.
After excluding clipping-generated packets, all 7536 vertices in 2512 packets
across 2412 activations match XYZ/fog/UV/color exactly; 1564 packets are skipped.
Initial UV/color mismatches came from inappropriate direct input correspondence
on clipped packets; these packets remain fallback, not silently marked supported.

PS2_AC4_COCKPIT_AUDIT=1 observes; PS2_AC4_COCKPIT_MESH=1 additionally prepares
verified direct primary triangles when ordinary model meshes are enabled. The
shader uses a separate cockpit depth flag while retaining ordinary lighting mode.
GS draw matching still verifies every attribute/state before replacing geometry.
Only the experimental flags enable this; public hybrid and ordinary defaults are
unchanged. Original cockpit VU execution remains active, and clipping/secondary
passes have not been replaced.

Both binaries and mesh.vert.spv rebuilt. All 25 renderer executables pass
(work/cockpit-mesh-tests.log), including observer memory immutability, exact hash
and entry restrictions, deliberate UV mismatch detection, depth scale, and no
cockpit shader-mode leakage into ordinary model draws. Two evidence-validator
unit tests reject absent/zero/contradictory coverage reports.

New tools/test_ac4_cockpit_gpu.py requires complete visible captures, positive
exact direct-vertex evidence and actual GPU draws in GPU modes. Material mode
also requires native material coverage without arena overflow. At scale 3,
E:/ac4-renderer-evidence/cockpit-gpu-u50_3ol2 passes GS, observer, mesh and material
modes with identical SHA256
b6a2ae177072e2424e4c7129b6444ab7d64730d4d4c9e1bc08788ebbac88ba09.
Each GPU mode has 2172 cockpit draws; material mode uses mesh.frag for all 2172
(manually confirmed before adding the automatic material coverage check).
At scale 1, private cockpit-observer and cockpit-mesh outputs also match SHA256
6ad77469b9a566e2767b9dd15408dbc16118ef35fbeb4ae241c72a6a18d9c2bf.

Existing combined native geometry/radar regression also passes all four modes
with the changed shader: E:/ac4-renderer-evidence/screen-mask-eukf82p1, SHA256
792a31f30ec8a444dc294bdb5dd471fbbddbe3031348e74840bb5a1261c69b85.
Next cockpit work: secondary pass, clipped polygons, then complete VU state and
instruction/packet-order proof before bypass. Do not count this subset as full
cockpit replacement or AC5 renderer parity.


### Cockpit secondary material pass (2026-10-15)

The independent cockpit kernel now compares the secondary environment-coordinate
pass, with cockpit depth scaling preserved. It verifies all 7536 secondary vertices
in the direct subset exactly: zero position/UV/color/fog mismatches, header failures
or rejected vertices. The mesh adapter inherits the cockpit depth flag into the
secondary candidate without leaking it into subsequent ordinary aircraft draws.
Clipped inputs and original VU execution remain unchanged/fallback.

At scale 3, tools/test_ac4_cockpit_gpu.py with --secondary passes GS, observer,
mesh and native-material modes. Evidence: E:/ac4-renderer-evidence/cockpit-gpu-r9ddqqkb.
Every mode is byte-identical, SHA256
b6a2ae177072e2424e4c7129b6444ab7d64730d4d4c9e1bc08788ebbac88ba09.
There are now 3436 cockpit GPU draws: 2172 primary and 1264 secondary; material
mode records all 3436 with mesh.frag and no arena refusal. The acceptance tool
requires positive verified secondary vertices and actual secondary GPU draws;
reported secondary coverage cannot exceed the primary verified packet/draw count.

Both executables rebuilt (work/cockpit-secondary-build.log). All 25 synthetic
renderer executables pass (work/cockpit-secondary-tests.log), including independent
secondary depth/UV/color values, deliberate secondary corruption detection, paired
shader mode 5/6 and normal-model mode restoration. Three Python evidence tests
reject missing, mismatched and inconsistent secondary coverage. Existing combined
geometry/material/radar regression remains exact in all four modes:
E:/ac4-renderer-evidence/screen-mask-4sb64mpx, SHA256
792a31f30ec8a444dc294bdb5dd471fbbddbe3031348e74840bb5a1261c69b85.

Cockpit mesh remains opt-in through PS2_AC4_COCKPIT_MESH with model meshes enabled.
Next: cockpit clipping uses a different plane-selection order and omits the lit
program's 0.001 intersection bias. Adapt and compare its complete clipped packets
and scratch state before attempting whole-program VU/GIF bypass. Full parity and
full cockpit native execution are still unaccepted; defaults/public source unchanged.


### Cockpit clipped polygons and trivial rejection (2026-10-15)

The cockpit clipping kernel uses unbiased intersections, quantized polygon records,
original primary/secondary GIF headers, and the distinct cockpit Z conversion.
All 1564 clipped/rejected packets now match the reference byte-for-byte, including
all 80 scratch qwords checked against predicted writes or the preceding observed
scratch state: zero packet/scratch mismatches and zero declines; 844 are empty.
This per-packet scratch proof is not yet whole-activation register/state proof.

An initial 792 scratch mismatches were traced to trivial frustum rejection before
scratch writes. Modeling that path as an empty packet with no scratch changes
resolved them. Qualification to the preceding note: the reordered FCOR checks are
trivial-rejection plane tests; the captured polygon clipping works with the same
axis clipping order. Do not infer that polygon plane order itself must change.

Verified nonempty polygons now submit native primary/secondary triangles in GS
queue order, preserving fan/strip indices and ADC suppression. Queue entries carry
explicit cockpit/material/clipped identity; terrain entries reset cockpit identity.
No VU instructions are bypassed by this addition, and the public preset stays as-is.

Acceptance tool --secondary --clipping requires exact positive packet/scratch and
secondary evidence plus actual clipped/secondary GPU draws. At 3x, private
E:/ac4-renderer-evidence/cockpit-gpu-l0a0i8f1 passes all four modes, SHA256
b6a2ae177072e2424e4c7129b6444ab7d64730d4d4c9e1bc08788ebbac88ba09.
At 1x, final build cockpit-gpu-elxxfv3l passes all four modes, SHA256
6ad77469b9a566e2767b9dd15408dbc16118ef35fbeb4ae241c72a6a18d9c2bf.
Both report 4708 native cockpit draws, including 1728 secondary and 1272 clipped
(these categories overlap); native material mode covers all 4708.

Both binaries rebuilt (work/cockpit-clipping-build.log). All 25 renderer checks
pass (work/cockpit-clipping-tests.log): clipping/intersection output, rejection
without scratch writes, polygon fan order, ADC suppression, atomic invalid tag
rejection, cockpit/terrain queue reuse and secondary identity. Four Python evidence
validator tests pass. Final combined geometry/material/radar regression remains
exact: E:/ac4-renderer-evidence/screen-mask-jpqmci7d, SHA256
792a31f30ec8a444dc294bdb5dd471fbbddbe3031348e74840bb5a1261c69b85.

Next: complete cockpit activation memory, integer/vector/scalar/flag pipeline,
instruction counts and continuation before native VU/GIF bypass. Also retain the
broader live/resize/HUD placement parity gates. Full AC5 parity remains incomplete.


### Cockpit activation memory and integer validation

The opt-in MODEL_PLAN cockpit observer now predicts all 16 KB of VU memory
from the activation snapshot, including clipping scratch, packet output and
preserved regions. It checks memory at each kick and completion, independently
tracks packet order, and checks all integer registers plus the final return
address/running state. This is validation, not native VU execution bypass.

Captured acceptance with --secondary --clipping --state-plan passed in GS,
observer, native mesh and native material modes:
E:/ac4-renderer-evidence/cockpit-gpu-6ekm4g6c.
Each observer mode reports 2412 begun/ended activations, 4076 packets, zero
memory/integer/sequence mismatches and zero declines. Mesh/material modes have
4708 cockpit GPU draws. All four final images share SHA256
6ad77469b9a566e2767b9dd15408dbc16118ef35fbeb4ae241c72a6a18d9c2bf.

Five evidence-validator unit tests pass, including missing, duplicate,
incomplete and mismatched state reports. All 25 renderer executables pass;
log: E:/ac4-renderer-evidence/cockpit-state-tests.log. Synthetic cockpit checks
inject unrelated memory corruption, integer changes, a wrong packet address,
and a wrong return address. The runner accepts --work-dir for temporary
binaries on a volume with available space.

Correction to earlier disassembly notes: the cockpit and lit programs have the
same FCOR plane-test mask order (1, 2, 4, 8, 16, 32 at PCs 0x238 through 0x288).
The apparent reordering was a diff-alignment artifact. Branch displacements
change with removed bias instructions; cockpit Z scaling and continuation
still differ. Do not alias the two resident signatures.

Remaining: vector registers/accumulator, scalar and flag pipelines, instruction
counts, then guarded native VU/GIF bypass. Captured memory/integer success does
not establish those states or whole-game coverage. Live resize and coherent
window-resolution HUD placement remain separate parity requirements. Public
preset and ordinary defaults remain unchanged.


### Cockpit scalar state follow-up

Scalar prediction now covers Q/P, pending divide values, readiness, I, random
state and clip bits at every kick and activation completion. Empty polygons
reuse the independently computed clipping trace for retained Q and I state.
The captured run has 4076 packets and 2412 completed activations with zero
scalar mismatches or declines. --state-plan now requires this coverage too.
Final four-mode acceptance: E:/ac4-renderer-evidence/cockpit-gpu-3ttlfa5i;
all images remain exactly equal with the preceding SHA256. Both executables
rebuilt; E:/ac4-renderer-evidence/cockpit-scalar-build.log records the build.
All 25 renderer checks and five evidence-validator tests pass after this change.
Synthetic tests additionally inject Q, pending Q, I and random-state corruption.
Vector/accumulator, flags and instruction counts remain to be validated before
cockpit execution bypass. This does not change the default renderer.


### Cockpit vector and accumulator state

The cockpit observer now predicts every vector register and the accumulator
at each packet and activation completion. The lit visible-register helper is
shared through explicit state and memory parameters, without sharing mutable
prediction state. Cockpit output retains its distinct projected Z values.
Clipped-away polygons must seed the clipping trace with the independently
tracked 85-qword scratch window: substituting zero scratch produced 60 packet
and 24 completion mismatches. Retaining predicted scratch removes these errors.

Final capture: E:/ac4-renderer-evidence/cockpit-gpu-yu9hjhhl. All four modes
are image-exact (SHA256 6ad77469b9a566e2767b9dd15408dbc16118ef35fbeb4ae241c72a6a18d9c2bf).
Memory/integer/scalar/vector reports each cover 4076 packets and 2412 completed
activations without mismatches or declines. The --state-plan gate now requires
vector/accumulator coverage as well. Both binaries rebuilt; build log is
E:/ac4-renderer-evidence/cockpit-vector-build.log.

All 25 renderer tests and five evidence-validator tests pass. Synthetic tests
supply independently constructed vector results and inject corruption into a
preserved vector register and the accumulator. Combined native geometry and
ordered radar-mask regression also passes all four modes:
E:/ac4-renderer-evidence/screen-mask-ge01vrkp, SHA256
02618856d1d22d3ec76c3795dc44f29ed4ae4d1fd5afcb346fcf0a0b286c4921.

Next: arithmetic flag pipeline and exact instruction counts before cockpit
VU/GIF bypass. This is still opt-in observation/mesh work, not full AC5 parity
or a change to the working default. Broader live coverage and HUD placement
remain separate requirements.


### Cockpit instruction-count validation

The observer now predicts total executed instruction pairs through entry,
visible loops, winding, split classification, clipping and empty exits.
Cockpit clipping retains one entry padding pair and one extra pair per edge
crossing relative to the unlit schedule. Empty output uses the lit/cockpit
exit tail (two additional pairs plus packet-bank parity). Crossing counts
are derived from the clipping geometry, not observed execution counts.

Captured final evidence: E:/ac4-renderer-evidence/cockpit-gpu-z3lna8td.
All 2412 completed activations match instruction counts with zero declines.
All memory/integer/scalar/vector checks remain exact across 4076 packets;
GS, observer, mesh and material images remain byte-identical, SHA256
6ad77469b9a566e2767b9dd15408dbc16118ef35fbeb4ae241c72a6a18d9c2bf.
Both binaries rebuilt (E:/ac4-renderer-evidence/cockpit-cycle-build.log).
All 25 renderer checks and five evidence tests pass. The direct synthetic
fixture independently expects 306 pairs and detects a one-pair discrepancy.
The --state-plan acceptance gate now also requires complete exact cycle counts.

Remaining state gate: arithmetic flags and delayed flag-pipeline slots.
Do not enable cockpit VU/GIF bypass until that gate passes. Original VU
execution and working defaults are unchanged; full AC5 parity remains active.


### Cockpit arithmetic flag pipeline

The cockpit observer now schedules arithmetic flag writes through the split
prelude, trivial reject path, full polygon clipping and visible lit loop.
Clipping uses its own entry/crossing schedule and stops flag prediction before
the unlit projection loop; cockpit then uses its lit schedule. The cockpit-only
MULi.z at visible pair 52 writes the Z lane before pair 53's W-lane write.
Completion compares MAC, status, slot index and every delayed pipeline entry,
including stale payload in drained slots. It does not assume sticky saturation.

Final captured gate at 3x: E:/ac4-renderer-evidence/cockpit-gpu-u6wyzlx3.
All 2412 activations match flags and instruction counts; 4076 packets match
memory and integer/scalar/vector state without declines. All four image modes
are byte-identical, SHA256
b6a2ae177072e2424e4c7129b6444ab7d64730d4d4c9e1bc08788ebbac88ba09.
All 25 renderer checks and five evidence-validator tests pass. Synthetic checks
verify the extra Z-lane instruction's zero/sign MAC flags and drained slot.
The --state-plan gate now requires complete exact arithmetic flag evidence.
Build and test logs: E:/ac4-renderer-evidence/cockpit-flag-build.log and
E:/ac4-renderer-evidence/cockpit-flag-tests.log.
Combined native geometry/radar regression passes all modes:
E:/ac4-renderer-evidence/screen-mask-kan_hjip, SHA256
02618856d1d22d3ec76c3795dc44f29ed4ae4d1fd5afcb346fcf0a0b286c4921.

Next: construct an atomic complete cockpit activation plan, replay ordered
packet kicks and commit the predicted continuation only for the exact supported
resident signature/layout. Add rejection/immutability tests and compare actual
native execution with the captured original. These passing observer checks do
not yet demonstrate cockpit VU/GIF bypass or whole-game coverage. Keep broader
live/resize/HUD placement gates and retain the working default.


### Opt-in native cockpit activation execution

PS2_AC4_COCKPIT_NATIVE=1 now enables a complete CPU-native activation plan for
the exact cockpit signature/entry accepted by the observer. It implies cockpit
mesh preparation. All packets, scratch/winding writes, continuation registers,
flags and instruction counts are predicted before the first guest write or
kick. Unsupported layouts, nonfinite input, invalid predictions and insufficient
instruction budgets return to original execution. The native path preserves
ordered original-format packet kicks and GS fallback side effects. Dedicated
cockpit GIF decoding bypass is not yet established by this work.

Synthetic tests cover a known 306-pair activation, final memory/register state,
correct two-pass packet depth, instruction-budget rejection, signature rejection,
and invalid-input rejection without memory/register changes or submitted kicks.
All 25 renderer executables and six evidence-validator tests pass. Build/test
logs: E:/ac4-renderer-evidence/cockpit-native-build.log and cockpit-native-tests.log.

Acceptance --native adds actual execution and material execution modes, requires
positive complete native coverage equal to the original observer, and compares
all six images. At 1x: E:/ac4-renderer-evidence/cockpit-gpu-17juhlir, SHA256
6ad77469b9a566e2767b9dd15408dbc16118ef35fbeb4ae241c72a6a18d9c2bf.
At 3x: E:/ac4-renderer-evidence/cockpit-gpu-865v5yd8, SHA256
b6a2ae177072e2424e4c7129b6444ab7d64730d4d4c9e1bc08788ebbac88ba09.
All modes match. Each native mode executes 2412 activations / 4076 packets,
replacing 2239844 original instruction pairs, and produces 4708 native draws
(1728 secondary and 1272 clipped, overlapping categories). This is captured
correctness evidence, not a measured live performance improvement.

Next: broader live/multiple-view validation and guarded cockpit GIF bypass,
then promote only validated features to the hybrid preset. Continue remaining
window-resolution HUD placement/resize gates. Ordinary defaults and public
checkout remain unchanged; AC5 capability parity is not complete.


### Cockpit guarded GIF bypass coverage

The existing preflighted mesh GIF parser already accepts verified cockpit
packets when general model auditing is disabled. Added cockpit-specific packet
identity/counters rather than duplicating the decoder or relaxing its guards.
Pending cockpit identity includes direct and clipped queues and resets on
ordinary model/terrain reuse. Unsupported tags retain generic parsing.

Acceptance --native --gif now compares eight modes, including native-GIF and
native-GIF-material, with positive bounded cockpit-specific bypass coverage.
At 1x: E:/ac4-renderer-evidence/cockpit-gpu-p5xpuzjj, all modes exact, SHA256
6ad77469b9a566e2767b9dd15408dbc16118ef35fbeb4ae241c72a6a18d9c2bf.
At 3x: E:/ac4-renderer-evidence/cockpit-gpu-s01wigq0, all modes exact, SHA256
b6a2ae177072e2424e4c7129b6444ab7d64730d4d4c9e1bc08788ebbac88ba09.
Fast modes bypass generic parsing for 3028 cockpit packets / 57480 qwords.
All 25 renderer checks and seven evidence-validator tests pass. Existing parser
checks compare all GS writes, Q latch and GIF continuation against the generic
parser, including wrapped two-pass packets and atomic malformed-tail rejection.
Build/test logs: E:/ac4-renderer-evidence/cockpit-gif-build.log and cockpit-gif-tests.log.

Broader live validation is recorded separately below. Native cockpit is still
opt-in and no public/default preset promotion has occurred.


### Live cockpit execution/GIF route

Private E:/ac4-renderer-evidence/hybrid-cockpit-native-live completed 13400
fields and exited 0. The driver used the actual hybrid launcher plus opt-in
cockpit native and radar mask paths, isolated saves/settings, and scheduled
camera changes at 12020 and 12800. The existing unrelated workload was left
running. acceptance.json passes the hybrid route coverage checks and positive
cockpit execution/GIF/GPU checks. Manually inspected cockpit.png and
third-person.png: cockpit structure, player aircraft, HUD, radar and radio
captions are present. These still images do not prove motion or audio sync.

Cockpit: 243612 native activations, 411676 packets, 226224244 instruction pairs
replaced; 305828 GIF packets / 5805480 qwords bypassed; 475508 GPU draws.
Combined: 18605938 native mesh draws, 10280774 material/mip draws, 5417430 supplied
mip draws, zero arena refusals and zero texture-allocation fallbacks. Unsupported
texture states still use fallback. Screen route has 88891 accepted elements.
Recorded 600-field intervals range from 48.9 to 54.1 fields/s under this load;
this is not locked-60 or an isolated before/after performance benchmark.

Next: promote validated cockpit features through an explicitly tested hybrid
preset, broaden scene coverage, and complete remaining HUD placement and live
resize validation. No ordinary-default/public-checkout changes or pushes made.


### Hybrid preset integration

The opt-in play.cmd -HybridRenderer preset now enables COCKPIT_NATIVE,
COCKPIT_MESH and SCREEN_MASK alongside the existing geometry/material/mip/HUD
features. This matches the explicitly enabled features in the preceding
13400-field live validation. Ordinary play and VU capture remain unchanged,
including when these environment flags are inherited from a developer shell.
All 130 public unit and Windows launcher acceptance tests pass:
E:/ac4-renderer-evidence/hybrid-cockpit-preset-tests.log.

The reproducible hybrid route tool now accepts --cockpit-views with --fields
13400 (or higher), using the established Mission 1 recording and camera changes
at 12020/12800. It requires positive unique cockpit native/GIF/GPU reports with
consistent packet totals. Missing paths and inconsistent reports fail unit
tests. The preceding live log passes the new gate, saved as
hybrid-cockpit-native-live/preset-route-coverage.json. No additional live run
or performance improvement is claimed for this launcher-only integration.

Next renderer gap: coherent centered HUD placement. The verified sprite helper
at 0x15FEE0 has 47 callsites across multiple HUD producers; its packet queue
currently marks all as RN_G_WORLD. Do not clear that flag globally: world target
markers must retain projection alignment, and fixed labels/lines/radar need a
consistent anchor. The Ghidra connector currently reports no program loaded;
generated local functions remain available for analysis. Live resize and broader
missions remain acceptance gaps. No public checkout changes or push occurred.


### HUD placement producer evidence

Captured layout logging was replayed for flight, cockpit and third-person views:
E:/ac4-renderer-evidence/hud-producer-map/{flight,cockpit,third}. Each directory
contains layout.txt/json, the original frame and a labelled sprite crop sheet.
Flight has 26 observed callsites, cockpit 25 and third-person 21. Repeated
replay loops are observations, not additional scene coverage.

Visual/source correspondence (not yet enabled as placement rules):
- 001633C8: TIME producer; sprite returns 00163660, 00163734, 0016378C.
- 00162958: score/PTS producer; 00162A54, 00162AB4, 00162C90.
- 00163818: weapon labels/symbols; 0016392C through 00163BBC observed.
- 00163C08: DMG label and percent symbol; 00163D5C, 00163DBC.
- 00165AB8: projected FRIEND marker labels at return 00166488; preserve world projection.
- Speed/altitude label/arrow sites also observed, but associated lines and values
  must be grouped before changing placement.

Generated source confirms TIME, PTS and weapons call both 0015FEE0 (sprites)
and 0015FA18 (numeric glyphs). Clearing RN_G_WORLD only for the sprite returns
would separate labels from their numbers. Next implementation should observe
complete higher-level producer packet ranges, validate their ownership/lifetime,
and transform a whole fixed group together. Radar and radio-caption grouping
remain separate, and world-marker producers must stay excluded. No HUD movement
was enabled from these partial observations.

Added tools/analyze_ac4_hud_layout.py to parse private PS2_RN_2D_LOG files,
join generated sprite return sites to parent functions, and preserve per-frame
bounds/emitter/native observations. It deliberately makes no automatic fixed-vs-
world classification. Tests cover malformed/nonfinite bounds, changing bounds,
and function-boundary mapping. All 113 public unit tests pass via the standard
runner (E:/ac4-renderer-evidence/hud-layout-tests.log). Direct unittest discovery
without the tools search path failed import; the documented runner supplies it.
Ghidra's server reports no loaded program and project listing unavailable in
headless mode, so this analysis used local generated source and captured pixels.


### Fixed HUD allocation census (2026-10-15)

Added opt-in PS2_AC4_HUD_GROUP_AUDIT=1 observer with exact AC4 producer
signatures, paired entry/exit hooks, bounded allocation snapshots and an atomic
parser for inline NEXT/DIRECT payloads. Mutable DMA links are excluded from
payload descriptors. No guest memory writes, draw ownership or HUD relocation
are introduced. Default play and the hybrid preset do not enable this audit.

Private live route: E:/ac4-renderer-evidence/hud-group-live, 10,400 fields,
normal exit. Each producer was called 604 times:
- PTS 00162958: 604 matched, 4,228 inline packets.
- TIME 001633C8: 604 matched, 3,624 inline packets.
- Weapons 00163818: 604 matched, 6,644 inline packets.
- TGT 00162D98: 604 matched, 604 inline packets.
- DMG 00163C08: all 604 declined; zero claimed coverage.
All hooks balanced: zero open, zero unpaired. Full five-producer coverage gate
correctly fails this run. It must not be presented as centered HUD acceptance.

Generated DMG source calls 00104338 at return 00163DF8. That allocator emits a
DMA CALL (0x50000000 | qwc) and an empty NEXT link (0x20000000), pointing at an
external chain. The inline-only preflight deliberately rejects this layout.
Do not simply skip those records and call the producer fully covered: inspect
the external chain's material/geometry effects and ownership first. Any future
parent grouping must retain traversal order, replace child claims atomically,
and preserve fallback when the whole group cannot be validated.

Validation: all 25 renderer test executables and all 135 public tests pass
(hud-group-tests-final.log and hud-group-public-tests-final.log under E evidence).
Observer tests cover nested callbacks, empty groups, wrong callback pairing,
range errors, guest immutability and rollback at every hook installation slot.
A direct standalone gcc attempt lacked its runtime PATH and produced no binary;
the standard renderer runner supplies that PATH and passed afterward.
The route ran alongside compilation/tests and is not an isolated FPS benchmark.
Next: account for DMG's external chain, then validate whole-group placement
and world-marker exclusion with stretched/centered capture controls.


### HUD external texture-chain handling and experimental ownership

The second live census (hud-group-chain-live, normal exit) identifies DMG's
CALL target 01075570 as CNT/DIRECTHL (210 qwords), transfer setup and image data,
followed by an empty RET at 010762A0. Added a strict parser for precisely the
four-register BITBLTBUF/TRXPOS/TRXREG/TRXDIR host-to-local setup plus IMAGE/RET
form. Unknown external chain layouts still decline. External texture uploads
remain on the original GS path and are not claimed as native geometry.

Experimental PS2_AC4_HUD_GROUP_NATIVE=1 enables producer-level ownership only
after complete inline/external preflight. Parent payloads replace child sprite
claims atomically; actual DMA traversal determines draw order. World-marker
claims remain untouched. Capacity/overlap/already-delivered checks decline
without modifying children. External chain hashes are checked again before
accepting a native payload, so reused upload memory cannot silently change a
previously accepted group. No normal or hybrid launcher enables this flag yet.

Build passed; all 25 synthetic renderer executables passed (hud-group-external-
tests.log). Tests include reversed traversal, atomic capacity failure, stale
payload/upload dependencies, world-marker preservation and unknown ownership.
Live capture validation was launched in E:/ac4-renderer-evidence/hud-group-
native-live using test_hud_group_native_live.py; results were still pending
when this note was written. Follow that live process rather than launching a
duplicate. Its capture at field 10200 is intended for the prepared private
E:/ac4-renderer-evidence/test_hud_group_capture.py stretched/centered replay
comparison. Do not claim visual acceptance until those results are inspected.


### HUD capture comparison and seven-producer follow-up

First native ownership run: hud-group-native-live completed normally. PTS,
TIME, weapons and TGT each matched 284 calls; all 1,136 accepted groups queued
with no ownership declines. DMG still declined all 284 calls. The full census
coverage gate therefore fails this run. Its private DMA capture permits an
independent image comparison: hud-group-replay GS, stretched native in-place,
and stretched native overlay are byte-identical at 1x (SHA256
 da9156d742d184b7b40efa07ec90d2a6a3bdaa04f5278d83970d10065a17bd10).
Native modes report 152 closed/accepted elements and 200 primitives. Centered
mode moves 75 elements; inspected images show the weapon block crowding the
unchanged altitude scale, so this is not complete centered-layout acceptance.

Captured RAM bytes show DMG's external chain has two uploads (texture and
palette), IMAGE tags without EOP, and a final TEXFLUSH before RET. The earlier
single-image/EOP predicate correctly declined rather than silently skipping
unknown data. Replaced it with a bounded sequence validator for this exact
transfer-only pattern. Synthetic cases include multiple uploads and rejection
of geometry registers; the actual extracted 3,392-byte private chain also
passes the updated predicate (hud-upload.bin / hud_upload_fixture_test.exe).
No game bytes are included in the redistributable tests.

Added exact-signature fixed producer hooks for speed 001609D8 and altitude
00161250. Generated source groups each label, numeric helper and scale-line
helpers under those producers. Ownership remains experimental; world targeting
and the projected pitch ladder are excluded. Radar and alternate camera HUD
coverage/placement still need acceptance. The census validator now requires
all seven producers, with --native additionally requiring ownership of every
matched producer. All 25 renderer executables and 136 public tests pass.

The rebuilt seven-producer live test is running in E:/ac4-renderer-evidence/
hud-seven-live (driver test_hud_seven_live.py). Reuse that process; do not start
a duplicate while it is alive. After completion run analyze_ac4_hud_groups.py
with --native, then E:/ac4-renderer-evidence/test_hud_seven_capture.py and inspect
the stretched/centered images. Results were pending at this note. Launch presets
remain unchanged; no push performed.


### Seven-producer acceptance and camera-transition follow-up

hud-seven-live completed normally: each of seven producers matched 302 calls,
with zero declines/unpaired hooks. All 2,114 matched groups obtained ownership.
The then-current seven-producer --native coverage gate passed. Replay at 1x
has 320 accepted/closed elements and 428 primitives; GS, stretched native
in-place and stretched native overlay are byte-identical, SHA256 88834877d7f87377e6b09a40c3f44c89aa62d7640009f6be662aa259d00fb053.
Centered output was inspected: whole speed/altitude scales and the DMG block
move, world labels remain projected. Separate fixed bracket producers still
stay at their old positions; radar is also unchanged. Centered parity is not
yet accepted. Images and comparison.json are under hud-seven-replay.

Added exact-signature hooks for bracket producers 00160FF8 and 00161890,
and alternate speed/altitude 00160E78 and 00161720. The alternate functions
end in tail calls to the numeric-glyph helper, so retain full-function entry/
exit observation. No guest patch or original-helper suppression was added.
The checker now requires all eleven reports; the two alternate producers may
have zero calls unless --views is passed. All other nine must be exercised.
Ownership remains opt-in only. All 25 renderer tests and 137 public tests pass.

Current live validation: E:/ac4-renderer-evidence/hud-views-live, launched via
test_hud_views_live.py, 13,400 fields, camera changes at 12,020 and 12,800,
capture at 12,900. It was still running when this note was written. Poll that
process rather than restarting it. After completion validate census with
--native --views, inspect camera screenshots, and run test_hud_views_capture.py
at scales 1 and 3. Radar placement, other mode/producer coverage and live resize
acceptance remain open. Existing launch presets and public checkout unchanged.


### Eleven-producer camera route accepted; radar census pending

hud-views-live exited 0 at 13,400 fields. --native --views coverage passes:
five always-visible groups each matched 1,637 calls, four full-HUD scale/
bracket groups each matched 1,350, and both alternate speed/altitude groups
each matched 287. All 14,159 groups queued, with zero declines or unpaired
hooks. This proves both full and alternate HUD producer paths were exercised.

The four-field third-person capture passes GS vs stretched native in-place
vs stretched native overlay comparisons at both internal scales:
- 1x SHA256 3f2fc458966e3bd4cc6ab8ad481f6c1dc91cf062cac50a4e873e5d5be5ab142a
- 3x SHA256 f33f9929f8b56c7c37301def1e9f6e4d7aa7fc11c15bacd7a8ffe55c2abc780c
Each native replay has 168 accepted/closed elements and 244 primitives.
Centered output differs as intended. Inspected flight-preview.png,
cockpit-preview.png and third-person centered frame-preview.png: speed/altitude
brackets stay with their scales, aircraft/cockpit are visible, projected friend
markers remain aligned, captions and DMG block are present. Previews are resized
to the intended 16:9 display aspect; original PPMs remain untouched. The radar
still has its original placement, so centered HUD parity remains incomplete.
The earlier seven-producer capture also passes at 3x (hud-seven-replay-3x).

Added observer-only candidates 00169010 and 0016BC68, based on the HUD root's
radar/map mode branch. They use separate `ac4 HUD candidate` reports and are
never queued by the native ownership path, even if their packet preflight
succeeds. Tests assert this exclusion. Optional private
PS2_AC4_HUD_CANDIDATE_DUMP writes the first bounded allocation for each candidate
into a specified evidence directory; files are private game data, not fixtures
for distribution. All 25 renderer executables and 138 public tests pass.

New live observer: E:/ac4-renderer-evidence/hud-radar-census-live, driver
E:/ac4-renderer-evidence/test_hud_radar_census_live.py. It was running when this
note was written; poll the existing process. Inspect candidate counters and
hud-00169010.bin / hud-0016bc68.bin if produced, then map their geometry/mask
coverage before enabling radar ownership or placement. No launch preset or
public checkout change; no push. Live resize and broader scene acceptance also
remain open for the full renderer parity goal.


### Normal radar packet range established; native validation pending

hud-radar-census-live completed normally. Candidate 00169010 matched all 300
calls (7,650 inline payloads), with no declines. Candidate 0016BC68 was not
called; its behavior is not established by this route. The first 00169010
allocation was saved privately: 2,224 bytes / 25 DMA tags. Offline inspection
(inspect_hud_allocation.py, hud-00169010.json) shows SCISSOR x=23..129,
y=291..421; background geometry, ALPHA/TEST changes, mask geometry, linework,
five aircraft/player sprites and a final full-frame SCISSOR restore. These
bounds match the visible radar. There are no external CALLs in this sample.

Changed only the known normal radar producer to eligible ownership in the
experimental HUD path. Its complete preflight still runs on every invocation;
unknown or oversized packets retain the existing path. Mask/background/icons
remain in original DMA traversal order. The alternate map producer remains
observer-only and cannot enqueue native ownership. The census gate now expects
twelve fixed producers (alternate speed/altitude coverage remains controlled
by --views). All 25 renderer executables and 138 public tests pass.

Current process: E:/ac4-renderer-evidence/hud-radar-native-live, launched by
test_hud_radar_native_live.py. It was still running when this note was written.
After completion run analyze_ac4_hud_groups.py --native (without --views for
this flight-only route), then test_hud_radar_capture.py at scales 1 and 3.
Inspect centered radar masking/background/icon placement before accepting or
promoting this mode. Launch presets and public checkout remain unchanged.

Storage: moved the old work/aircraft-off.ppm diagnostic image to
E:/ac4-renderer-evidence/archived-work/aircraft-off.ppm and verified SHA256 before
and after the move. aircraft-off-relocation.json records both paths and hash.
This preserves the image while recovering about 21 MB on nearly full F:.
No source or game data was removed. Future test outputs remain on E:.


### Normal radar and window-size transitions accepted; combined preset in test

Normal radar ownership passed hud-radar-native-live: 302 matched calls at
00169010, 7,701 radar payloads, 3,020 total fixed groups queued, zero declines
and balanced hooks. hud-radar-replay and hud-radar-replay-3x compare GS,
stretched in-place and stretched overlay output byte-for-byte at each scale.
Each native mode reports 416 elements, 384 accepted whole, 536 accepted
primitives, 52 fallback primitives and zero unclosed elements. Centered output
was inspected: the radar panel, mask, linework and icons move together.
The alternate map remains observer-only and is not promoted by this evidence.

Added a hidden replay-only SDL/Vulkan resize diagnostic and a fresh-control
comparison at 720p, 1080p, 4K, then 720p again. Window cycle evidence in
E:/ac4-renderer-evidence/window-cycle-zlrvu03r passed all four byte-exact
comparisons with actual drawable extents checked. Synthetic tests cover hidden
window guards, invalid sizes, GPU wait/SDL/swapchain failures and unchanged
settings on failure. This does not establish interactive Alt-Tab or settings UI
acceptance. Replay fuzz mode is excluded from this diagnostic.

The opt-in HybridRenderer preset now enables HUD_GROUP_NATIVE. Normal play and
VU capture retain their prior defaults; inherited developer flags cannot enable
it there. The combined route checker now requires complete native fixed-HUD
ownership, including alternate speed/altitude when cockpit views are requested.
All 26 renderer executables and 141 public tests passed. README descriptions
were updated to distinguish the default compatibility renderer from the hybrid
path. Public checkout and GitHub remain untouched.

Combined preset route in progress: E:/ac4-renderer-evidence/hybrid-hud-preset-live,
driver test_hybrid_hud_preset_live.py. This uses the actual preset without forced
HUD/cockpit/mask feature overrides, isolated saves/settings, 13,400 fields,
camera changes at 12,020 and 12,800, and a four-field capture at 12,900.
Require combined route report with cockpit=True,hud=True and inspect images.
Do not interpret this pending route as finished acceptance or locked 60 FPS.


### Combined HUD preset route accepted for Mission 1

hybrid-hud-preset-live completed all 13,400 fields and exited 0. Combined route
validation with cockpit=True,hud=True passed; coverage.json records counts.
All twelve fixed HUD producers were exercised: the six common groups (including
radar) matched 1,529 calls each, four scale/bracket groups matched 1,286 each,
and alternate speed/altitude matched 243 each. All 14,804 groups queued with
zero ownership declines and balanced hooks. GPU coverage: 11,644,521 mesh
draws, 7,240,890 native materials, 2,840,636 supplied-mip draws, zero arena
refusals or native texture allocation fallbacks. Unsupported states retain GS.
Cockpit: 145,323 native activations, 182,437 whole GIF packets bypassed and
283,657 GPU draws. Counts establish coverage, not equivalent workload to older
runs; game draw counts vary with scheduling and dropped fields.

Inspected the flight frame with AWACS caption, cockpit frame at field 12,300,
and third-person frame at 12,900. Aircraft, radar background/icons, fixed
brackets and captions remain visibly coherent. Audio was not listened to.
This diagnostic run reported 47.5-53.6 fields/s in its 600-field intervals;
it is not evidence of locked 60 FPS or a controlled performance comparison.

The final replay resize test after excluding fuzz mode also passed, evidence
window-cycle-1kt21i1a. Added tools/test_ac4_hud_gpu.py as a reusable private
capture comparator; its first normal-radar run (hud-gpu-1x-uvvwddjh) matched
GS exactly for both stretched native modes. Centered output requires visual
review in addition to changed-pixel evidence. The complete parity goal remains
active: alternate map and broader mission/material/temporal transitions are
not established by this Mission 1 route. No public checkout or GitHub changes.


### Alternate-map route investigation

The previous combined preset third-person capture also passes 3x GS versus
native stretched HUD comparisons (hud-gpu-3x-b4le6eio): 220 elements,
188 whole, 308 native primitives, 52 fallback primitives, zero unclosed.

The alternate-map gate in analyze_ac4_hud_groups.py now has --map-candidate:
require one positive, consistent, decline-free observer report for 0016BC68.
This evidence is explicitly not native ownership. Added positive/negative
synthetic tests; all 142 public tests pass.

hud-map-census-live (11,200 fields) tested R2 at 10,220 and 10,620. It exited
normally but produced zero alternate-map calls, so it does NOT validate map
coverage. The internal action mask at control+2D4 is not established as raw PS2
button numbering. The follow-up hud-map-select-live tests SELECT at the same
fields and captures 10,800..10,803. It is running at this note. Keep 0016BC68
observer-only until actual packet and rendered evidence supports ownership.


Alternate-map control correction: hud-map-select-live also exited 0 with zero
0016BC68 calls. Do not count either R2 or SELECT route as map acceptance.
The AC4 manual identifies Square as the pressure-sensitive map control (manual
scan: https://www.scribd.com/document/668355481/Ace-Combat-4-Manual-PS2).
Started hud-map-square-live using test_hud_map_square_live.py: Square at 10,220
and continuous eight-field script entries from 10,620 through 10,860 (32 total
entries, within PAD_SCRIPT_MAX); capture 10,800..10,803 occurs while held.
It is still running. Poll existing exec session 74091 before any relaunch.
Inspect candidate report/dump; run --map-candidate gate, inspect allocation,
then establish native ownership only if its complete packet range is safe.
No renderer behavior changed during this map investigation.


### Paused for user validation

User requested wrapping up and testing the current code manually. Interrupted
the held-Square diagnostic session 74091 with Ctrl-C; it exited 1 before the
flight/map capture (last recorded progress near field 3,000). This is an
interrupted test, not map acceptance. Alternate map remains observer-only;
no native map behavior was enabled. The last completed public suite passed
142 tests. Prior combined Mission 1 cockpit/third-person HUD acceptance and
720p/1080p/4K resize comparisons remain the current validated evidence.

Run play.cmd -HybridRenderer from this v3-native-mesh checkout to validate the
combined native renderer. Ordinary play.cmd retains the previous default.
No public checkout changes or push. Resume by completing the Square-held map
route and inspecting its producer packets/images, then broader scene and
transition acceptance. Full AC5 parity and locked 60 FPS remain unclaimed.


### User feedback while paused: flicker, intro timing and 4K qualification

User reports flickering and the same intro lag/audio-video desynchronization in
manual validation. Exact flicker scene and which intro sequence are not yet
specified. Treat these as open observed issues; earlier movie acceptance does
not clear this new report. User explicitly deferred investigation until tomorrow;
no debugging run or renderer changes were started for this feedback.

4K qualification: verified 3840x2160 window/swapchain and high-resolution native
overlay/display, not every draw at one-to-one 3840x2160 raster resolution.
The fresh2160 resize control uses a 3200x2240 5x display/overlay with a captured
2560x2240 display crop, then widescreen presentation. Some draws remain on GS.
Do not describe this as every native item proven literal native 4K.
Radio speaker/glyph draws have explicit 1x-scene/5x-overlay lift evidence in
screen-caption-tail (draws 1223/1224); this proves native high-resolution caption
rasterization for that capture, not all caption states or HD replacement fonts.
Pending: audit per-layer render sizes and fallback in the user's reported scenes,
then decide whether a true output-sized target is required for the 4K promise.


### HUD flicker investigation and optional full-output coverage

User clarified flicker affects HUD and requested resuming work, with additional
4K coverage. Exact HUD elements/flash versus shimmer remain unanswered in the
pending clarification. Do not claim this issue fixed.

Captured 60 consecutive flight fields in hud-flicker-4k-live (10800-field run,
exit 0, capture at10400, 3840x2160 window). HUD group ownership 4890 queued,
zero declines. Added hidden-only replay --field-shots (bounded1..120 fields)
and --skip-field diagnostics. Skip injection is GPU-lock serialized. Short
four-field radar tests, including adjacent-field comparisons, remain byte-exact
when each prior presentation is skipped; this does not prove live timing is safe.

Temporal evidence: hud-temporal-1x-stretched matches GS, native in-place and
native overlay in all60fields. hud-temporal-5x-stretched matches GS/overlay in
all12testedfields. hud-temporal-5x-centered matches native in-place/overlay in
all12testedfields. Original hud-temporal-1x attempt had incorrectly sectioned
HUD-layout settings and was interrupted; use only corrected named results.
These samples do not reproduce the reported flicker. Need exact affected element,
scene and flashing-versus-shimmer description, then broaden targeted capture.

Added opt-in play.cmd -HybridRenderer -FullOutputResolution. It sets
PS2_AC4_FULL_OUTPUT_RES and only changes Auto resolution selection: cover width
as well as height for both scene and native overlay. Manual scene resolution,
maximum image dimensions and scale limit still apply. Ordinary play and hybrid
without the option preserve previous scaling; capture mode disables the flag.
This is integer supersampling, not higher-resolution texture replacement.

GPU evidence full-output-gpu uses the60field flight capture at3840x2160.
The scene advances from5x to8x after Auto stabilization; overlay/display is8x,
final display crop4096x3584. Native meshes291932, native material draws114924,
supplied mip draws114924, zero arena refusal/texture allocation fallback.
Compared with full-output-fixed8, final output is byte-identical, SHA256
5c2033ded132d972a7eec8f83151d4d5dd004128cdbfd6d6e23e3c681fd074e6.
Preview inspected: terrain, aircraft, HUD/radar and radio caption visible.
This verifies sampled pixel coverage, not locked60FPS, all scenes or HD artwork.

144 public tests and26renderer executables passed. Added width/height,
placeholder/overflow/limit tests, launcher opt-in/capture isolation checks and
hidden skip guards. Updated README/TESTING. Final build log hud-4k-final-build.log.
No public checkout changes or push. Intro lag/desync remains a separate open
report. Alternate-map native ownership is still observer-only.


### Field handoff and hybrid pipeline preparation (2026-10-15)

User reports intermittent radar/flight HUD and pause-menu flicker, plus intro
and aircraft-room stalls. Producer-written display crop/target/blank state was
shared with the renderer even after it adopted an earlier draw list. Publish
that state under list_lock and adopt it alongside the draw pointers; queued
replacement preserves the last published dimensions. Modal window redraws now
adopt pending draw lists instead of silently consuming the request. Synthetic
tests exercise pending replacement, active-state immutability, blank/override
targets and modal adoption. This fixes a concrete race; perceptual flicker
acceptance remains pending rather than inferred from synthetic tests.

A bounded 10,800-field hybrid route reached opening/story, briefing, aircraft
room, flight and pause (START at 10420, resume at 10720), exiting successfully.
Evidence: E:/ac4-renderer-evidence/handoff-pause-live. Saved images show aircraft,
HUD and pause menu, but sparse images do not prove absence of temporal flicker.
The route recorded 19 slow gameplay pipeline compilations totaling 2854 ms,
including renderer record stalls of 1103 ms and 1361 ms. Twenty observed states
were absent from the shipped startup list. They are now prepared before guest
startup; only device-independent renderer keys were added, not cached shader
binaries or game assets. Unknown states still compile on demand.

Periodic 4K screenshot writes also caused multi-second diagnostic pauses.
Batch screenshot RGB conversion/writes by row, preserving crop/stride and exact
pixels, checking file errors. Synthetic exact-pixel tests pass. Ordinary play
without captures does not benefit from this specific change. A separate route
without screenshots or DMA capture is being measured in handoff-clean-timing;
its application pipeline cache started empty. Do not compare whole-run FPS
with the capture-heavy route as a controlled performance benchmark.

Build succeeded; native regression tests including field handoff and screenshot
rows passed (28 executables; initial log label still said 27), and all 144
public unit/launcher acceptance tests passed. No public checkout or push.

Clean timing result: 10,800 fields, exit 0, initially empty application pipeline
cache, all 94 shipped states prepared, ZERO gameplay pipeline compilations.
10,799 presentation intervals averaged 16.7543 ms (~59.69 Hz), p99 38.7734 ms,
worst 88.6721 ms, 514 intervals over 33.37 ms. These are CPU presentation
timestamps, not scanout; hidden-window test with EE sampling during fields
6900..9000, no periodic screenshots or DMA capture. Approximate route windows:
opening/story mean 16.6603 ms / max 70.126; briefing mean 16.6835 / max 64.4349;
aircraft room mean 17.1148 / max 88.6721; loading/takeoff mean 16.6774 / max
41.476; flight/pause mean 16.7834 / max 49.0355. Window labels follow the
recorded route and sparse visual checkpoints, not per-frame scene detection.

Worst gaps (source 6683..6685) track ready-to-ready frame-production gaps
(77.6..89.3 ms), while renderer host work is 2.1..6.1 ms and image allocation
time is zero. EE samples are primarily waiting/polling: 53.86% outside the
executable, 21.72% ps2_kernel_poll_vblank, 8.15% ps2_mmio_r32. Profile the
graphics worker next; this is not proof of a specific worker function being
the cause. Remaining spikes and audio-sync manual acceptance stay open.
Do not claim locked 60 FPS, or compare this clean aggregate directly against
the screenshot-heavy baseline. The demonstrated optimization is moving the
observed hybrid pipeline compilations out of gameplay.

Final GPU regression: all 12 consecutive 5x flight fields match GS versus
native overlay exactly, and all 24 GS/overlay images also match the pre-change
reference byte-for-byte. Evidence: handoff-temporal-5x/comparison.json. This
validates image preservation and the batched screenshot output; lockstep
replay cannot prove the live field race or intermittent flicker is resolved.
All bounded test processes have exited; rebuilt native binary is ready for
manual flight/HUD/pause and story-audio checks.


### Full output hybrid default and aircraft-room math optimization (2026-10-15)

At the user's request HybridRenderer now always enables FULL_OUTPUT_RES. The
FullOutputResolution switch remains compatible; bootstrap/capture and ordinary
nonhybrid launches remain isolated. Existing fixed internal resolutions are
still honored; Auto plus the default 3840x2160 output enables full coverage.
README, launcher tests and TESTING reflect this default. All 144 public tests
pass. No changes to the public source checkout or GitHub.

Two 10,800-field routes used 3840x2160, full output coverage, original route
and isolated saves, no periodic screenshots/DMA capture, initially empty
application caches, and graphics-worker sampling over fields 6300..8100.
Evidence: E:/ac4-renderer-evidence/full4k-worker-baseline and
full4k-worker-simd (config, timeline, logs, host samples, summary). Both exit 0.
The baseline opening overlapped launcher tests briefly; room sampling did not.
These are hidden-window CPU presentation timings, not scanout measurements.
Both confirmed 8x scene scaling and no gameplay pipeline compilation.

The baseline worker profile showed transform 17.68%, flags_result 9.85%,
flags_transform 6.13%. Added SSE2 four-lane transforms with explicit denormal
flush, saturation and NaN behavior after each separate multiply/add; retained
the scalar implementation on other architectures. Flag advancement now drains
at most four pending slots rather than iterating through idle cycles. No
fast-math, reduced resolution or change to guest timing. Scalar differential
tests cover 100,000 matrices/vectors including arbitrary bit patterns, signed
zeros, denormals, infinities/NaNs, and 10,000 delayed flag sequences. All 28
native renderer test executables passed; both binaries rebuilt successfully.

Before -> after in the approximate aircraft-room window (fields 6300..8100):
mean interval 17.024 -> 16.841 ms; intervals >33.37 ms 546 -> 155; p99 42.476 ->
44.270 ms; maximum 52.426 -> 60.844 ms. Thus long intervals are substantially
less frequent, but the tail is NOT uniformly improved or resolved. Worker
transform share fell to 7.83%, flags_transform to 2.95%. Whole-route mean
16.796 -> 16.707 ms (~59.71 -> ~59.85 Hz), p99 39.212 -> 35.957 ms, maximum
195.726 -> 112.129 ms. Flight/pause interval count >33.37 ms 22 -> 0, maximum
80.527 -> 31.591 ms. Uncontrolled host variance remains; these are two runs,
not a locked-60 guarantee. Intro and briefing sporadic gaps remain, with some
metrics worse on the repeat. Audio synchronization still needs manual review.

Remaining worker costs include flags_result, model evaluation/clipping and
texture upload processing. Investigate these and the isolated long production
gaps with full-output mode retained. Do not silently revert to height-only
scaling to improve benchmark numbers.

Final 8x GPU validation: third_person_controlled.gscap, four fields, passed
all seven original/audit/mesh/material/native-lit modes with identical final
images (SHA256 6fd8e15723eea71adba57ad9996fb0642f3fad0f995b18d27e93b228e3d20f72),
3796 matching lit activation/packet continuations and 2024 GPU model draws.
Evidence: model-gpu-parity-45g_y2ft, full4k-flight-gpu.log on E:.

Aircraft-room eight-field capture (briefing_corruption.gscap) produced identical
visible final images across all nine original/native/material modes and zero
reported packet/state mismatches, but the strict full-coverage gate FAILED:
16 empty clipped batches lack full native continuation prediction (32 skipped
packets). This known unsupported class retains fallback; do not report this as
a full acceptance pass or weaken the gate. Evidence: model-gpu-parity-ppxgynyp,
full4k-model-gpu.log. Preview inspected: aircraft and room geometry present.
Synthetic math and supported flight coverage pass; broader clipped/full-game
coverage remains required. Final live screenshot confirms 4096x3584 displayed
scene/overlay crop at 8x feeding the 3840x2160 window. All test runs exited.


### Ordered AC4 fields and full-4K spike investigation (2026-10-18)

User requested adapting PresidentRaptor/Ace-Combat-04-PC-Port's no-dropped-field
fix while retaining our native mesh/HUD renderer and full-output coverage.
Reference: its runtime/src/ps2_video.c and docs/technical-notes.md on main,
reviewed 2026-10-18; Apache-2.0 attribution added in CREDITS.md. No public push.

Normal AC4 handoff now preserves a pending field until the renderer adopts it.
Timeouts are 100-ms cancellation checks, not permission to overwrite it. This
is stronger than the reference's one-time 100-ms wait followed by replacement.
Shutdown/exit is observed and wait failures request exit rather than discard
queued draws. PS2_DROP_FIELDS=1 restores legacy replacement for diagnostics;
normal launchers force zero. Rendering every field is distinct from displaying
every field at a stable interval. Synthetic real-thread tests verify sequence
retention through >100-ms delays, spurious wakeups, shutdown and opt-in legacy
behavior. Pending mesh lists and their display state remain latched together.

Completed the presentation changes begun before the interrupted 2026-10-16
turn: semaphore pools belong to swapchain images, successful SUBOPTIMAL
acquisition is submitted before recreation, and the acquired-image transition
starts at COLOR_ATTACHMENT_OUTPUT to match its semaphore wait. Tests exercise
creation failure rollback, acquired-image ordering, resize and acquisition
results. The native model flag-mask SIMD optimization is also built and
passes 100,000 scalar differential traces (including high mask bits and
signed-zero/NaN signs). Native renderer regressions passed 30 executables;
public unit/launcher acceptance passed 145 tests.

The old full4k-views-radar-before attempt was interrupted before its capture;
it is incomplete and excluded. ordered-fields-4k-route was deliberately stopped
early when the missing acquire barrier was identified, and is also excluded.
The completed ordered-fields-4k-final route covered 13,400 fields, native/full
4K, view changes at 11020/11420/11820/12420/12800 and Square at 12020. All
source IDs 1..13400 appear in the renderer timeline, with no missing IDs and no
queued-field replacements; exit 0. HUD groups: 14,408 queued, zero declines;
normal radar 1003 matched calls. Alternate map remains observer-only: 572 calls
explicitly declined. Its 60-field DMA capture at 12790 is complete. Configuring
DMA capture forces synchronous graphics execution, so this route is correctness
evidence and MUST NOT be compared with async normal-play performance runs.
The user's replay flicker means in-game post-mission replay; this route does
not complete a mission and does not establish replay acceptance.

Separate clean async 10,800-field runs used streamed ISO reads and the existing
RAM disc-cache option (3,554,705,408 bytes successfully preloaded), full output
3840x2160, identical route/settings and worker sampling at 6300..8100. Both
exit 0 and have no missing source IDs. Evidence directories: ordered-fields-
4k-streamed and ordered-fields-4k-memory on E:/ac4-renderer-evidence. Do not
claim RAM caching fixes every spike: its worst frame remained 180 ms. The
frame trace attributes record-time stalls at source 8936 and 9753 to 2.2 GB
GPU allocations taking 127 and 110 ms, rather than shader compilation.

Found full-output startup preparation was still reserving 5x images: 38
3200x4480 entries, only two consumed before the first scaling change released
the unused pool. New preparation targets full-output flight size while reducing
the entry count to retain the previous reserved-pixel budget: 13 entries at
5120x7168 for the tested 4K setup, versus 38 smaller entries. Unused larger
images survive expected lower-scale transitions at the same GS dimensions.
Unexpected dimensions/larger scale release them, and allocation pressure
releases all unused reservations before retrying normal allocation. No guest
render-target slot is pre-created. The tests cover budget bounds, matching
resource ownership and failure recovery. A new camera-view pipeline key is
prepared at startup. All 31 renderer executables pass after teaching the C
function extractor to skip forward declarations. Final async RAM-cache run
with this preparation is being measured in ordered-fields-4k-prewarm. The
initial opening overlapped test compilation; later room/loading windows did
not. A successful run is not a locked-60 or visual-flicker acceptance claim.

Final validation and muted repeat:

- Same native/full-output binary, deliberate 100-ms renderer delays: legacy
  replacement discarded six fields; ordered mode discarded zero. Both 800-
  field runs exit 0. Evidence: ordered-stall-comparison.json on E:.
- The full-4K prewarm run exits 0 with all 10,800 source IDs present, 13 startup
  image reservations consumed (previous strategy consumed two), and no gameplay
  shader compilations. Room p99 was 32.39 ms, nine intervals over 33.37 ms;
  loading max 105.70 ms. This was not consistently reproduced, so do not claim
  locked 60 or a uniform performance win.
- At the user's request, repeated muted in ordered-fields-4k-muted. Added
  PS2_AUDIO_MUTE for diagnostic playback gain zero before the device resumes;
  mixing/callbacks remain active. Log confirms muting and 34,838,400 bytes
  drained (181.4 seconds). No audio disabling or dummy backend. Native/full
  4K, 10,800 fields, exit 0, no missing source IDs, all fields retained.
  Mean presentation interval 16.799 ms (~59.53 submissions/s), p99 41.074 ms,
  worst 162.359 ms. Room p99 51.644 ms / max 146.810 ms, 371 intervals over
  33.37 ms; flight/pause max 27.122 ms. Large resource allocations remain the
  dominant recorded loading spikes (2.07 GB bursts, 83/129 ms allocation time).
  Host-load variation is substantial; keep this repeat alongside the better
  first result rather than selecting the better run. Future background game
  tests should set PS2_AUDIO_MUTE=1 after launcher environment isolation.
- All 20 consecutive 8x captured outputs match centred native in-place versus
  overlay composition exactly. Field one is uniform capture priming, excluded
  from visual coverage; 19 visible frames show third-person aircraft and the
  expanded radar/map. The inspected window does NOT visibly establish a camera
  transition despite the scripted button timing. Whole live route did exercise
  cockpit rendering (322,498 GPU draws). Do not turn that into per-transition
  or post-mission-replay acceptance. Evidence: ordered-fields-views-temporal.
- Real hidden SDL/Vulkan window 720p/1080p/4K/720p recreation with internal 8x
  retained matches fresh controls at each size. This validates presentation
  pool recreation and captured rendering, not interactive Alt-Tab. Evidence:
  window-cycle-xcn4kce4 and ordered-present-cycle.log. Preview inspected for
  visible aircraft, HUD and radar. All test processes finished.

Remaining: manually verify user-reported view/radar transitions and in-game
post-mission replay flicker. No queued field loss is reproduced with the fix,
but that is not proof every source of flicker is fixed. Keep optimizing actual
room CPU production and large loading allocations with full 4K retained; save/
load, broader missions and Alt-Tab still need their own acceptance.

### Manual validation update - 2026-10-18

User tested the current native hybrid build with full 4K enabled and reports
substantial frame drops and noticeable lag. Performance acceptance remains
unresolved: the current build must not be described as locked or stable 60 FPS.
This is a qualitative user observation; no new measured frame-time statistics
or specific scene attribution were supplied with this report. Preserve the
existing timing evidence and continue investigating stalls with full 4K enabled.

### Full-4K Auto scale transition stalls - 2026-10-18

The previous muted route rebuilt all live targets when Auto switched 8x -> 6x
-> 8x between AC4 display crops. Two fields allocated approximately 2.07 GB
each, costing 83/129 ms in the allocator. Native hybrid rendering remains on.
Auto now covers the known 512x448 flight crop from startup and keeps that
minimum across menu/room transitions. The current window still determines
the minimum, so downsizing can reduce it; hardware limits and allocation
failure fallback remain in force. Explicit internal resolution bypasses it.
PS2_AC4_STABLE_AUTO=0 is a private renderer diagnostic rollback; the public
launcher isolates inherited PS2 variables. Full output coverage stays enabled.

Startup prewarm counts retain the previous height-based byte budget even
though startup now chooses the larger scale. At 4K: 13 images, not 38.
The first exploratory stable-auto-4k-muted run used the enlarged 38-image
reservation; exclude it from final performance acceptance. The corrected
run is stable-auto-budget-4k-muted. Final renderer suite: all 31 executables
pass, including production choose_scale checks for Auto, manual scale,
hardware limits, smaller windows, compatibility mode and diagnostic rollback.

Corrected-budget live result: native hybrid, full output coverage, Auto,
3840x2160, muted playback with mixer active, 10,800 fields, exit 0. All source
IDs retained; no target scale transition logged; final crop 4096x3584 at 8x.
All 13 reservations consumed. Summary: stable-auto-budget-4k-muted/
stable-summary.json under E:/ac4-renderer-evidence. Compared with the prior
ordered-fields-4k-muted run: mean 16.677 vs 16.799 ms; p99 28.824 vs 41.074 ms;
worst 109.759 vs 162.359 ms; intervals >33.37 ms 19 vs 383; >50 ms 2 vs 31.
Room fields 6300-8099: p99 32.688, max 41.177 ms, 13 >33.37 ms (prior 371).
Flight/pause fields 9900-10799: max 22.734 ms, none >33.37 ms. Host variation
prevents attributing every room improvement to this policy. Regression suite
compilation overlapped early opening playback, but finished well before room/
loading/flight; this is not a clean opening performance acceptance run.

Correction to the initial allocation hypothesis: scale rebuilds are gone,
but newly needed target/snapshot images still allocate lazily at transitions.
Field 8936 allocates 1.485 GB / 54.773 ms (73.151 ms interval); field 9738
allocates 2.219 GB / 88.412 ms (109.759 ms interval); next field allocates
740.655 MB / 24.810 ms. Do not claim the large allocation bursts are removed.
The exploratory 38-image run consumed all 38 reservations and had a 66.497 ms
maximum, but changes startup memory commitment; it is not the shipped default
and is not an isolated comparison. Next allocation work must address actual
late resource creation/retirement without discarding guest rendering history
or silently reducing 4K coverage. Stable 60 FPS and manual flicker acceptance
remain unresolved. No public checkout changes or push performed.

### Budget-aware loading image preparation - 2026-10-18

Allocation trace loading-alloc-trace-4k reproduces the remaining stalls with
full native/4K enabled: max 117.289 ms; field 9741 allocates 2.219 GB in
93.325 ms, field 8936 allocates 1.485 GB in 57.739 ms, next loading field
allocates 740.655 MB in 26.251 ms. Exact image-requirements.json separates
formats AND usage masks. They are RGBA8/D32 targets and native composition
images, not float-color effects. Known route total: 17 color usage 23,
16 depth usage 39, nine snapshots usage 6, two depth usage 32, one color
usage 21. Two initial target images already exist before reserve creation.

Native full-output Auto can now prepare 15 target pairs, eight snapshots
and five remaining native composition images before releasing the game.
This creates unused image objects only: guest target existence, descriptor
binding, image layouts, draws and history are initialized by ordinary demand.
No aliasing, texture changes, format changes, dropped fields or resolution
reduction. Exact requirements use the same image-create-info helper as
ordinary allocations and vkGetDeviceImageMemoryRequirements (Vulkan 1.3).

VK_EXT_memory_budget is enabled only if advertised. The larger plan requires
all image classes to choose one device-local heap; estimated bytes must fit
half of its reported remaining budget, an 8 GiB reserve cap, and leave at
least 2 GiB. Budget is rechecked during preparation. Unknown/unsupported or
low budgets keep the previous smaller reservation. Allocation failure still
releases unused reservations before ordinary retry. Real window resizing
that lowers scale releases incompatible unused reservations; menu crop
changes at the same window keep compatible ones. Estimates are not a
guarantee against later external GPU-memory pressure.

PS2_LOADING_PREWARM=0 privately restores the small-reserve policy; inject
after launcher isolation for comparisons. No new public launcher mode.
On this adapter the 43-image plan is 6,368,968,976 bytes (~5.93 GiB) committed
earlier, versus the former 13-image pool. The traced route needs those
images anyway; this is explicit startup preparation, not cheaper images
or a persistent shader/image cache. Other applications remain running.
Reference for budget semantics: https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_memory_budget.html

Both executables rebuilt; 32 native renderer executables pass, including
new mocked-Vulkan budget/requirements/startup lifecycle tests. They cover
exact usage, nonlocal/split heaps, bounds, small/unsupported budgets, opt-out,
pressure stops, window changes and cleanup. Final logs: loading-reserve-
final-build.log / loading-reserve-final-tests.log in E:/ac4-renderer-evidence.

First live run loading-reserve-4k-muted: 10,800 fields, exit 0, no missing
source IDs; 43 images prepared in 234.90 ms, all 43 consumed. Final crop
4096x3584 at 8x. Largest in-play allocation now 64 MiB / 3.495 ms. Former
loading fields 8936 and 9740-9742 have 17.969 / 14.967 / 16.739 / 15.463 ms
presentation intervals, with no allocation. This proves the reproduced
allocation mechanism moved out of gameplay on this route, not all loading
or campaign stalls are fixed. Overall p99 29.553 ms, max 102.074 ms, 44
intervals >33.37 ms, 15 >50 ms. Room max 50.307 ms; flight/pause max
34.343 ms. Separate source-production gaps around 8346-8433 remain: at
8395 ready interval 104.232 ms, host rendering only 2.129 ms, allocation
zero. Do not attribute that remaining maximum to the image allocator.
A final repeat profiles gfx at fields 8100-8500, after the window-resize
reservation guard; its binary hash and symbol map are preserved.

Final live repeat loading-reserve-repeat-4k-muted (final window-guard build):
10,800 fields, exit 0, every source ID present. 43 reservations created in
251.35 ms; all 43 consumed. Full-4K crop remains 4096x3584 at 8x. Mean
16.676 ms, p99 28.603 ms, max 38.731 ms; 11 intervals >33.37 ms and none
>50 ms. Room p99 32.152 / max 37.311 ms (nine >33.37); flight/pause
max 26.963 ms, none >33.37. Largest measured in-play allocation 64 MiB /
3.109 ms. No multi-GB image allocation recurs on either larger-reserve run.
Keep BOTH runs: the independent first-run 102 ms source-production stall
did not reproduce here, so do not select this repeat as proof of locked 60.

Targeted gfx sample at fields 8100-8500: 4,381 samples. Internal leading
symbols trx_stream_fast 395, flags_result 263, transform 252, VIF unpack
drain 210; ucrtbase module 902 and ntdll 564. Binary SHA256 and full symbol
map accompany host-summary.json. This is a profile of a non-reproducing
repeat, not causal attribution of the first-run 102 ms gap. Further CPU/
producer investigation should distinguish texture transfer/native model
work from waits and host scheduling. No CPU algorithm changed in this fix.

Final GPU regression: loading-reserve-gpu-parity runs the same captured 12
consecutive fields with the small reserve and budget-aware reserve, at Auto/
full-4K with native meshes/overlay retained. Every corresponding PPM is
byte-identical; final output is nonuniform/visible and 4096x3584 at 8x.
comparison.json stores per-field equality and SHA256. Includes capture
priming; this is image/lifecycle correctness, not a live frame-time benchmark
or post-mission replay/Alt-Tab acceptance. Both GPU runs exit 0. All bounded
game/replay tests are complete. Native/full-4K defaults remain enabled; no
public checkout changes or push.

### CPU row-copy optimization and production-pause tracing - 2026-10-18

Kept native hybrid, full output coverage, 3840x2160/Auto and budget-aware
loading preparation enabled. Other programs were not interrupted. The exact
earlier 102 ms source-production pause remains intermittent/unresolved.

An EE profile at 8100-8500 (cpu-ee-baseline-4k-muted) mostly samples the
guest interrupt polling loop: kernel poll 1,889 / 4,369, MMIO read 727,
func_00264520 596, func_00108F00 390. This does not identify the 102 ms cause
or justify modifying guest clock/scheduling. Its early timing sections overlap
short standalone copy tests; do not use it as a clean all-route benchmark.
The targeted sampling region follows those tests. No kernel behavior changed.

Optimized CT32 bulk transfers: check the whole 256-byte GS block once, then
copy adjacent even/odd 32-bit pixels in 8-byte pairs. Odd starts/tails, partial
VRAM edges, wrapped addresses and overlapping source/destination retain old
per-pixel ordering. Indexed T8 experiments did not improve reliably and were
rejected; its original byte loop remains. Source capture/progress/dirty state
logic is unchanged. Private PS2_TRX_ROW_SCALAR=1 selects the old row strategy;
normal launchers strip inherited PS2 flags.

715,904 differential helper cases pass. Separate real --gs-test paired and
scalar processes both pass, including 64 bulk-transfer layouts and complete/
partial/split chunks against per-qword VRAM/cursor/upload-recording results.
Isolated CT32 row benchmark (2 million calls per sample, five samples) is
1.33-1.63x faster, median 1.50x; it is a small cache-hot routine benchmark,
not an end-to-end FPS or memory-bandwidth claim. Full renderer suite: 34
executables pass. Logs: cpu-stalls-final-tests.log, cpu-gs-selftest-*.log.

Added optional PS2_GFX_TIMELINE: deferred, bounded per-field EE/queue/worker
timestamps and CPU counters, with producer thread identity. No per-packet
clock or gameplay file IO. Regular queue records and field depth are retained
when disabled. New traced marker payload is covered by real pthread queue
tests, including ring wrap, backpressure and trace overflow. Windows CPU
counters are coarse (observed 15.625 ms steps), not exact per-frame costs.
CPU deltas across different producer threads are invalid. Queue idle/wall
intervals help distinguish submitted work from producer gaps; waits/host
scheduling cannot be inferred from a CPU counter alone.

The first CPU optimized route cpu-row-native-4k-traced has valid renderer
timing (max 69.350 ms; room max 37.793; flight/pause max 23.183), but no
queue CSV: bounded ps2_finish exits through _exit and skipped worker cleanup.
Fixed the diagnostic exit path to join/flush the worker before video shutdown
when queue tracing is requested. A 60-field live smoke verifies CSV output,
zero overflow, clean exit. Do not cite the first route as queue-stage evidence.

Final optimized route cpu-row-final-4k-traced: 10,800 renderer and queue fields,
no missing source IDs, exit 0. Mean 16.678 ms, p99 26.841, max 46.611; eight
intervals >33.37, none >50. Room p99 29.001 / max 36.448 (two >33.37);
flight/pause p99 30.267 / max 46.611 (five >33.37). This flight section is
worse than the preceding trial despite a better room section; preserve both.
At source 10047: renderer ready gap 46.817 ms, EE submission interval 16.679,
queue age 81.534, worker idle zero, previous present 0.662, host rendering
4.029, no allocation. Worker CPU delta 31.250 ms is quantized. This is
backlog before rendering, not a long GPU allocation or EE submission gap;
it does not retrospectively prove the cause of the old 102 ms event.
A separate 43.663 ms presentation interval at 6106 has a normal 16.643 ms
ready gap and 27.396 ms renderer host time, showing more than one spike class.
Host sampler disabled for matched queue-timed comparisons. Native/full 4K
and all 43 loading-image reservations remain in use.

Matched scalar row rollback cpu-row-scalar-control-4k-traced: identical final
row-code build and queue tracing, but PS2_TRX_ROW_SCALAR=1; 10,800 fields,
exit 0, no missing source IDs. Mean 16.682, p99 30.262, max 100.411 ms;
24 intervals >33.37, two >50. Room max 39.254 (11 >33.37); flight/pause
max 100.411. Source 10283 is a comparable production pause: ready gap
102.886 ms, EE submitted after 17.131 ms, queue age 126.642 ms, worker idle
zero, previous present 0.196 ms, GS prepare 0.145 ms, renderer host 3.594
ms, allocation zero. Quantized worker CPU delta 46.875 ms does not account
precisely for the whole wall gap; do not label it 103 ms of computation.
This reproduces a similar graphics backlog class, not the old opening-stage
102 ms event at the same source/location. At source 1341 the control has
a distinct EE/producer gap: 59.640 ms submission and 43.682 ms worker idle.
The logs distinguish several stall mechanisms rather than one global cause.
Single sequential runs and host load cannot prove that all improvement is
from the CT32 copy optimization. Its isolated differential benchmark supplies
stronger evidence for the local arithmetic/copy cost change.

Second CPU change: specialize native flags_scalar instead of broadcasting a
float through the four-lane flags_result routine. It shares flags_advance
semantics and writes the same pending slot/mac/sticky status, using bit tests
for signed zero/NaNs. No vector transform or game timing changes. Added
100,000 complete-state comparisons with the previous broadcast path, all
pass along with existing independent flag/model continuation tests. Isolated
5-million-call benchmark, five samples: 1.41-1.69x, median 1.56x faster.
Again, not a whole-game speedup claim. Both executables rebuilt and all 34
renderer executables pass after this change (cpu-flags-final-tests.log).
Combined full-4K validation is cpu-combined-final-4k-traced, with the same
non-suspending queue trace and all image preparation enabled.

Combined candidate run cpu-combined-final-4k-traced: 10,800 renderer/queue
fields, exit 0, no missing source IDs. Mean 16.680, p99 30.991, max
49.571 ms; 40 intervals >33.37 and none >50. Room p99 34.053 / max
49.571 with 27 >33.37, worse than the row-only accepted run's two. Flight/
pause max 30.738, none >33.37. Preserve this result rather than reporting
only its better maximum. Aggregate worker CPU: row-only 79.094 s, scalar
row control 81.313 s, combined candidate 82.625 s. Workloads differ (native
model-space draws 6.834m / 6.181m / 6.180m respectively); these are not
controlled CPU speedup ratios. The candidate did not confirm end-to-end gain.

FINAL DECISION: reverted the specialized flags_scalar and its added test from
the working source; original scalar flag behavior remains. Saved the complete
candidate plus test as E:/ac4-renderer-evidence/scalar-flags-experiment.patch
and its benchmark/results for a future matched-workload investigation. The
100,000 new scalar-broadcast test was experimental, not part of the final
source. Existing independent model flag and continuation tests remain. Keep
the CT32 paired row optimization and the optional queue timing diagnostics.
The accepted full-route evidence is cpu-row-final-4k-traced; the combined
candidate is explicitly not the final implementation. Similar 100 ms worker
backlog reproduced in the scalar row control remains a performance concern;
no universal 102 ms fix or locked-60 claim. Full native/4K is still enabled.

Final accepted rebuild after holding back scalar-flag experiment: both native
executables linked; all 34 renderer executables pass again. Logs cpu-accepted-
final-build.log and cpu-accepted-final-tests.log. Final rebuilt GPU output
(cpu-accepted-gpu/final) matches the scalar CT32-row control exactly on all
12 consecutive full-4K capture fields. Final PPM is visible and 4096x3584
at 8x; includes priming and is not live frame-time/replay acceptance. Earlier
cpu-row-gpu-parity also matches all 12 fields with row optimization on/off.
Live native acceptance validator passes mesh/model/terrain/sky/cloud, native
mips/anisotropy and HUD coverage for cpu-row-final-4k-traced. All bounded
game/replay tests have completed. No new public checkout changes or push.

Remaining focus: identify and reduce the particular VIF/model/transfer bursts
that build graphics-worker backlog, distinguish CPU execution from host
scheduling/waits, and recheck the separate producer/renderer spike classes.
Retained CT32 optimization is locally faster and correct; global stability
is not established by these sequential runs. Full native/4K, 60-Hz game
timing, active muted audio mixing in background tests and unrelated programs
remain as requested. Save/load, broader missions, Alt-Tab, and perceptual
flicker/audio acceptance still require separate validation.


2026-10-18 graphics worker command attribution (native full 4K)
- Added opt-in PS2_GFX_JOB_TIMING with PS2_GFX_TIMELINE. Bounded per-field/opcode counts, total wall time, slowest record and its byte size are written only after worker shutdown to .jobs.csv. Disabled in ordinary play. Regression tests cover field attribution, delayed commands, wraparound and bounded overflow without dropping game work.
- Muted gfx-jobs-4k-traced completed 10800 fields, no missing source IDs and native/HUD acceptance passed. Heavy briefing fields contain 40758 single-word VIF commands plus 11716 VIF transfers. Field 7418: 59.543 ms measured command work, dominated by VIF words (48.826 ms), presentation 0.193 ms, no allocation. Per-command clocks materially perturb this dense workload; do not use this run for an acceptance/performance comparison.
- Trial: direct copy for complete, unmasked VIF vectors without MODE accumulation. 262144 full-state/memory differential cases, all 35 trial test executables, real VIF self-test and 12 consecutive full-4K GPU images (byte-identical to control) passed. The live route with packet clocks OFF still had 123.257 ms worst interval; 19 above 50 ms, all in the briefing range. Flight range 9900..10799 max 28.345 ms, no interval above 33.37 ms. All 10800 source IDs retained. Field 7088 worker wall 119.213 ms, reported CPU 31.25 ms (coarse counter), idle zero, renderer host 4.496 ms, allocation zero. This is not 119 ms of established CPU computation; scheduling/non-idle waits require further attribution.
- FINAL DECISION: held back the VIF trial and rebuilt both executables with the prior VIF emitter. Candidate block, differential test and logs saved privately under E:/ac4-renderer-evidence. Current production change is diagnostic capability only; no demonstrated improvement to the intermittent 100 ms stall claimed. Native hybrid/full-output 4K defaults and ordered fields retained. Focused briefing worker profile follows; sampler timings are diagnostic, not acceptance metrics.
- Focused restored-build profile gfx-room-profile-4k completed 8200 fields muted, native/full-4K, targeted worker samples only at fields 6900..7500; 6553 samples, matching executable hash/symbol map retained. Largest internal leaf samples: flags_result 565, transform 553, rn_ac4_model_eval 401, trx_stream_fast 365, lit_visible_flags 275, vif_unpack_drain 213, unlit_clip_build 213, vif_exec_code 196, flags_advance 175. This supports targeting native model transform/flag/evaluation work ahead of further emitter micro-optimization (110 samples). UCRT module had 1028 samples; nearest export hints are not stacks or reliable proof of an individual allocator/copy routine. Next investigation: reduce repeated model/flag reconstruction while preserving VU state, and separately capture scheduling/wait time during long wall/CPU gaps. Built-in VIF self-test passes on the restored final executable. No test processes remain running.


2026-10-18 bounded native transforms and field-completion wakeups
- Retained a conservative ordinary-value SSE2 path in transform/flags_transform. Nonzero matrix/position values must lie in [2^-30,2^30]; under these bounds ordered single-precision products/sums cannot overflow or produce a nonzero subnormal. Signed zero, operation ordering and all four VU flag writes are retained. Inputs outside the guard use the existing fully clamped implementation. Private PS2_AC4_TRANSFORM_CONSERVATIVE=1 restores that implementation for comparison.
- Extended existing model tests to 200000 bit-exact transforms and full flag-state traces (arbitrary bits plus guard boundaries, cancellation, signed zero and finite in-range values). Existing 100000 flag-mask traces and 10000 delayed-pipeline cases retained. All 34 renderer test executables passed. Isolated five-repeat, actual-helper benchmarks: ordinary transforms 1.566-1.995x faster, flagged transforms 1.263-1.564x. These are helper gains, not whole-game speedups. Twelve consecutive native/full-4K GPU images remain byte-identical to the previous control.
- Added optional field-level worker CPU cycles and cumulative batch/queue-lock/notification wall clocks, without packet clocks or hot-path IO. In bounded-transform-4k, a reproduced 119.853 ms presentation interval at field 7336 had 120.258 ms worker wall, 61.691 ms command-batch wall, 0.023 ms queue-lock acquisition and 58.545 ms outside batches/idle/lock. CPU time was 78.125 ms (coarse), cycles 145.560M. This narrowed the unexplained part to bookkeeping/scheduling, not allocation or presentation. It does not establish the exact cause of the previous run's 123 ms event.
- Found shared-condition wakeups: field-depth waiters were awakened on each 256-record ring-progress batch although only a field marker can satisfy their condition. Added a separate field-completion condition; full-ring waiters still receive batch-progress notifications. Field order, depth and backpressure unchanged. Private PS2_GFX_FIELD_WAKE_LEGACY=1 restores the old shared condition.
- Extended actual-queue tests to count one field notification per completed field despite 600 preceding single-word commands, and to force full-ring waits with 512 KiB records, a 1 MiB ring and eight pending fields. Normal/traced/legacy/ring-full/overflow cases pass; no lost payloads, order or forward progress.
- Matched notification comparison (native/full-4K, muted, same executable/transform path, 10800 fields each, no packet clocks or host sampler): field-wake-split-4k mean 16.6755 ms, p99 28.6369, worst 85.8743, 18 over 33.37 ms / 3 over 50. Briefing range 6300..8099 p99 31.7121, max 52.7956, 7 over 33.37 / 1 over 50. Flight 9900..10799 max 24.0326, none over 33.37. Legacy flag: mean 16.7146, p99 34.9295, max 85.6060, 142 over 33.37 / 16 over 50; briefing p99 46.9845, max 85.6060, 125 over 33.37 / 13 over 50. Flight max 24.7679, none over 33.37. All source IDs retained in both.
- Direct notification attribution: briefing cumulative notification intervals in split mode total 10.8445 ms, max 0.0516 ms per field; legacy total 310.8218, max 26.5945. Legacy fields 7529/7642 spent 26.595/25.606 ms in notification calls. These are wall intervals and include any descheduling there. This supports retaining the separated condition, but does not prove all 100 ms stalls resolved or give a controlled global FPS gain.
- Remaining distinct stalls: split run field 5995 had an 85.874 ms presentation interval, 86.084 ms producer gap, worker idle 84.579 ms, queue age 0.033 ms, renderer host 0.431 ms, allocation zero. Field 6498 instead spent 54.116 ms inside command batches, with 31.25 ms coarse CPU, 131.771M cycles and no idle/allocations. Need separate producer/clock attribution and further model/flag CPU reduction. Final repeat enables existing PS2_CLOCK_TIMELINE to distinguish timer wake lateness from guest delivery/work.
- Model-only first A/B did not establish a whole-route speedup: fast/shared-condition run max 119.853 ms and 72 intervals over 33.37 versus conservative/shared-condition max 61.5688 and 51 over 33.37. Briefing median cycle counts were 81.294M vs 83.506M, but workload timing/host variation prevent a global CPU ratio claim. Keep claims limited to bit-exact helper gains and the separately measured notification improvement.
- Final configuration repeat field-wake-final-clock-4k: 10800 fields, no missing IDs, native/HUD acceptance passed. Mean 16.6830 ms, p99 29.7651, worst 78.2373; 38 over 33.37 / 3 over 50. Briefing max 78.2373, p99 36.6795, 28 over 33.37 / 3 over 50. Flight max 30.0640, none over 33.37. Worst worker interval at field 6991 was 82.6204 ms, 82.5701 in command batches, notification 0.0094 ms, lock 0.0196 ms, no idle: substantial model/command work remains after the wakeup fix. No claim that intermittent 100 ms pauses are eliminated.
- Clock attribution at field 9594: producer gap 47.4318 ms, worker idle 25.5726 ms, previous interrupt delivery only 0.0465 ms. Timer deadline was 9.309 ms after prior producer-ready timestamp but woke at 45.728 ms (36.4197 ms late); the next two overdue ticks woke immediately. Delivery resumed at 47.390 ms, with two ticks pending, then caught up. This identifies a late host timer wake in this event, rather than a long VBlank handler or graphics notification wait. It does not identify which OS/host-load mechanism delayed the timer, and the previously observed 85 ms producer gap was from a different run without a clock trace.
- Both optimized and legacy notification routes and both transform comparisons passed native/HUD route acceptance. Native/full-4K defaults, ordered fields, quality, audio mixing/timing and user saves retained. Final executable has bounded transforms plus separated field wakeups by default; diagnostic flags are only injected into private test processes. All runs completed; no game/test process remains running. Remaining priorities: expensive model/flag command batches and late host timer/producer gaps. No public checkout changes, commits or pushes.
