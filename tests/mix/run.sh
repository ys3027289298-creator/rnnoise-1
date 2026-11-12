#!/usr/bin/env bash
# 干湿混合联调验证：三段素材 x 三种比例，全部产物进 output/。
set -euo pipefail
cd "$(dirname "$0")/../.."
ROOT="$PWD"
DEMO="$ROOT/build/examples/.libs/rnnoise_demo"
BASELINE="${RNNOISE_BASELINE:-/tmp/rnnoise_demo_baseline_bin}"
export LD_LIBRARY_PATH="$ROOT/build/.libs:${LD_LIBRARY_PATH:-}"
CLIPS=(noise speech_noise silence)
RATIOS=(0 0.5 1)

stage() { sleep 6; echo; echo "===== 阶段 $1/6：$2 ====="; }

stage 1 "重新构建"
make -C build -j"$(nproc)" >/dev/null

stage 2 "生成测试素材（纯噪声 / 语音+噪声 / 静音，各 1 秒 48k/16bit）"
python3 tests/mix/make_pcm.py
ls -l output/*.pcm | sed 's/^/  /'

stage 3 "按 0 / 0.5 / 1 三种比例跑现成示例 rnnoise_demo"
for c in "${CLIPS[@]}"; do
  for r in "${RATIOS[@]}"; do
    "$DEMO" "output/$c.pcm" "output/${c}_${r}.pcm" "$r"
  done
done
ls -l output/*_*.pcm | sed 's/^/  /'

stage 4 "能量比与两端差异样本数"
for c in "${CLIPS[@]}"; do python3 tests/mix/check.py "$c" "${RATIOS[@]}"; done

stage 5 "默认行为逐样本一致性（未设比例 vs 旧二进制，差异应为 0）"
for c in "${CLIPS[@]}"; do
  "$BASELINE" "output/$c.pcm" "output/${c}_ref.pcm"
  "$DEMO" "output/$c.pcm" "output/${c}_default.pcm"
  d=$(cmp -l "output/${c}_ref.pcm" "output/${c}_default.pcm" | wc -l)
  echo "  $c: 与基线差异样本数 = $d"
done

stage 6 "越界比例错误码（-0.1 / 1.5 / nan 均须拒绝）"
for r in -0.1 1.5 nan; do
  if "$DEMO" output/silence.pcm output/bad.pcm "$r" 2>/dev/null; then
    echo "  ratio=$r: 未拒绝（异常）"; exit 1
  else echo "  ratio=$r: 已拒绝，退出码 $?"; fi
done
echo; echo "全部验证完成。"
