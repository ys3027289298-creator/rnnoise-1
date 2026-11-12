#include <stdio.h>
#include <limits.h>
#include "rnnoise.h"
static short sat16(float x) {
  if (x >= (float) SHRT_MAX) return SHRT_MAX;
  if (x <= (float) SHRT_MIN) return SHRT_MIN;
  return (short)(x >= 0 ? x + 0.5f : x - 0.5f);
}
int main(void) {
  DenoiseState *st = rnnoise_create(NULL);
  DenoiseState *sf = rnnoise_create(NULL);
  int n = rnnoise_get_frame_size(), bad = 0, clipped = 0;
  short in[480], out[480];
  float fin[480], fout[480];
  for (int i = 0; i < n; i++) in[i] = SHRT_MAX;
  for (int frame = 0; frame < 20; frame++) {
    for (int i = 0; i < n; i++) fin[i] = (float) in[i];
    rnnoise_process_frame(sf, fout, fin);
    rnnoise_process_frame_int16(st, out, in);
    for (int i = 0; i < n; i++)
      if (out[i] != sat16(fout[i])) bad++;
    for (int i = 0; i < n; i++)
      if (fout[i] > SHRT_MAX || fout[i] < SHRT_MIN) clipped++;
  }
  printf("saturation check: %s (wrap violations=%d, clipped-by-float=%d)\n",
         bad ? "FAIL" : "PASS", bad, clipped);
  rnnoise_destroy(st);
  rnnoise_destroy(sf);
  return bad ? 1 : 0;
}
