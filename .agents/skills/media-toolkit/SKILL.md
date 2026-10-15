---
name: media-toolkit
description: Process RelayCore audio sources and playtest recordings with clipping, conversion, analysis, captions, thumbnails, GIFs, and batch utilities. Use for media preparation, not audio-engine changes or runtime asset staging alone.
---

# Media Toolkit

Use this suite for practical audio and video operations that were previously spread across many narrow skills.

## RelayCore use

Prepare sounds for equipment, terminals and UI, or process gameplay recordings and other requested media. Keep the original source and use a separate output path. For sound-event behavior or playback changes, inspect the existing `AudioSystem` and game owner instead of treating these authoring scripts as engine code.

Read the adjacent asset README, existing generator and consumer before choosing channels, sample rate, encoding, duration or gain. `assets/audio/player/README.md` records the current player-sample convention; it is not a universal format rule for every sound. Preserve the audible role and relative levels of effects; peak/RMS normalization in these scripts is not a verified LUFS measurement. Generated tones and noise are useful signals or source layers, not proof of a finished gameplay sound.

Use `relaycore-assets` when adopting an output into source assets, staging or runtime references. Inspect/listen to the result and verify the affected consumer when delivering game audio. Edited recordings are supporting evidence; retain the source and time range, and do not use a montage to claim an uninterrupted playtest or measured frame times.

## Included Tools

- Audio: `audio_analyzer.py`, `audio_converter.py`, `audio_normalizer.py`, `audio_trimmer.py`, `podcast_splitter.py`, `sfx_generator.py`
- Video: `video_captioner.py`, `video_clipper.py`, `video_metadata_inspector.py`, `video_thumbnail_extractor.py`, `gif_workshop.py`, `thumbnail_gen.py`, `timelapse_creator.py`

## Workflow

1. Identify the medium, input format, and target output.
2. Pick the narrowest script that completes the job.
3. Keep transformations explicit: clip times, output codec, target size, caption source, or thumbnail cadence.
4. Verify output duration, dimensions, and bitrate-sensitive settings when the request depends on platform limits.

## Guardrails

- Avoid recompression churn when a simple trim or extract is enough.
- Call out when generated captions or “best frame” choices are heuristic.

## Windows execution

Resolve tools in `scripts/` relative to this skill. Read the selected script's `--help`; create the output directory under an ignored task directory in `build/` before running it. Use an existing suitable Python environment, or an isolated environment under `build/skill-tools/`; [references/windows.md](references/windows.md) covers dependencies and PowerShell examples. Install only the packages needed by the chosen operation. Authoring packages do not belong in the engine's vcpkg manifest.

Keep the full upstream tool set available, including captions, podcast splitting, GIFs and batch operations. Select only the requested operation; changing a source format or adding these skills does not call for a game build, full content bake or release archive. Upstream pin and local adaptations are recorded in [../MEDIA_SVG_SOURCES.md](../MEDIA_SVG_SOURCES.md).
