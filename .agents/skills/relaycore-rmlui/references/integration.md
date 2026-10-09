# Integration decision points

Recheck these against the selected release; they do not describe an already implemented RelayCore integration.

## Sources

- [Upstream repository](https://github.com/mikke89/RmlUi): selected tag's `Include/RmlUi/Core`, CMake and `Backends`, including `RmlUi_Platform_SDL.*`, `RmlUi_Renderer_VK.*` and `RmlUi_Backend_SDL_VK.cpp` where present.
- [Documentation version policy](https://mikke89.github.io/RmlUiDoc/): online documentation tracks master.
- [Application integration](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/integrating.html) and [interfaces](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/interfaces.html): initialization and interface lifetime.
- [Render interface](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/interfaces/render.html): callbacks, color conventions and optional features.
- [Input](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/input.html), [contexts](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/contexts.html), [fonts](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/fonts.html), [data models](https://mikke89.github.io/RmlUiDoc/pages/data_bindings/model.html).

## SDL3 input and DPI

Adapt the selected release's SDL key/modifier conversion where appropriate. RelayCore retains SDL initialization, window and event-loop ownership.

Define input ownership across gameplay, game menus and ImGui tools. `Process*` return values are not a uniform capture flag: key functions return true for unconsumed events, while mouse functions can report lack of interaction. Verify each API and backend wrapper. A modal game menu must block gameplay even when a widget does not consume an event.

Map SDL mouse button indices explicitly. Submit committed SDL text as UTF-8, independently of key events. Verify available text-input-handler/IME support before claiming composition support. Tie SDL text input and cursor/relative-mouse mode to the active owner. Clear held gameplay input and UI hover/drag state as appropriate on focus/owner changes; preserve release handling to prevent stuck keys/buttons. Test Escape, Enter, Tab/Shift-Tab, wheel and Resume clicks leaking into gameplay.

Keep context dimensions, pointer coordinates, viewport/scissor and framebuffer pixels consistent. Convert SDL logical coordinates explicitly when required; set context density from actual display scaling. Test resize and monitor/DPI changes for double scaling or displaced hit targets. Verify passive HUD `pointer-events` and focus behavior so it does not intercept gameplay.

## Vulkan integration

Inspect upstream loader/header assumptions, allocator ownership, shaders and frame lifecycle before reuse. Integrate with RelayCore's device, queue and presentation flow. Avoid a second swapchain/present loop or conflicting Vulkan loader/VMA setup.

Use release headers for geometry callback signatures; older examples may be incompatible. Preserve draw order, translation, top-left pixel coordinates, scissor, winding and generated-texture orientation. Generated textures and vertex colors use sRGB with premultiplied alpha. Match blending, commonly ONE / ONE_MINUS_SRC_ALPHA, and verify color conversion in the real SDR/HDR path, including translucent text edges.

Match authored effects to implemented callbacks. Rectangular scissor cannot provide transformed/rounded clipping; filters/layers and gradient shaders need their optional features. Declare unsupported effects rather than silently dropping them.

Handles must resolve to owned geometry/texture resources. Retire buffers/images/descriptors after outstanding GPU use completes, including document closure and presentation recreation. Establish the required Vulkan state for subsequent scene/ImGui draws. Keep shaders/resources in the actual repository build/staging path.

## Lifecycle and update order

Install interfaces before initialization; keep them alive through `Rml::Shutdown()`, which uses non-owning pointers. Tear down documents/models/listeners while callback targets and render device still exist. Retire UI GPU work before device destruction. Check release headers for document closure/ownership; do not assume all returned pointers transfer ownership.

Make the update order explicit: input dispatch; game actions at a safe boundary; presentation data and dirty bindings; context update; UI rendering in the intended composition phase. Inspect scene tone mapping and display conversion before choosing that phase. Use a suitable UI clock so menu interaction remains responsive during paused simulation.

Use existing logging/file facilities when defaults cannot express the asset root or useful diagnostics. Load failures should identify the asset. Preserve models/documents across ordinary frames; add reload support only when requested or already in the owning workflow.
