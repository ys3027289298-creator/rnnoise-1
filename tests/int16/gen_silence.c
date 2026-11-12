#include <stdio.h>
#define N (480 * 40)
int main(void) {
  FILE *f = fopen("output/silence.pcm", "wb");
  for (int i = 0; i < N; i++) {
    short s = 0;
    fwrite(&s, sizeof(short), 1, f);
  }
  fclose(f);
  return 0;
}
