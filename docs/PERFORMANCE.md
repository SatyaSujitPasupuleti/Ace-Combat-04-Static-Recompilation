# Performance work log

Updated 2026-10-19. Consolidated record for the local V3 native build.
Detailed evidence: [V3 renderer](V3-RENDERER.md) and [AC5 parity investigation](AC5-PARITY.md).

## Current state

Native Vulkan hybrid rendering, full output coverage, 3840x2160 output/Auto,
60 FPS cap and ordered fields remain the target. No frame generation, reduced
resolution or game-speed increase is used to claim improvements. Full output
coverage does not replace the original texture artwork.

The user reports that frame drops are reduced but still occur. **Locked 60 FPS
is not achieved.** A near-59.94-Hz average can hide 30-40 ms intervals and longer
pauses. The latest experimental upload optimization was rejected; the working
runtime and both executables were restored to their prior hashes.

## Retained implementation work

| Area | Work retained | Evidence / limits |
|---|---|---|
| Hybrid model renderer | Supported AC4 coordinates/constants use Vulkan indexed meshes and vertex shaders, native materials, supplied mips and anisotropic filtering, alongside GS fallback. | Guarded coverage, not complete VU/GS replacement or an automatic speedup. |
| Duplicate model calculations | Non-audited mode uses original packet outputs instead of recomputing reference lighting/UV/fog; eligible packets bypass generic GIF decoding while preserving GS/parser state. | Images/counts match. A hangar case avoided 45,252 reference vertex evaluations and decoding 34,299 payload quadwords; operation counts, not FPS. |
| Per-activation reuse | Cache immutable vertex clipping/projection safety within one activation, reject empty packets early, reuse identical complete model constant blocks within the current arena. | Invalidate on new activation/arena; mutable attributes remain checked. No persistent disk cache. |
| SIMD transforms/flags | Four-lane transforms retain separate arithmetic, explicit flush/clamp/NaN behavior; drain at most four pending flag slots rather than idle cycles; retain flag-mask SIMD. | Differential/image tests. Older room runs improved some metrics but worsened others; no blanket fast-math. |
| Bounded transform fast path | Ordinary finite inputs within conservative bounds use a shorter SSE2 path; other inputs keep the clamped implementation. | 200,000 bit-exact transform/full-flag traces. Isolated ordinary helper 1.566-1.995x and flagged helper 1.263-1.564x; not whole-game gains. |
| VU AOT coverage | Added missing cockpit entries, private program-corpus merging and deferred fallback reporting. Default native CPU replacements plus AOT retained; AOT-only CPU policy selectable. | Nine generated programs in tested corpus; measured routes had zero interpreter/JR fallbacks. Unknown scenes can still fall back. |
| Pipeline preparation | Prepare known device-independent render states before guest startup; expanded to 96 states, including Mission 2 key 000302000064011F. | Reproduced approximately 300 ms compile moved before gameplay. Source field 12041 interval 336.55 to 36.84 ms on follow-up. Startup still pays compilation; unknown states remain on demand. |
| Stable full-4K Auto scale | Cover known flight crop at startup, avoiding menu/room 8x/6x/8x rebuilds; keep window/hardware limits. | Removed reproduced scale rebuild mechanism, not all lazy image allocations. |
| Budget-aware image preparation | Prepare target/depth/snapshot/composition images before gameplay if GPU budget permits; smaller fallback reserve, pressure checks and unused-reserve release retained. | Tested adapter prepared 43 images, approximately 5.93 GiB committed earlier. Two runs removed reproduced multi-GB loading allocations; largest in-play allocation 64 MiB / approximately 3.1-3.5 ms. Trades startup time/memory for fewer later allocations. |
| CT32 upload rows | Check whole resident blocks and copy adjacent 32-bit pixels in pairs; retain scalar ordering for odd ends, clipping, wrap and overlap. | 715,904 differential cases, real GS transfer tests and 12 identical full-4K frames. Isolated helper median approximately 1.50x, not game FPS. |
| Field completion wakeups | Separate field-depth waiters from ring batch-progress waiters, preserving order/depth/backpressure. | Briefing notification wall time 310.82 to 10.84 ms in matched routes. Independent long stalls remain. |
| Texture retirement | Order eligible victims once per collection batch, preserving exact age/slot policy and GPU/queued-frame lifetime protection. | Exact order/failure/lifetime tests; included in worker comparison below. |
| Mip atlas preparation | Copy contiguous wrapped mip rows; search resident cache after direct-cache collisions before rebuilding. | Preserves supplied texture/mip content. Legitimate in-process reuse is part of the improvement. |
| PSMT8 decoding | Precompute indexed byte offsets at GS initialization, preserving pixels, palettes and hashes. | 622,592 decode cases. Cold replay median 21.570 to 21.285 ms/field, small 1.32% observation under external load. |
| Memory disc option | ISO preload via -DiscCache memory reduces dependence on runtime disc reads. | Startup/RAM tradeoff, not proof a particular spike is fixed. |
| Screenshot output | Convert/write RGB in rows with exact crop/stride/pixels. | Reduces diagnostic capture overhead; ordinary play without screenshots does not benefit. |

