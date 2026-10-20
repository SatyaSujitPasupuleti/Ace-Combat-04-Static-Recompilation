# Credits and licenses

- **Ace Combat 5 Static Recompilation / sal063**: https://github.com/sal063/Ace-Combat-5-Static-recompilation — direct source-code foundation, based on `078662b4018b5e79d5aa5ee9b9c60aceec9e7649`. Includes the PS2Recomp Python translator, runtime, Vulkan renderer, VU tooling, and other infrastructure. Retained under the root Apache-2.0 license. AC4 modifications include game profiles/hooks, IOP/audio/movie compatibility, VU generation integration, GS/texture/performance adaptations, and launch/build tooling.
- **Dear ImGui**: https://github.com/ocornut/imgui — vendored UI code; see `third_party/imgui/LICENSE.txt`.
- **Lua**: https://www.lua.org/ — vendored scripting runtime; see `third_party/lua/LICENSE.html` and `VENDORED.md`.
- **SDL3**: https://github.com/libsdl-org/SDL — external window/input/audio dependency.
- **Vulkan**: https://www.vulkan.org/ — graphics API; external loader/headers/shader tools.
- **Ghidra**: https://github.com/NationalSecurityAgency/ghidra and **ghidra-mcp**: https://github.com/bethington/ghidra-mcp — development/reverse-engineering tools used during investigation, not required to run or build the game and not bundled here.

Ace Combat and the original game's code, art, music, text, and trademarks belong to their respective rights holders. The root source license does not grant rights to the original game. Supply your own supported ISO. No game download is provided.

Fresh Git history is a packaging choice, not a claim that inherited code was independently written. Existing source-level notices and bundled dependency licenses are retained.

- **PresidentRaptor / Ace Combat 04 PC Port**: https://github.com/PresidentRaptor/Ace-Combat-04-PC-Port - its Apache-2.0 renderer field-preservation and presentation synchronization fixes and technical notes informed our AC4 ordered-field handoff adaptation (2026-10-18). Our adaptation preserves pending draws across timed wait intervals and keeps the native mesh/HUD renderer.
