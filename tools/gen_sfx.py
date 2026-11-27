#!/usr/bin/env python3
"""生成几个极简音效（纯合成，无外部素材、无版权问题）。

用法：python tools/gen_sfx.py
输出：assets/sfx/click.wav、buy.wav、evolve.wav（22.05kHz 单声道 16bit）
"""
import math
import os
import struct
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "sfx")
RATE = 22050


def write_wav(path, samples):
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        frames = b"".join(struct.pack("<h", max(-32767, min(32767, int(s * 32767)))) for s in samples)
        f.writeframes(frames)


def tone(freq, dur, amp=0.35, decay=18.0, harmonics=(1.0, 0.35, 0.15)):
    n = int(RATE * dur)
    out = []
    for i in range(n):
        t = i / RATE
        env = math.exp(-decay * t)
        s = 0.0
        for k, h in enumerate(harmonics, start=1):
            s += h * math.sin(2 * math.pi * freq * k * t)
        out.append(s * amp * env / sum(harmonics))
    return out


def mix(a, b, offset):
    n = max(len(a), offset + len(b))
    out = [0.0] * n
    for i, v in enumerate(a):
        out[i] += v
    for i, v in enumerate(b):
        out[offset + i] += v
    return out


def main():
    os.makedirs(OUT, exist_ok=True)

    write_wav(os.path.join(OUT, "click.wav"), tone(760.0, 0.07, amp=0.22, decay=40.0))

    write_wav(os.path.join(OUT, "buy.wav"),
              mix(tone(440.0, 0.12, amp=0.26, decay=22.0),
                  tone(660.0, 0.14, amp=0.22, decay=18.0), int(RATE * 0.07)))

    evolve = []
    for i, f in enumerate((392.0, 523.25, 659.25, 784.0)):
        evolve = mix(evolve, tone(f, 0.22, amp=0.20, decay=12.0), int(RATE * 0.09 * i))
    write_wav(os.path.join(OUT, "evolve.wav"), evolve)

    for name in ("click.wav", "buy.wav", "evolve.wav"):
        p = os.path.join(OUT, name)
        print("%-12s %6d bytes" % (name, os.path.getsize(p)))


if __name__ == "__main__":
    main()