## Measurement and correctness work

- Optional bounded frame, graphics-worker and clock timelines separate producer
  gaps, queue waits, worker command work, notifications, allocation and Vulkan
  host stages. Field records are deferred to avoid gameplay file IO. Dense
  command clocks and host samplers can perturb timings and are diagnostic.
- Mission 2 navigation uses private save copies and recorded input. Complete
  90-field late-flight and 56-field early-flight DMA captures permit identical
  work comparisons. Capture speed itself is not gameplay performance.
- Metrics runs and isolated application-cache prefixes support comparison;
  retain raw slow runs, settings, build/save hashes and fallback counts.
- Ordered source fields and atomic display-state/draw-list handoff protect
  correctness. Dropping fields is not an acceptable FPS optimization.
- Final restored build: 152 public tests and 37 renderer executables passed.
  Extra lowest-vacant-slot and allocation-failure texture tests were retained
  even though the slot-search experiment was removed.

## Cache-independent evidence and cache limitations

Retained retirement/mip-atlas changes were compared on identical captured work,
in fresh processes with reversed arm order and three passes per process:

| Application pipeline cache | Control median ms/field | Fixed median ms/field | Reduction |
|---|---:|---:|---:|
| Empty | 24.635 | 21.845 | 11.3% |
| Identical warm seeds | 24.395 | 21.705 | 11.0% |
| Disabled | 24.415 | 21.595 | 11.6% |

All 12 final images and work counts matched. Cold first passes also improved.
These are replay-throughput figures, not live FPS or per-frame percentiles.
Known pipeline preparation also works without a persisted application cache.

**Cold means an empty application pipeline-cache prefix.** Driver shader and
Windows file caches were not purged. Runtime texture reuse remains enabled;
later passes retain in-process state. Memory disc preload is independent.
We have not proved fresh-machine performance or established how much caching
helped the user's latest manual run. Cache-off also disables startup pipeline
preparation, so it is not identical to normal cold-start behavior.

## Repeated live baseline

Four native/full-4K runs retained all 68,400 requested source fields. Common
flight window 12900..16199 contains 3,300 intervals per run:

| Run | CPU mode | Flight p99 ms | Flight worst ms | Above 33.37 ms |
|---|---|---:|---:|---:|
| Cold extended | AOT | 29.85 | 37.17 | 6 |
| Warm repeat | AOT | 30.08 | 36.99 | 3 |
| Cold repeat | AOT | 29.25 | 36.75 | 5 |
| Cold default | Native replacements | 30.09 | 39.40 | 1 |

