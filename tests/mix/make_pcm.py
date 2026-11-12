#!/usr/bin/env python3
# 生成 48k/16bit 单声道 PCM：纯噪声、语音+噪声、静音（各 1 秒）。
import math, os, random, struct
FS = 48000
OUT = os.path.join(os.path.dirname(__file__), "..", "..", "output")

def w(name, samples):
    p = os.path.join(OUT, name)
    with open(p, "wb") as f:
        f.write(b"".join(struct.pack("<h", max(-32768, min(32767, int(s)))) for s in samples))

rng = random.Random(20261112)
n = [rng.gauss(0, 6000) for _ in range(FS)]
w("noise.pcm", n)
v = [(9000 * (0.55 * math.sin(2 * math.pi * 220 * t / FS)
              + 0.3 * math.sin(2 * math.pi * 330 * t / FS))
      * (0.5 + 0.5 * math.sin(2 * math.pi * 3 * t / FS))) for t in range(FS)]
w("speech_noise.pcm", [x + rng.gauss(0, 4000) for x in v])
w("silence.pcm", [0.0] * FS)
