#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "rnnoise.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int read_pcm(const char *p, short **b) {
    FILE *f = fopen(p, "rb"); fseek(f, 0, SEEK_END); long n = ftell(f) / 2; rewind(f);
    *b = malloc(n * 2);
    if (fread(*b, 2, n, f) != (size_t) n) { fclose(f); return 0; }
    fclose(f);
    return (int) n;
}

static double peak_hz(const float *x, int n, int rate) {
    double best = -1; int bf = 0;
    for (int f = 50; f < rate / 2; f += 2) {
        double w = 2 * cos(2 * M_PI * f / rate), s1 = 0, s2 = 0;
        for (int i = 0; i < n; i++) { double s0 = x[i] + w * s1 - s2; s2 = s1; s1 = s0; }
        double e = s1 * s1 + s2 * s2 - w * s1 * s2;
        if (e > best) { best = e; bf = f; }
    }
    return bf;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "output";
    const int rates[3] = {48000, 44100, 32000};
    const char *kinds[3] = {"sine", "noise", "silence"};
    double ref_peak = 0;
    for (int k = 0; k < 3; k++) for (int r = 0; r < 3; r++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/%s_%d.pcm", dir, kinds[k], rates[r]);
        short *raw;
        int n = read_pcm(path, &raw), frame = rnnoise_sr_get_frame_size(rates[r]);
        DenoiseState *st = rnnoise_sr_create(NULL, rates[r]);
        float *buf = malloc(n * sizeof(float));
        int in_cnt = 0, out_cnt = 0; double sum2 = 0;
        for (int i = 0; i + frame <= n; i += frame) {
            for (int j = 0; j < frame; j++) buf[i + j] = raw[i + j];
            rnnoise_sr_process_frame(st, buf + i, buf + i);
            in_cnt += frame;
            for (int j = 0; j < frame; j++) sum2 += (double)buf[i+j] * buf[i+j];
            out_cnt += frame;
        }
        double rms = sqrt(sum2 / out_cnt), pk = peak_hz(buf, out_cnt, rates[r]);
        if (k == 0 && r == 0) ref_peak = pk;
        printf("%-7s rate=%d in=%d out=%d rms=%.4f peak_dev_hz=%.1f\n", kinds[k],
               rates[r], in_cnt, out_cnt, rms, k == 0 ? fabs(pk - ref_peak) : 0.0);
        free(buf);
        free(raw);
        rnnoise_sr_destroy(st);
    }
    return 0;
}
