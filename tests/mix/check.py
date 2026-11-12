#!/usr/bin/env python3
# 读 PCM，打印输出/输入能量比，以及与 ratio=0、ratio=1 结果的差异样本数。
import os, struct, sys
D = os.path.join(os.path.dirname(__file__), "..", "..", "output")


def rd(p):
    data = open(os.path.join(D, p), "rb").read()
    return list(struct.unpack("<%dh" % (len(data) // 2), data))


name, ratios = sys.argv[1], sys.argv[2:]
mid = ratios[1]
src, outs = rd(name + ".pcm")[480:], {r: rd(f"{name}_{r}.pcm") for r in ratios}
ein = sum(x * x for x in src) or 1
print(f"  {name:<14} Eout/Ein={sum(x * x for x in outs[mid]) / ein:.6f}  "
      f"diff_vs_0={sum(a != b for a, b in zip(outs[mid], outs['0'])):5d}  "
      f"diff_vs_1={sum(a != b for a, b in zip(outs[mid], outs['1'])):5d}  N={len(outs[mid])}")
