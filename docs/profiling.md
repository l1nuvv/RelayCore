# Profiling in RelayCore

## Recommended local preset

Use `msvc-relwithdebinfo` for local profiling work.

This preset enables:
- `RelWithDebInfo`
- `CMAKE_UNITY_BUILD=ON`
- `RELAYCORE_ENABLE_PCH=ON`
- `RELAYCORE_ENABLE_TRACY=ON`

## Tracy

When `RELAYCORE_ENABLE_TRACY=ON`, the engine links `Tracy::TracyClient` and enables `TRACY_ENABLE`.

Current instrumentation points:
- `Application::run`
- `Renderer::render_frame`
- frame markers once per rendered frame

To profile locally:
1. Build and run `RelayCore` with the `msvc-relwithdebinfo` preset.
2. Launch Tracy Profiler locally.
3. Connect to the running client.

## RenderDoc

RenderDoc remains an external runtime tool and is intentionally **not** wired into CMake presets or CI.

Recommended use:
1. Run `RelayCore` locally.
2. Attach RenderDoc to the process.
3. Capture a frame while rendering the cube / future scene.

This keeps the build system simple while still giving you frame-level GPU inspection when needed.
