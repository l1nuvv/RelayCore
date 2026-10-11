"""Original mono PCM16 loops for the ventilation motor, steam leak and damaged breaker.

Noise is filtered per source; integer-period harmonics and a seam crossfade avoid loop clicks.
These sounds describe equipment, never player surface contacts.
"""
from pathlib import Path
import math
import random
import struct
import wave

RATE = 44100
ROOT = Path(__file__).resolve().parents[1] / "assets/audio/environment"


def build(kind, seconds=6.4):
    rng = random.Random(304)
    samples = []
    low = medium = steam = 0.0
    for i in range(round(seconds * RATE)):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        low += (noise - low) * 0.025
        medium += (noise - medium) * 0.32
        if kind == "ventilation":
            motor = math.sin(2 * math.pi * 50 * t) * 0.028 + math.sin(2 * math.pi * 100 * t) * 0.009
            value = motor + low * 0.25 + medium * 0.025 * (0.85 + 0.15 * math.sin(2 * math.pi * 12.5 * t))
        elif kind == "steam":
            steam += (noise - steam) * 0.12
            value = (steam - low) * 0.16 * (0.92 + 0.08 * math.sin(2 * math.pi * 0.3125 * t))
        else:
            burst = t % 3.2
            envelope = max(0, 1 - burst / 0.38) ** 2
            value = (noise - medium) * envelope * 0.16
        samples.append(value)
    seam = round(RATE * 0.025)
    for i in range(seam):
        blend = i / (seam - 1)
        samples[i] = samples[-seam + i] * (1 - blend) + samples[i] * blend
    # Zero endpoints avoid clicks when a session starts or ends.
    for i in range(80):
        samples[i] *= i / 80
        samples[-1 - i] *= i / 80
    return [round(sample * 32767) for sample in samples]


if __name__ == "__main__":
    ROOT.mkdir(parents=True, exist_ok=True)
    for kind in ("ventilation", "steam", "electrical"):
        samples = build(kind)
        with wave.open(str(ROOT / f"{kind}.wav"), "wb") as output:
            output.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
            output.writeframes(struct.pack(f"<{len(samples)}h", *samples))
    print("Built three original positional equipment loops")
