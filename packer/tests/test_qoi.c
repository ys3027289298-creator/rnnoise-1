#define QOI_IMPLEMENTATION
#include "qoi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "packer.h"

#define IMG_WIDTH 128u
#define IMG_HEIGHT 128u
#define IMG_CHANNELS 4

typedef struct {
    const char *name;
    uint8_t *pixels;
    char png_path[256];
    char qoi_path[256];
} test_case;

static uint8_t *alloc_pixels(void)
{
    size_t bytes = (size_t)IMG_WIDTH * IMG_HEIGHT * IMG_CHANNELS;
    uint8_t *pixels = (uint8_t *)malloc(bytes);
    if (pixels == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(EXIT_FAILURE);
    }
    return pixels;
}

static void build_gradient(uint8_t *pixels)
{
    for (uint32_t y = 0; y < IMG_HEIGHT; ++y) {
        for (uint32_t x = 0; x < IMG_WIDTH; ++x) {
            uint8_t *px = pixels +
                ((size_t)y * IMG_WIDTH + x) * IMG_CHANNELS;
            px[0] = (uint8_t)(x * 2u);
            px[1] = (uint8_t)(y * 2u);
            px[2] = (uint8_t)((x + y) % 256u);
            px[3] = (uint8_t)(255u - (x + y) / 2u);
        }
    }
}

static void build_solid(uint8_t *pixels)
{
    for (size_t i = 0;
         i < (size_t)IMG_WIDTH * IMG_HEIGHT * IMG_CHANNELS;
         i += IMG_CHANNELS) {
        pixels[i + 0] = 42;
        pixels[i + 1] = 137;
        pixels[i + 2] = 201;
        pixels[i + 3] = 255;
    }
}

static void build_transparent(uint8_t *pixels)
{
    for (uint32_t y = 0; y < IMG_HEIGHT; ++y) {
        for (uint32_t x = 0; x < IMG_WIDTH; ++x) {
            uint8_t *px = pixels +
                ((size_t)y * IMG_WIDTH + x) * IMG_CHANNELS;
            /* Opaque border, fully transparent interior. */
            int border = (x < 4 || y < 4 ||
                          x >= IMG_WIDTH - 4 || y >= IMG_HEIGHT - 4);
            px[0] = 10;
            px[1] = 200;
            px[2] = 90;
            px[3] = border ? 255 : 0;
        }
    }
}

static void build_noise(uint8_t *pixels)
{
    /* Deterministic LCG so the run is reproducible without any input. */
    unsigned int seed = 0x12345678u;
    size_t bytes = (size_t)IMG_WIDTH * IMG_HEIGHT * IMG_CHANNELS;
    for (size_t i = 0; i < bytes; ++i) {
        seed = seed * 1103515245u + 12345u;
        pixels[i] = (uint8_t)((seed >> 16) & 0xffu);
    }
}

static long file_size(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    long size = ftell(fp);
    fclose(fp);
    return size;
}

