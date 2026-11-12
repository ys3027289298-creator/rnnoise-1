#include <stdio.h>
#include <stdlib.h>
#include "rnnoise.h"

/* Native 48 kHz reference path, processed frame by frame like rnnoise_demo. */
int main(int argc, char **argv) {
    if (argc < 3) return 1;
    FILE *fi = fopen(argv[1], "rb"), *fo = fopen(argv[2], "wb");
    DenoiseState *st = rnnoise_create(NULL);
    float x[480];
    short tmp[480];
    int n, i;
    while ((n = fread(tmp, 2, 480, fi)) == 480) {
        for (i = 0; i < 480; i++) x[i] = tmp[i];
        rnnoise_process_frame(st, x, x);
        for (i = 0; i < 480; i++) tmp[i] = x[i];
        fwrite(tmp, 2, 480, fo);
    }
    rnnoise_destroy(st);
    fclose(fi);
    fclose(fo);
    return 0;
}
