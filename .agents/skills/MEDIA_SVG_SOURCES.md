# Media and SVG skills in RelayCore

Installed with the system `skill-installer` into repository-local skill directories. These are authoring tools, not engine dependencies or runtime format changes.

| Source | Pinned commit | Installed directories |
| --- | --- | --- |
| https://github.com/dkyazzentwatwa/chatgpt-skills | `103b4308f65df393e76765cb1ce12395d9cf5106` | `media-toolkit`, `svg-precision-skill` |

Local adaptations preserve the upstream workflows, full script inventory, SVG specification, recipes and examples:

- Entrypoints route visual decisions, RmlUi integration and runtime delivery to the existing RelayCore skills and owning documents.
- Windows references explain isolated Python environments, FFmpeg/Cairo availability, output verification and ignored working directories.
- Agent UI metadata uses the Codex `interface` schema and skill-specific prompts.
- Media requirements constrain MoviePy to the 1.x API used by the unchanged scripts and add the Python 3.13+ `audioop` compatibility package for pydub.
- SVG rendered comparison computes bounds across all RGBA bands. Upstream `getbbox()` missed RGB-only changes when alpha was unchanged; three rendered regression cases cover identical images, color changes and combined RGB/alpha bounds.
- The same rendering helper closes SVG input files explicitly; the rendered tests exposed unclosed-file warnings on Windows.

Upstream Python scripts are unchanged except for the narrow comparison correction in `svg_skill/render.py`; SVG references and examples are unchanged. Optional Python/native tools are checked per operation; installing the skills does not install global packages or alter the vcpkg manifest. Verification evidence for this adoption follows below.

## Adoption verification — 2026-10-12

Used Python 3.12.14 in `build/skill-tools/.venv`, MoviePy 1.0.3, Pillow 12.3.0, CairoSVG 2.9.1 and the installed FFmpeg/FFprobe/Cairo. Verification packages and generated files remain under ignored `build/skill-tools/`; no global Python installation was modified.

- System `skill-creator/scripts/quick_validate.py`: both entrypoints passed. UI metadata, local Markdown links and Python syntax were checked separately.
- `svg_cli.py build`, `validate`, `render`: all five upstream example specs passed, including PNG dimensions with SVG units accounted for. Missing-viewBox input was rejected. The diagram preview was visually inspected.
- The original rendered comparison reported black/red opaque SVGs as identical. After the local correction, the CLI reports a change (exit 1), and `build/skill-tools/.venv/Scripts/python.exe -m unittest discover -s .agents/skills/svg-precision-skill/scripts/tests -v` passes all three rendered cases. The render helper now closes its input files; no unclosed-file warnings remain in those tests.
- Media CLI checks: tone/fades, WAV conversion, trimming and peak normalization passed mono/PCM16/44.1 kHz, duration and sample-peak checks. Video clipping and frame extraction passed duration/dimension/audio-stream and PNG checks. Audio timestamp tests use the documented three-digit millisecond field; pydub slicing is checked within one sample.
- Test command inventory and generated fixtures: `build/skill-tools/checks/commands.json`. Captions, colored noise, podcast splitting, GIF/batch modes, Python 3.13 compatibility and runtime game integration were not exercised. They remain available; this smoke check is not a full upstream audit or a game-audio listening pass.

Verification scope is the skills and their authoring tools. Engine source, CMake, vcpkg and authored game content are unchanged; no engine build or release package is required for this adoption.
