#include <stdio.h>
#define N (480 * 40)
static unsigned rng = 12345u;
int main(void) {
  FILE *f = fopen("output/small_noise.pcm", "wb");
  for (int i = 0; i < N; i++) {
    rng = rng * 1103515245u + 12345u;
    short s = (short) ((int) (rng >> 24) - 128); /* +/-127 */
    fwrite(&s, sizeof(short), 1, f);
  }
  fclose(f);
  return 0;
}