No missing/duplicate fields, gameplay pipeline builds or interpreted VU in
those routes; original saves unchanged. Live progress/load differs, so these
are not controlled CPU-policy comparisons. Do not select an earlier best single
run as proof of stable 60. Timestamps are CPU presentation timings, not scanout.

## Rejected or unadopted experiments

| Experiment | Outcome |
|---|---|
| Cursor texture replacement | Removed; retain established victim order. |
| Earlier T8 block copy | Correctness/image checks passed; cold median 22.45 versus 21.61 ms control, removed. |
| Specialized scalar VU flags | Helper gains did not hold reliably across route; reverted. |
| Direct unmasked VIF copies | Differential/image checks passed, but live route still had 123.26 ms worst interval; held back. |
| Register-resident flags | Private opt-in only; no reliable whole-route gain. |
| Blanket round-toward-zero/FTZ | Arithmetic counterexamples remain; not enabled as a general bit-exact PS2 solution. |
| Small JIT / alternate GS investigation | Isolated research, not installed in game path. |
| Lowest-vacant texture-slot hint | Unaligned version regressed; aligned version no useful gain; removed. |
| Latest SSE2 PSMT8 column uploads | 544,320 oracle cases and matching images, but mixed early/late results and failed live acceptance; removed and original binaries restored. |

Latest column prototype: early-flight cold first passes improved 8.26%, late
flight regressed 2.30%. Live flight p99 35.036 ms, 56 intervals above 33.37 ms,
worst 104.836 ms. An earlier harness incorrectly set a presence-based disable
variable to 0, still disabling the prototype. Those preliminary comparisons
are invalid; corrected enabled tests remove the variable entirely.

## Remaining targets and acceptance gaps

1. Worker model/flag/GS preparation/transfer bursts remain over budget. Latest
   rejected-test examples spent approximately 46-49 ms in worker apply, with
   small renderer host costs and zero traced allocation.
2. Renderer handoff/event/scheduling is a separate target. Rejected-test source
   15793 had a 104.836 ms interval, including 69.532 ms AFTER publication before
   renderer latch; worker apply 34.434 ms, Vulkan host 4.104 ms. This locates a
   delay but does not prove its cause or persistence in the restored build.
3. Independent producer/clock gaps and swapchain-acquire outliers also remain.
   Do not attribute all pauses to shaders, allocations or one worker function.
4. Mission 1 flight stalls, below-50-FPS Mission 2 sections, UI/replay flicker,
   focus changes and full tours require broader testing.
5. Recent automated routes skip the opening and mute output with mixing active.
   They do not accept opening/movie sync, audible crackle/desync, save-write/load
   roundtrips or the campaign. Earlier user-confirmed movie fixes remain
   regression requirements, not results of these muted tests.

Future candidates must preserve native/full-4K quality and source-field order,
pass state/image checks, improve matched workloads, and hold over repeated
live cold application-cache runs. Report p99, worst, counts above 33.37/50/100 ms,
averages and startup costs. Do not infer smoothness from average FPS alone.

## Private evidence index

Under E:/ac4-renderer-evidence/mission2-20261019/:

- WORKER-FIX.txt, CACHE-ISOLATION.txt, INDEXED-DECODE.txt: retained changes.
- STABILITY-REPEATABILITY.txt, stability-combined.json: repeated live baseline.
- WORKER-NEXT-EXPERIMENTS.txt, columns-isolated-comparison.json: rejected trials.
- columns-live-native-cold/summary.json and spike-detail.json: rejected live run.
- before-worker-next/: restored original source/executable backups.
- worker-restored-public-tests.log, worker-restored-renderer-tests.log: final checks.

Earlier allocation/transform/wakeup evidence is indexed in AC5-PARITY.md under
2026-10-15 and 2026-10-18 entries. Captures, generated microcode, saves and images
remain private/ignored. Local evidence paths are not source dependencies.
No runtime changes, benchmarks, commits or pushes were made for this note.