static int run_case(const test_case *tc)
{
    size_t pixel_bytes =
        (size_t)IMG_WIDTH * IMG_HEIGHT * IMG_CHANNELS;

    /* Strong check: our stream must be byte-identical to the reference
     * encoder, since op selection and the index hash are deterministic. */
    size_t our_len = 0;
    uint8_t *our_bytes = packer_encode_qoi(
        tc->pixels, IMG_WIDTH, IMG_HEIGHT, IMG_CHANNELS, &our_len);
    qoi_desc ref_desc;
    ref_desc.width = IMG_WIDTH;
    ref_desc.height = IMG_HEIGHT;
    ref_desc.channels = IMG_CHANNELS;
    ref_desc.colorspace = QOI_SRGB;
    int ref_len = 0;
    void *ref_bytes =
        qoi_encode(tc->pixels, &ref_desc, &ref_len);
    int encoder_mismatch = 1;
    if (our_bytes != NULL && ref_bytes != NULL &&
        our_len == (size_t)ref_len &&
        memcmp(our_bytes, ref_bytes, our_len) == 0) {
        encoder_mismatch = 0;
    }
    free(our_bytes);
    free(ref_bytes);
    if (encoder_mismatch) {
        fprintf(stderr, "[%s] output differs from reference encoder\n",
                tc->name);
        return 1;
    }

    packer_status png_status = packer_write_png(
        tc->png_path, tc->pixels, IMG_WIDTH, IMG_HEIGHT, IMG_CHANNELS);
    packer_status qoi_status = packer_write_qoi(
        tc->qoi_path, tc->pixels, IMG_WIDTH, IMG_HEIGHT, IMG_CHANNELS);
    if (png_status != PACKER_OK || qoi_status != PACKER_OK) {
        fprintf(stderr, "[%s] encode failed: png=%d qoi=%d\n",
                tc->name, (int)png_status, (int)qoi_status);
        return 1;
    }

    long png_bytes = file_size(tc->png_path);
    long qoi_bytes = file_size(tc->qoi_path);

    FILE *fp = fopen(tc->qoi_path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "[%s] cannot open %s\n",
                tc->name, tc->qoi_path);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long raw_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    void *raw = malloc((size_t)raw_len);
    if (raw == NULL || fread(raw, 1, (size_t)raw_len, fp) !=
                       (size_t)raw_len) {
        fprintf(stderr, "[%s] cannot read qoi file\n", tc->name);
        free(raw);
        fclose(fp);
        return 1;
    }
    fclose(fp);

    qoi_desc desc;
    memset(&desc, 0, sizeof(desc));
    void *decoded = qoi_decode(raw, (int)raw_len, &desc, IMG_CHANNELS);
    free(raw);
    if (decoded == NULL) {
        fprintf(stderr, "[%s] reference decoder rejected the stream\n",
                tc->name);
        return 1;
    }

    if (desc.width != IMG_WIDTH || desc.height != IMG_HEIGHT ||
        desc.channels != IMG_CHANNELS) {
        fprintf(stderr,
                "[%s] decoded header mismatch: %ux%u ch=%u\n",
                tc->name, desc.width, desc.height, desc.channels);
        free(decoded);
        return 1;
    }

    unsigned long diff_pixels = 0;
    const uint8_t *back = (const uint8_t *)decoded;
    for (size_t p = 0; p < pixel_bytes / IMG_CHANNELS; ++p) {
        if (memcmp(tc->pixels + p * IMG_CHANNELS,
                   back + p * IMG_CHANNELS,
                   IMG_CHANNELS) != 0) {
            ++diff_pixels;
        }
    }
    free(decoded);

    double saving = (png_bytes > 0)
        ? (1.0 - (double)qoi_bytes / (double)png_bytes) * 100.0
        : 0.0;
    printf("[%s] raw=%zu png=%ld bytes qoi=%ld bytes "
           "(vs png %+.1f%%) diff_pixels=%lu\n",
           tc->name, pixel_bytes, png_bytes, qoi_bytes,
           -saving, diff_pixels);

    if (qoi_bytes != raw_len) {
        fprintf(stderr, "[%s] size mismatch\n", tc->name);
        return 1;
    }
    return diff_pixels == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *out_dir = (argc > 1) ? argv[1] : "output";

    test_case cases[4];
    cases[0].name = "gradient";
    cases[1].name = "solid";
    cases[2].name = "transparent";
    cases[3].name = "noise";

    for (int i = 0; i < 4; ++i) {
        cases[i].pixels = alloc_pixels();
        snprintf(cases[i].png_path, sizeof(cases[i].png_path),
                 "%s/%s.png", out_dir, cases[i].name);
        snprintf(cases[i].qoi_path, sizeof(cases[i].qoi_path),
                 "%s/%s.qoi", out_dir, cases[i].name);
    }

    build_gradient(cases[0].pixels);
    build_solid(cases[1].pixels);
    build_transparent(cases[2].pixels);
    build_noise(cases[3].pixels);

    int failures = 0;
    for (int i = 0; i < 4; ++i) {
        failures += run_case(&cases[i]);
        free(cases[i].pixels);
    }

    if (failures == 0) {
        printf("all 4 cases: reference decoder round-trip exact, "
               "0 differing pixels\n");
    } else {
        printf("%d case(s) failed\n", failures);
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
