# Native renderer roadmap

The current accepted model runs recompiled CPU and VU code through the Vulkan GS compatibility renderer. Preserve this model while developing direct rendering in a separate build configuration.

1. Identify AC4 VU programs, input layouts, transforms, material state and output packets. Validate against the existing GS path; do not enable AC5 address/layout assumptions for AC4.
2. Implement one opt-in native mesh path with a narrowly defined scene/aircraft test. Retain GS fallback for unsupported programs and effects.
3. Verify ordering, clipping, depth, lighting, textures, shadows and scene transitions when native meshes and GS draws share a frame. Measure frame-time distributions, not only average FPS.
4. Extend coverage to terrain and additional models only after visual and behavior comparisons pass.
5. Add AC4-specific HUD/radar/caption rendering at window resolution, then a documented texture replacement mechanism for fonts, menus, briefings and terrain.
6. Improve PC-facing options and test save/load and focus changes. Keep user-confirmed movie/audio behavior as a regression check.

The native mesh and replacement UI work is planned, not implemented by this source publication. No performance gain is guaranteed solely by switching paths.
