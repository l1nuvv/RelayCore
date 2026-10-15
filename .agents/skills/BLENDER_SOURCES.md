# Blender skills in RelayCore

Installed into this repository with the system `skill-installer`, with supporting references, scripts and agent metadata. These are authoring instructions/tools, not engine dependencies or an MCP server installation.

| Source | Pinned commit | Installed directories |
| --- | --- | --- |
| https://github.com/ifBars/blender-agent-studio | `9191be7ee2b5ce4bd0596fadcddf8f7b0c6ec3b1` | `blender-modeling-workflow`, `blender-iterative-refinement`, `blender-asset-validation`, `blender-rendering-workflow`, `blender-procedural-workflow` |
| https://github.com/libevm/agent-skills | `cc98ad8cd54fcb1ef365c6b738559f418fddef89` | `blender` (source path `skills/blender`) |

`relaycore-blender` is the project-owned integration skill. Local adaptations to Blender Agent Studio: installed specialist invocations omit the plugin namespace; each entrypoint links to the project integration skill. Modeling intake uses `relaycore-art-direction`; character/animation specialists outside the installed set are conditional, not required routes. Entrypoints load only task-relevant sections of the identical `astra-workflow.md` copies and reuse already-read sections across specialists. Copies remain in place for standalone compatibility. Upstream Python/TypeScript helpers are unchanged. Read only the specialists relevant to the requested work.

Blender supplies `bpy`, `bmesh`, `mathutils` and NumPy for the scripts. No global Python packages are required. Optional Poly Haven lookup runs with Bun or a Node version supporting native TypeScript stripping. MCP, Rust SceneIR, hosted generation and benchmark tooling are not installed or required.

## Upstream notice: Blender Agent Studio

MIT License

Copyright (c) 2026 Bars

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
