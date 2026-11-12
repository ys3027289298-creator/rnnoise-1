#!/usr/bin/env python3
"""Generate 3 s of 48 kHz noisy test audio as raw float32 samples."""
import math
import struct
import sys

RATE = 48000
SECONDS = 3
N = RATE * SECONDS


def main(path):
    rng_state = 123456789
    with open(path, "wb") as f:
        for i in range(N):
            rng_state = (rng_state * 1103515245 + 12345) & 0x7FFFFFFF
            noise = rng_state / 0x7FFFFFFF * 2.0 - 1.0
            t = i / RATE
            tone = 0.6 * math.sin(2 * math.pi * 440 * t)
            tone += 0.3 * math.sin(2 * math.pi * 1000 * t)
            sample = max(-1.0, min(1.0, 0.5 * tone + 0.1 * noise))
            f.write(struct.pack("<f", sample))


if __name__ == "__main__":
    main(sys.argv[1])
