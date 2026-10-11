# Media tools on Windows

Use an existing Python environment with the selected script's dependencies, or create an isolated one under `build/skill-tools/`. Invoke its interpreter directly from PowerShell; activation is optional. Keep `ffmpeg` and `ffprobe` available on the process PATH for encoding and inspection. The Python package `ffmpeg-python` does not provide these executables.

The full optional dependency inventory is `scripts/requirements.txt`; do not install the whole inventory for a single operation. Common paths:

| Operation | Python packages |
| --- | --- |
| Tone/white-noise/DTMF generation to WAV | NumPy, soundfile; SciPy for the colored-noise branch |
| Audio conversion, trimming, normalization | pydub; NumPy for normalization |
| Video clipping and frame extraction | MoviePy 1.x, Pillow, NumPy |
| SVG-independent GIF and thumbnail helpers | Read their imports; MoviePy/Pillow/NumPy cover the common paths |
| Detailed audio or video analysis | Read the selected method's imports; librosa/SciPy or pandas/ffmpeg-python may be needed |

The upstream video code imports `moviepy.editor`; keep MoviePy below 2 rather than silently migrating the scripts. pydub needs `audioop-lts` on Python 3.13+ because the standard-library `audioop` module was removed. TextClip-based caption rendering can also need ImageMagick and explicitly available fonts; inspect the selected operation before assuming FFmpeg alone is sufficient.

From the repository root, after assigning `$mediaPython` to the chosen interpreter and creating the output directory:

```powershell
& $mediaPython .agents/skills/media-toolkit/scripts/sfx_generator.py --beeps '800,1000,800' --duration 100 --gap 100 --fade-in 5 --fade-out 5 --output build/media-task/terminal.wav
& $mediaPython .agents/skills/media-toolkit/scripts/audio_converter.py --input build/media-task/terminal.wav --output build/media-task/terminal-pcm.wav --format wav --sample-rate 44100 --channels 1
& $mediaPython .agents/skills/media-toolkit/scripts/video_clipper.py --input build/media-task/recording.mp4 --output build/media-task/excerpt.mp4 --start 12 --end 18
& $mediaPython .agents/skills/media-toolkit/scripts/video_thumbnail_extractor.py --input build/media-task/recording.mp4 --output build/media-task/frame.png --time 12
```

The rates, channels and signal frequencies are examples, not project-wide defaults. Inspect the written file with `ffprobe` or a WAV reader; listen for clicks, clipped peaks, unintended silence and changes in relative level. Verify the actual output codec, duration, channels and rate rather than inferring them from a suffix. For video, inspect the selected frames, dimensions and audio presence. `video_clipper.py` re-encodes through MoviePy; when the task only needs stream-copy trimming and keyframe boundaries are acceptable, use the installed FFmpeg directly instead of adding another lossy generation.

For `audio_trimmer.py`, write fractional timestamps with exactly three millisecond digits, such as `00:00.100` for 100 ms. Its upstream parser treats the digits after the dot as an integer millisecond field (`00:00.1` means 1 ms). Video helpers instead parse decimal seconds. Check the resulting duration when crossing between these tools.
