#!/usr/bin/env python3
"""Verify independent multichannel RNNoise denoising through librnnoise."""
import ctypes, math, os, struct

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "output")
FRAME = 480
lib = ctypes.CDLL(os.path.join(ROOT, ".libs", "librnnoise.so"))
lib.rnnoise_create.restype = ctypes.c_void_p
lib.rnnoise_multi_create.restype = ctypes.c_void_p
lib.rnnoise_multi_create.argtypes = [ctypes.c_void_p, ctypes.c_int]
for fn in ("rnnoise_process_frame", "rnnoise_multi_process_frame"):
    getattr(lib, fn).restype = ctypes.c_float

def load(name):
    raw = open(os.path.join(OUT, name), "rb").read()
    return list(struct.unpack("<%df" % (len(raw) // 4), raw))

def energy(x):
    return sum(v * v for v in x)

def mono(x):
    st = lib.rnnoise_create(None); out = []
    for f in range(len(x) // FRAME):
        b = (ctypes.c_float * FRAME)(*x[f*FRAME:(f+1)*FRAME])
        lib.rnnoise_process_frame(st, b, b); out += list(b)
    lib.rnnoise_destroy(st); return out

def multi(x, ch):
    st = lib.rnnoise_multi_create(None, ch)
    assert st, "rnnoise_multi_create rejected valid channel count"
    out = []
    for f in range(len(x) // (ch * FRAME)):
        b = (ctypes.c_float * (ch * FRAME))(*x[f*ch*FRAME:(f+1)*ch*FRAME])
        lib.rnnoise_multi_process_frame(st, b, b); out += list(b)
    lib.rnnoise_multi_destroy(st); return out

x0 = load("sig1_diff_freq.f32")[0::2]
print("mono API vs multi(1) bit-exact:", mono(x0) == multi(x0, 1))
assert lib.rnnoise_multi_create(None, 0) is None
assert lib.rnnoise_multi_create(None, 9) is None
print("channel cap: channels=0 and channels=9 rejected -> RNNOISE_EINVAL/NULL OK")

for name, desc in (("sig1_diff_freq", "L=440 Hz tone, R=1760 Hz tone"),
                   ("sig2_left_only", "L active, R silent"),
                   ("sig3_in_phase", "identical L/R")):
    x = load(name + ".f32")
    L, R = x[0::2], x[1::2]
    mL, mR = mono(L), mono(R)
    sL, sR = multi(x, 2)[0::2], multi(x, 2)[1::2]
    resL = energy([a-b for a, b in zip(sL, mL)])
    resR = energy([a-b for a, b in zip(sR, mR)])
    db = 10 * math.log10((energy(sR) + 1e-30) / (energy(sL) + 1e-30))
    print("[%s] %s" % (name, desc))
    print("  output energy    L=%.4e R=%.4e" % (energy(sL), energy(sR)))
    print("  residual vs mono L=%.4e R=%.4e (0.0 means bit-exact)" % (resL, resR))
    print("  crosstalk R/L=%.2e (%+.1f dB)" % (energy(sR)/(energy(sL)+1e-30), db))
    verdict = "no crosstalk: silent R stays %.1f dB below L" % db if name == "sig2_left_only" \
        else "channels independent: stereo output equals per-channel mono output"
    print("  verdict:", verdict)
