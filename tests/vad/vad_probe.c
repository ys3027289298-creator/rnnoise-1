#include <stdio.h>
#include <dlfcn.h>
#include "rnnoise.h"

/* Reads 48 kHz mono s16le PCM; prints model VAD posterior per frame.
   lib path is argv[2] so the same binary works on baseline/patched builds. */
int main(int argc, char **argv) {
  void *h = dlopen(argv[2], RTLD_NOW);
  float (*get_vad)(DenoiseState *) = (float (*)(DenoiseState *)) dlsym(h, "rnnoise_get_vad_prob");
  FILE *f = fopen(argv[1], "rb");
  DenoiseState *st = rnnoise_create(NULL);
  short tmp[480];
  float x[480];
  while (fread(tmp, sizeof(short), 480, f) == 480) {
    for (int i = 0; i < 480; i++) x[i] = tmp[i];
    rnnoise_process_frame(st, x, x);
    printf("%.6f\n", get_vad ? get_vad(st) : -1.0f);
  }
  rnnoise_destroy(st);
  dlclose(h);
  fclose(f);
  return 0;
}
