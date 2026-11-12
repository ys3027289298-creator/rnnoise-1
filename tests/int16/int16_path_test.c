#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "rnnoise.h"

static short sat16(float x) {
  if (x >= (float) SHRT_MAX) return SHRT_MAX;
  if (x <= (float) SHRT_MIN) return SHRT_MIN;
  return (short)(x >= 0 ? x + 0.5f : x - 0.5f);
}

int main(int argc, char **argv) {
  if (argc != 2) { fprintf(stderr, "usage: %s <pcm>\n", argv[0]); return 1; }
  FILE *f = fopen(argv[1], "rb");
  DenoiseState *sf = rnnoise_create(NULL), *si = rnnoise_create(NULL);
  int n = rnnoise_get_frame_size(), frames = 0, overflow = 0;
  long maxerr = 0;
  double ef = 0, ei = 0, maxfrac = 0;
  short in[480], outi[480];
  float inf[480], outf[480];
  while (fread(in, sizeof(short), n, f) == (size_t) n) {
    for (int i = 0; i < n; i++) inf[i] = (float) in[i];
    rnnoise_process_frame(sf, outf, inf);
    rnnoise_process_frame_int16(si, outi, in);
    for (int i = 0; i < n; i++) {
      float scaled = outf[i];
      if (scaled > SHRT_MAX || scaled < SHRT_MIN) overflow++;
      short ref = sat16(scaled);
      long e = labs((long) outi[i] - (long) ref);
      if (e > maxerr) maxerr = e;
      double d = (double) outi[i] - scaled;
      if (d < 0) d = -d;
      if (d > maxfrac) maxfrac = d;
      ef += (double) outf[i] * outf[i];
      ei += (double) outi[i] * outi[i];
    }
    frames++;
  }
  long total = (long) frames * n;
  printf("  samples            : %ld (%d frames)\n", total, frames);
  printf("  float-path energy  : %.2f (mean square)\n", ef / total);
  printf("  int16-path energy  : %.2f (mean square)\n", ei / total);
  printf("  max error vs float : %ld LSB (raw diff %.4f LSB)\n", maxerr, maxfrac);
  printf("  overflowed samples : %d\n", overflow);
  rnnoise_destroy(sf);
  rnnoise_destroy(si);
  fclose(f);
  return maxerr > 1 ? 2 : 0;
}
