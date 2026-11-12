#!/usr/bin/env python3
"""Generate 440 Hz sine, white noise and silence clips as raw int16 PCM."""
import math
import os
import random
import struct
import sys

DUR = 0.2
RATES = (48000, 44100, 32000)
KINDS = ("sine", "noise", "silence")


def samples(kind, rate):
    n = int(DUR * rate)
    random.seed(20261112)
    for i in range(n):
        if kind == "sine":
            v = 0.5 * math.sin(2 * math.pi * 440.0 * i / rate)
        elif kind == "noise":
            v = 0.08 * random.uniform(-1.0, 1.0)
        else:
            v = 0.0
        yield max(-32768, min(32767, int(v * 32767)))


def main(outdir):
    os.makedirs(outdir, exist_ok=True)
    for rate in RATES:
        for kind in KINDS:
            path = os.path.join(outdir, f"{kind}_{rate}.pcm")
            with open(path, "wb") as f:
                f.write(struct.pack(f"<{int(DUR * rate)}h", *samples(kind, rate)))
            print(path)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "output")
