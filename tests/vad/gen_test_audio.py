#!/usr/bin/env python3
"""Generate 2 s s16le 48 kHz clips: silence, white noise, clean speech."""
import os, random, subprocess
D = os.path.dirname(os.path.abspath(__file__))
SR, N = 48000, 96000
URL = "https://www.voiptroubleshooter.com/open_speech/american/OSR_us_000_0010_8k.wav"

def write_raw(path, samples):
    with open(path, "wb") as f:
        for v in samples:
            f.write(int(max(-1.0, min(1.0, v)) * 32767).to_bytes(2, "little", signed=True))

random.seed(7)
write_raw(f"{D}/silence.raw", [0.0]*N)
write_raw(f"{D}/noise.raw", [random.gauss(0, .05) for _ in range(N)])
subprocess.run(["curl", "-sL", "-A", "curl/8.0", "-o", f"{D}/_sp.wav", URL], check=True)
subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", f"{D}/_sp.wav",
                "-ac", "1", "-ar", str(SR), "-t", "2", "-f", "s16le", f"{D}/speech.raw"], check=True)
os.remove(f"{D}/_sp.wav")
print("generated silence.raw noise.raw speech.raw")
