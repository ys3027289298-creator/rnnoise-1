#!/bin/sh
# Staged multichannel RNNoise acceptance run (no keyboard input required).
set -e
cd "$(dirname "$0")/../.."

echo "=== Stage 1/4: build librnnoise + bundled example ==="
sleep 6
test -x configure || autoreconf -isf
test -f Makefile || ./configure --enable-examples --disable-doc
make -j4 >/dev/null
ls .libs/librnnoise.so examples/.libs/rnnoise_demo

echo "=== Stage 2/4: generate 3 stereo test signals (output/) ==="
sleep 5
python3 tests/multichannel/gen_signals.py
sleep 3

echo "=== Stage 3/4: mono vs stereo denoise, energies and crosstalk ==="
sleep 7
python3 tests/multichannel/verify.py | tee output/verify_report.txt
sleep 3

echo "=== Stage 4/4: sanity run with the bundled example ==="
sleep 7
export LD_LIBRARY_PATH="$PWD/.libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
python3 - <<'EOF'
import struct, subprocess
raw = open("output/sig1_diff_freq.f32", "rb").read()
vals = struct.unpack("<%df" % (len(raw)//4), raw)
mono = [0.5*(vals[i]+vals[i+1]) for i in range(0, len(vals), 2)]
pcm = b"".join(struct.pack("<h", max(-32768, min(32767, int(32767*v)))) for v in mono)
open("output/demo_mono_in.pcm", "wb").write(pcm)
EOF
examples/.libs/rnnoise_demo output/demo_mono_in.pcm output/demo_mono_out.pcm
ls -l output/demo_mono_out.pcm
sleep 6
echo "=== all stages complete; artifacts in output/ ==="
