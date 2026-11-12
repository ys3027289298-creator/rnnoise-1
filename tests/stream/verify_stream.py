#!/usr/bin/env python3
"""Streaming adapter vs direct 480-sample frame processing."""
import ctypes, math, struct, sys
FRAME, BLOCKS, F32 = 480, (480, 128, 300, 2000), ctypes.POINTER(ctypes.c_float)
def rms(x):
    return math.sqrt(sum(v * v for v in x) / len(x)) if x else 0.0
def reference(lib, s):
    st, out = lib.rnnoise_create(None), []
    for i in range(0, len(s) - FRAME + 1, FRAME):
        src = (ctypes.c_float * FRAME)(*s[i:i+FRAME]); dst = (ctypes.c_float * FRAME)()
        lib.rnnoise_process_frame(st, dst, src)
        out.extend(dst)
    lib.rnnoise_destroy(st)
    return out
def streaming(lib, s, block):
    st = lib.rnnoise_stream_create(None)
    out, n_in = [], 0
    for i in range(0, len(s), block):
        chunk = s[i:i+block]
        n_in += len(chunk)
        src = (ctypes.c_float * len(chunk))(*chunk)
        dst = (ctypes.c_float * (len(chunk) + FRAME))()
        got = lib.rnnoise_stream_process(st, dst, src, len(chunk))
        out.extend(dst[j] for j in range(got))
    tail = (ctypes.c_float * FRAME)()
    flushed = lib.rnnoise_stream_flush(st, tail); out.extend(tail[j] for j in range(flushed))
    lib.rnnoise_stream_destroy(st)
    return n_in, out, flushed
def main(lib_path, audio_path):
    lib = ctypes.CDLL(lib_path)
    cfp = ctypes.c_void_p, F32, F32
    for n, a, r in (("rnnoise_create", (ctypes.c_void_p,), ctypes.c_void_p),
        ("rnnoise_destroy", (ctypes.c_void_p,), None),
        ("rnnoise_process_frame", cfp, ctypes.c_float),
        ("rnnoise_stream_create", (ctypes.c_void_p,), ctypes.c_void_p),
        ("rnnoise_stream_destroy", (ctypes.c_void_p,), None),
        ("rnnoise_stream_process", (ctypes.c_void_p, F32, F32, ctypes.c_int),
         ctypes.c_int),
        ("rnnoise_stream_flush", (ctypes.c_void_p, F32), ctypes.c_int)):
        f = getattr(lib, n); f.argtypes, f.restype = a, r
    raw = open(audio_path, "rb").read()
    samples = list(struct.unpack("<%df" % (len(raw) // 4), raw))
    ref = reference(lib, samples)
    print("block=ref   in=%-6d out=%-6d rms=%.8f" % (len(samples), len(ref), rms(ref)))
    ok = True
    for block in BLOCKS:
        n_in, out, flushed = streaming(lib, samples, block)
        diff = max((abs(a-b) for a, b in zip(out, ref)), default=0.0)
        ok &= len(out) == len(ref) and diff == 0.0 and flushed == 0
        print("block=%-5d in=%-6d out=%-6d rms=%.8f maxdiff=%.3g flush=%d" %
              (block, n_in, len(out), rms(out), diff, flushed))
    trimmed = samples[:-(FRAME + 137)]
    _, out, flushed = streaming(lib, trimmed, 300)
    ok &= flushed == len(trimmed) % FRAME and len(out) == len(trimmed)
    print("flush check: trimmed=%d out=%d flushed=%d" %
          (len(trimmed), len(out), flushed))
    print("RESULT:", "PASS sample-exact for all block sizes" if ok else "FAIL")
    sys.exit(0 if ok else 1)
if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
