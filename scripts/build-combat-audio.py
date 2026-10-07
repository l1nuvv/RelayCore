"""Build original short combat cues: mono PCM16, 44100 Hz, no runtime synthesis."""
from pathlib import Path
import math
import random
import struct
import wave

RATE = 44100
ROOT = Path(__file__).resolve().parents[1] / "assets/audio/combat"


def write(name, duration, sound):
    rng = random.Random(3003)
    samples = [sound(i / RATE, rng.uniform(-1, 1)) for i in range(round(duration * RATE))]
    peak = max(abs(v) for v in samples)
    gain = min(1, 0.72 / peak)
    pcm = bytearray()
    for i, value in enumerate(samples):
        fade = min(1, i / 30, (len(samples) - 1 - i) / 220)
        pcm.extend(struct.pack("<h", round(value * gain * fade * 32767)))
    with wave.open(str(ROOT / name), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(pcm)


def tone(t, frequency):
    return math.sin(2 * math.pi * frequency * t)


def pistol_report():
    low = 0.0
    direct = []

    def sample(t, noise):
        nonlocal low
        low += 0.22 * (noise - low)
        crack = (noise - low) * math.exp(-t * 170)
        body = low * 2.1 * math.exp(-t * 55)
        direct.append(crack + body)
        value = direct[-1]
        # Short, quiet room reflections; no oscillator or pitched tail.
        for delay, gain in ((0.011, 0.17), (0.023, 0.10), (0.041, 0.05)):
            index = len(direct) - 1 - round(delay * RATE)
            if index >= 0:
                value += direct[index] * gain
        if t >= 0.038:
            value += (noise - low) * 0.10 * math.exp(-(t - 0.038) * 180)
        return value

    return sample


def shotgun_report():
    low = 0.0
    direct = []

    def sample(t, noise):
        nonlocal low
        low += 0.09 * (noise - low)
        value = (noise - low) * math.exp(-t * 110) + 3.6 * low * math.exp(-t * 30)
        direct.append(value)
        for delay, gain in ((0.016, 0.20), (0.035, 0.12), (0.060, 0.06)):
            index = len(direct) - 1 - round(delay * RATE)
            if index >= 0:
                value += direct[index] * gain
        return value

    return sample


def build_impacts():
    write("impact_hard.wav", .11, lambda t, n: .55*n*math.exp(-95*t) + .16*tone(t, 180)*math.exp(-55*t))
    write("impact_metal.wav", .19, lambda t, n: .24*n*math.exp(-110*t) + (.25*tone(t, 3100) + .13*tone(t, 4730))*math.exp(-28*t))


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--impacts-only", action="store_true")
    args = parser.parse_args()
    ROOT.mkdir(parents=True, exist_ok=True)
    build_impacts()
    if args.impacts_only:
        print("Generated 2 impact cues in", ROOT)
        raise SystemExit(0)
    # Dry report, metal impact, rising motor, heavy discharge, servo strike,
    # falling motor with debris, and a low body impact have distinct envelopes.
    write("weapon_fire.wav", .16, pistol_report())
    write("shotgun_fire.wav", .30, shotgun_report())
    write("weapon_hit.wav", .12, lambda t, n: (.25*n + .22*tone(t, 1750) + .12*tone(t, 2630)) * math.exp(-48*t))
    write("threat_windup.wav", .30, lambda t, n: .24 * math.sin(2*math.pi*(350*t + 1100*t*t)) * min(1, t*35) + .05*n)
    write("turret_fire.wav", .23, lambda t, n: (.52*n + .32*tone(t, 75)) * math.exp(-23*t))
    write("rover_attack.wav", .20, lambda t, n: (.26*n + .22*tone(t, 420)) * math.exp(-22*t))
    write("destroyed.wav", .55, lambda t, n: .34*n*math.exp(-9*t) + .20*math.sin(2*math.pi*(480*t-330*t*t))*math.exp(-5*t))
    write("player_hurt.wav", .20, lambda t, n: (.35*tone(t, 62) + .13*n) * math.exp(-22*t))
    print("Generated 10 combat cues in", ROOT)
