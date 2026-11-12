#!/bin/bash
# VAD probability validation: builds baseline/patched libs, runs 3 clips, reports stats.
set -e
cd "$(dirname "$0")/../.."
OUT=output; T=tests/vad
mkdir -p "$OUT"
echo "=== Stage 1/4: build baseline (unpatched) and patched rnnoise ==="
make -j4 >/dev/null 2>&1
make_lib() { # $1=src dir $2=out dir
  make -C "$1" -j4 >/dev/null 2>&1
  gcc -O2 -Iinclude "$T/vad_probe.c" "$2/librnnoise.a" -ldl -lm -o "$T/vad_probe_$3"
  gcc -O2 -Iinclude examples/rnnoise_demo.c "$2/librnnoise.a" -lm -o "$T/demo_$3"
}
if [ ! -x "$T/demo_baseline" ]; then
  rm -rf /tmp/rnn_baseline
  git worktree prune
  git worktree add --detach /tmp/rnn_baseline HEAD >/dev/null 2>&1
  (cd /tmp/rnn_baseline && ./autogen.sh >/dev/null 2>&1 && ./configure --disable-x86-rtcd >/dev/null 2>&1)
  make_lib /tmp/rnn_baseline /tmp/rnn_baseline/.libs baseline
fi
make_lib "$PWD" "$PWD/.libs" patched
sleep 10

echo "=== Stage 2/4: generate 2 s test clips (silence / noise / speech) ==="
python3 "$T/gen_test_audio.py"
sleep 6

echo "=== Stage 3/4: run VAD probe and check process_frame sample equivalence ==="
sleep 8
for f in silence noise speech; do
  "$T/vad_probe_baseline" "$T/$f.raw" /tmp/rnn_baseline/.libs/librnnoise.so > /dev/null
  "$T/vad_probe_patched" "$T/$f.raw" "$PWD/.libs/librnnoise.so" > "$OUT/${f}_vad.txt"
  "$T/demo_baseline" "$T/$f.raw" "$OUT/${f}.base.pcm"
  "$T/demo_patched" "$T/$f.raw" "$OUT/${f}.new.pcm"
done
if cmp -s "$OUT/speech.base.pcm" "$OUT/speech.new.pcm" && cmp -s "$OUT/noise.base.pcm" "$OUT/noise.new.pcm"; then
  echo "process_frame output: bit-identical to baseline on all clips"
else
  echo "process_frame output: DIFFERS"; exit 1
fi
sleep 5

echo "=== Stage 4/4: statistics vs hand-labelled activity regions (threshold 0.5) ==="
sleep 3
python3 - "$T" "$OUT" <<'PYEOF'
import statistics, sys
t, out = sys.argv[1], sys.argv[2]
HAND = {"silence": (), "noise": (), "speech": ((0, 9), (50, 200))}
for name, label in (("silence", "all inactive"), ("noise", "all inactive"), ("speech", "ranges [0,9)+[50,200)")):
    p = [float(x) for x in open(f"{out}/{name}_vad.txt")]
    truth = [1 if any(a <= i < b for a, b in HAND[name]) else 0 for i in range(len(p))]
    pred = [1 if v > .5 else 0 for v in p]
    fp = sum(1 for a, b in zip(truth, pred) if a == 0 and b == 1)
    fn = sum(1 for a, b in zip(truth, pred) if a == 1 and b == 0)
    active = sum(truth)
    print(f"[{name}] label={label} frames={len(p)} active={active}")
    print(f"  prob mean={statistics.mean(p):.4f} max={max(p):.4f} min={min(p):.4f} "
          f"misjudged={fp+fn} (false_positive={fp}, false_negative={fn})")
PYEOF
echo "per-frame probabilities saved under $OUT/"
