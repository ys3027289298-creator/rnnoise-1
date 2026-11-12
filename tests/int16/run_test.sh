#!/bin/sh
# Build the int16/float path comparison test and run it on three clips.
# Pacing: one title per segment, a 2 s pause between segments, total 30-90 s.
set -e
cd "$(dirname "$0")/../.."
mkdir -p output

CC=${CC:-cc}
CFLAGS="-Iinclude -O2 -Wall"
LIB=.libs/librnnoise.a
if [ ! -f "$LIB" ]; then
  echo "building librnnoise first ..."
  ./autogen.sh >/dev/null 2>&1 || true
  ./configure --disable-doc --disable-x86-rtcd >/dev/null
  make -j4 >/dev/null
fi

$CC $CFLAGS tests/int16/gen_fullscale_sine.c -lm -o output/gen_fullscale_sine
$CC $CFLAGS tests/int16/gen_small_noise.c     -o output/gen_small_noise
$CC $CFLAGS tests/int16/gen_silence.c         -o output/gen_silence
$CC $CFLAGS tests/int16/int16_path_test.c "$LIB" -lm -o output/int16_path_test
$CC $CFLAGS tests/int16/saturation_check.c "$LIB" -lm -o output/saturation_check

output/gen_fullscale_sine
output/gen_small_noise
output/gen_silence
output/saturation_check

run_segment() {
  title=$1; pcm=$2
  echo ""
  echo "==================== $title ===================="
  output/int16_path_test "output/$pcm"
}

run_segment "Segment 1/3: full-scale sine" fullscale_sine.pcm
sleep 10
sleep 2
run_segment "Segment 2/3: small-amplitude noise" small_noise.pcm
sleep 10
sleep 2
run_segment "Segment 3/3: silence" silence.pcm
sleep 10
echo ""
echo "done: all artifacts under output/"
