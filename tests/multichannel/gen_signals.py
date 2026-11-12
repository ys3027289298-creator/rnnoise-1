#!/usr/bin/env python3
"""Generate three 0.5 s stereo (48 kHz, float32 interleaved) test signals."""
import math, os, struct, random

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "output")
SR, SEC, N = 48000, 0.5, 24000
random.seed(20261112)

def tone(freq, n):
    return [0.2 * math.sin(2 * math.pi * freq * i / SR) for i in range(n)]

def noise(n, scale=0.05):
    return [random.gauss(0, scale) for _ in range(n)]

def save(name, left, right):
    data = struct.pack("<%df" % (2 * N), *[v for pair in zip(left, right) for v in pair])
    with open(os.path.join(OUT, name), "wb") as f:
        f.write(data)

os.makedirs(OUT, exist_ok=True)
nl, nr = noise(N), noise(N)
save("sig1_diff_freq.f32", [a + b for a, b in zip(tone(440, N), nl)],
     [a + b for a, b in zip(tone(1760, N), nr)])
save("sig2_left_only.f32", [a + b for a, b in zip(tone(440, N), nl)], [0.0] * N)
sig = [a + b for a, b in zip(tone(880, N), noise(N))]
save("sig3_in_phase.f32", sig, sig[:])
print("wrote 3 signals, each %d interleaved floats, to %s" % (2 * N, os.path.abspath(OUT)))
