"""Offline synthesis of a mechanical flashlight switch.

Footsteps and landings are built from real recordings by build-player-footsteps.py.
Outputs mono PCM16 / 44100 Hz. No synthesis or sample allocation happens in-game.
"""
from pathlib import Path
import math
import random
import struct
import wave

RATE = 44100
ROOT = Path(__file__).resolve().parents[1] / 'assets' / 'audio' / 'player'


def write(name, samples):
    peak = max(abs(value) for value in samples)
    gain = min(1.0, 0.78 / max(peak, 0.001))
    pcm = bytearray()
    for i, value in enumerate(samples):
        fade = min(1.0, i / 45, (len(samples) - 1 - i) / 220)
        pcm.extend(struct.pack('<h', round(value * gain * fade * 32767)))
    with wave.open(str(ROOT / name), 'wb') as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(pcm)




if __name__ == '__main__':
    ROOT.mkdir(parents=True, exist_ok=True)
    rng = random.Random(882)
    click = []
    for i in range(int(RATE * 0.09)):
        t = i / RATE
        value = rng.uniform(-1, 1) * math.exp(-t * 260) * 0.16
        if t > 0.018:
            s = t - 0.018
            value += (rng.uniform(-1, 1) * 0.18 + math.sin(s * 2 * math.pi * 850) * 0.07) * math.exp(-s * 210)
        click.append(value)
    write('flashlight_switch.wav', click)
    print('Generated flashlight switch in', ROOT)
