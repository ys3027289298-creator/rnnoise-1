#include <stdio.h>
#include <math.h>
#define N (480 * 40)
int main(void) {
  FILE *f = fopen("output/fullscale_sine.pcm", "wb");
  for (int i = 0; i < N; i++) {
    short s = (short) (32767.0 * sin(2 * M_PI * 440.0 * i / 48000.0));
    fwrite(&s, sizeof(short), 1, f);
  }
  fclose(f);
  return 0;
}
