#!/usr/bin/env bash
# End-to-end check: resampling entry, anti-aliasing and 48 kHz bit-exactness.
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT=output
SRC="src/denoise.c src/rnn.c src/pitch.c src/kiss_fft.c src/celt_lpc.c src/nnet.c src/nnet_default.c src/parse_lpcnet_weights.c src/rnnoise_data.c src/rnnoise_tables.c"
CF="-O2 -Iinclude -Isrc -I."

stage() { echo; echo "===== $1 ====="; sleep 3; }
pause2() { sleep 2; }

stage "Stage 1/4: build library and test programs"
make -j4 >/dev/null 2>&1 || true
gcc $CF tests/samplerate/test_denoise.c $SRC -lm -o "$OUT/test_denoise"
gcc $CF tests/samplerate/ref48.c $SRC -lm -o "$OUT/ref48"
echo "build OK"

stage "Stage 2/4: generate test audio (sine / noise / silence)"
python3 tests/samplerate/gen_audio.py "$OUT"
ls -l "$OUT"/*.pcm
pause2

stage "Stage 3/4: 48 kHz path bit-exactness (new API vs native API)"
"$OUT/ref48" "$OUT/sine_48000.pcm" "$OUT/ref_sine48.pcm"
"$OUT/ref48" "$OUT/noise_48000.pcm" "$OUT/ref_noise48.pcm"
cat > "$OUT/sr48.c" <<'EOF'
#include <stdio.h>
#include "rnnoise.h"
int main(int c,char**v){FILE*a=fopen(v[1],"rb"),*b=fopen(v[2],"wb");
DenoiseState*s=rnnoise_sr_create(0,48000);float x[480];short t[480];int i;
while(fread(t,2,480,a)==480){for(i=0;i<480;i++)x[i]=t[i];
rnnoise_sr_process_frame(s,x,x);for(i=0;i<480;i++)t[i]=x[i];fwrite(t,2,480,b);}
rnnoise_sr_destroy(s);return 0;}
EOF
gcc $CF "$OUT/sr48.c" $SRC -lm -o "$OUT/sr48"
"$OUT/sr48" "$OUT/sine_48000.pcm" "$OUT/sr_sine48.pcm"
"$OUT/sr48" "$OUT/noise_48000.pcm" "$OUT/sr_noise48.pcm"
cmp "$OUT/ref_sine48.pcm" "$OUT/sr_sine48.pcm" && echo "sine48: identical"
cmp "$OUT/ref_noise48.pcm" "$OUT/sr_noise48.pcm" && echo "noise48: identical"
pause2

stage "Stage 4/4: denoise at 48000 / 44100 / 32000 Hz"
"$OUT/test_denoise" "$OUT" | tee "$OUT/results.txt"
echo; echo "all done, artifacts in $OUT/"
