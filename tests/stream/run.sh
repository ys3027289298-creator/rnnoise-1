#!/bin/sh
# Build the streaming adapter and run the block-size equivalence check.
set -e

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT="$ROOT/tests/stream/output"
BUILD="$OUT/build"
MODEL_DIR=/tmp/rnnoise/src

stage() {
  echo ""
  echo "==================== $1 ===================="
}

pause() {
  sleep 4
}

mkdir -p "$BUILD"

stage "Stage 1/6: warm up and check toolchain"
gcc --version | head -1
python3 --version
pause

stage "Stage 2/6: prepare model data"
cp "$MODEL_DIR/rnnoise_data.c" "$MODEL_DIR/rnnoise_data.h" "$BUILD/"
echo "model data copied into $BUILD"
pause

stage "Stage 3/6: compile library objects"
SRCS="denoise rnn pitch kiss_fft celt_lpc nnet nnet_default parse_lpcnet_weights rnnoise_tables rnnoise_data stream"
OBJS=""
for name in $SRCS; do
  src="$ROOT/src/$name.c"
  [ -f "$src" ] || src="$BUILD/$name.c"
  echo "  CC $name.c"
  gcc -O2 -fPIC -Wall -Wextra -I"$ROOT/include" -I"$ROOT/src" -I"$BUILD" \
      -c "$src" -o "$BUILD/$name.o"
  OBJS="$OBJS $BUILD/$name.o"
done
pause

stage "Stage 4/6: link librnnoise with streaming adapter"
gcc -shared -o "$BUILD/librnnoise.so" $OBJS -lm
echo "built $BUILD/librnnoise.so"
pause

stage "Stage 5/6: generate 3 s 48 kHz test audio"
python3 "$ROOT/tests/stream/gen_audio.py" "$OUT/test_audio.f32"
echo "wrote $OUT/test_audio.f32 ($(stat -c %s "$OUT/test_audio.f32") bytes)"
pause

stage "Stage 6/6: verify streaming output vs direct 480-frame processing"
python3 "$ROOT/tests/stream/verify_stream.py" \
    "$BUILD/librnnoise.so" "$OUT/test_audio.f32" | tee "$OUT/verify.log"
pause
echo ""
echo "All outputs are in $OUT"
