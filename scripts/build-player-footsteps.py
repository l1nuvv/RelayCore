"""Build footsteps and two-foot landing contacts from the supplied surface recordings."""

from pathlib import Path
import struct
import wave


RATE = 44100
ROOT = Path(__file__).resolve().parents[1] / "assets" / "audio" / "player"
SOURCES = {
    "concrete": ("concrete_footsteps_short_3.5s.wav", (0.155, 1.025, 1.865, 2.720)),
    "metal": ("metal_footsteps_short_3.6s.wav", (0.125, 1.335, 2.500, 3.110)),
    "tile": ("tile_footsteps_short_3.5s.wav", (0.200, 0.765, 1.430, 2.080)),
}


def read_mono_pcm16(path):
    with wave.open(str(path), "rb") as source:
        description = (source.getnchannels(), source.getsampwidth(), source.getframerate())
        if description != (1, 2, RATE):
            raise RuntimeError(f"{path.name}: expected mono PCM16/{RATE} Hz, got {description}")
        return list(struct.unpack(f"<{source.getnframes()}h", source.readframes(source.getnframes())))


def write_clip(path, samples):
    fade_in = int(RATE * 0.008)
    fade_out = int(RATE * 0.055)
    for index in range(min(fade_in, len(samples))):
        samples[index] = round(samples[index] * index / fade_in)
    for index in range(min(fade_out, len(samples))):
        position = len(samples) - 1 - index
        samples[position] = round(samples[position] * index / fade_out)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(struct.pack(f"<{len(samples)}h", *samples))


def slice_step(samples, peak_seconds):
    start = max(0, round((peak_seconds - 0.065) * RATE))
    end = min(len(samples), round((peak_seconds + 0.285) * RATE))
    return samples[start:end]


if __name__ == "__main__":
    for surface, (source_name, peaks) in SOURCES.items():
        recording = read_mono_pcm16(ROOT / source_name)
        for variant, peak in enumerate(peaks, start=1):
            write_clip(ROOT / f"step_{surface}_{variant}.wav", slice_step(recording, peak))
        first = slice_step(recording, peaks[0])
        second = slice_step(recording, peaks[1])
        offset = round(0.032 * RATE)
        landing = [0.0] * (max(len(first), len(second)) + offset)
        for index, sample in enumerate(first):
            landing[index] += sample
        for index, sample in enumerate(second):
            landing[index + offset] += sample * 0.75
        gain = min(1.0, 28000 / max(1, max(abs(value) for value in landing)))
        write_clip(ROOT / f"land_{surface}.wav", [round(value * gain) for value in landing])

    print("Built 12 footsteps and 3 landings from the supplied concrete, metal and tile recordings")
