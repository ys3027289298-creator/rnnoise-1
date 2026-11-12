#include "packer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QOI_MAGIC0 0x71u /* 'q' */
#define QOI_MAGIC1 0x6fu /* 'o' */
#define QOI_MAGIC2 0x69u /* 'i' */
#define QOI_MAGIC3 0x66u /* 'f' */

#define QOI_HEADER_SIZE 14u
#define QOI_PADDING_SIZE 8u
#define QOI_INDEX_SIZE 64u
#define QOI_RUN_MAX 62

#define QOI_OP_INDEX 0x00u /* 00xxxxxx */
#define QOI_OP_DIFF  0x40u /* 01xxxxxx */
#define QOI_OP_LUMA  0x80u /* 10xxxxxx */
#define QOI_OP_RUN   0xc0u /* 11xxxxxx */
#define QOI_OP_RGB   0xfeu
#define QOI_OP_RGBA  0xffu

typedef struct {
    uint8_t r, g, b, a;
} qoi_pixel;

/* Spec section 4: index hash over the four sRGB-linear-alpha channels. */
static unsigned int qoi_hash(qoi_pixel px)
{
    return ((unsigned int)px.r * 3u +
            (unsigned int)px.g * 5u +
            (unsigned int)px.b * 7u +
            (unsigned int)px.a * 11u) & 63u;
}

static void put_big32(uint8_t *out, size_t *pos, uint32_t value)
{
    out[(*pos)++] = (uint8_t)((value >> 24) & 0xffu);
    out[(*pos)++] = (uint8_t)((value >> 16) & 0xffu);
    out[(*pos)++] = (uint8_t)((value >>  8) & 0xffu);
    out[(*pos)++] = (uint8_t)(value & 0xffu);
}

static void put_byte(uint8_t *out, size_t *pos, uint8_t value)
{
    out[(*pos)++] = value;
}

void packer_free(void *ptr)
{
    free(ptr);
}

uint8_t *packer_encode_qoi(const uint8_t *pixels,
                           uint32_t width,
                           uint32_t height,
                           int channels,
                           size_t *out_len)
{
    if (pixels == NULL || out_len == NULL ||
        width == 0 || height == 0 ||
        (channels != 3 && channels != 4)) {
        return NULL;
    }

    size_t pixel_count = (size_t)width * (size_t)height;
    /* Worst case: QOI_OP_RGBA/RGB style chunk per pixel plus header/marker. */
    size_t capacity = pixel_count * (size_t)(channels + 1) +
                      QOI_HEADER_SIZE + QOI_PADDING_SIZE;

    uint8_t *out = (uint8_t *)malloc(capacity);
    if (out == NULL) {
        return NULL;
    }

    size_t pos = 0;
    put_big32(out, &pos,
              (QOI_MAGIC0 << 24) | (QOI_MAGIC1 << 16) |
              (QOI_MAGIC2 << 8) | QOI_MAGIC3);
    put_big32(out, &pos, width);
    put_big32(out, &pos, height);
    put_byte(out, &pos, (uint8_t)channels);
    put_byte(out, &pos, 0u); /* colorspace: sRGB with linear alpha */

    qoi_pixel index[QOI_INDEX_SIZE];
    memset(index, 0, sizeof(index));

    qoi_pixel previous = { 0u, 0u, 0u, 255u };
    unsigned int run = 0;

    for (size_t i = 0; i < pixel_count; ++i) {
        const uint8_t *raw = pixels + i * (size_t)channels;
        qoi_pixel current;
        current.r = raw[0];
        current.g = raw[1];
        current.b = raw[2];
        current.a = (channels == 4) ? raw[3] : 255u;

        if (memcmp(&current, &previous, sizeof(qoi_pixel)) == 0) {
            /* Flush at 62 (tags 63/62 are reserved for QOI_OP_RGB/RGBA)
             * or when this is the final pixel. */
            if (++run == QOI_RUN_MAX || i + 1 == pixel_count) {
                put_byte(out, &pos, (uint8_t)(QOI_OP_RUN | (run - 1u)));
                run = 0;
            }
            previous = current;
            continue;
        }

        if (run > 0) {
            put_byte(out, &pos, (uint8_t)(QOI_OP_RUN | (run - 1u)));
            run = 0;
        }

        unsigned int slot = qoi_hash(current);
        if (memcmp(&index[slot], &current, sizeof(qoi_pixel)) == 0) {
            put_byte(out, &pos, (uint8_t)(QOI_OP_INDEX | slot));
        } else {
            index[slot] = current;

            if (current.a == previous.a) {
                int dr = (int)current.r - (int)previous.r;
                int dg = (int)current.g - (int)previous.g;
                int db = (int)current.b - (int)previous.b;

                if (dr >= -2 && dr <= 1 &&
                    dg >= -2 && dg <= 1 &&
                    db >= -2 && db <= 1) {
                    uint8_t chunk = (uint8_t)(
                        QOI_OP_DIFF |
                        ((dr + 2) << 4) |
                        ((dg + 2) << 2) |
                        (db + 2));
                    put_byte(out, &pos, chunk);
                } else {
                    int dr_dg = dr - dg;
                    int db_dg = db - dg;
                    if (dg >= -32 && dg <= 31 &&
                        dr_dg >= -8 && dr_dg <= 7 &&
                        db_dg >= -8 && db_dg <= 7) {
                        put_byte(out, &pos,
                                 (uint8_t)(QOI_OP_LUMA | (dg + 32)));
                        put_byte(out, &pos, (uint8_t)(
                            ((dr_dg + 8) << 4) | (db_dg + 8)));
                    } else {
                        put_byte(out, &pos, QOI_OP_RGB);
                        put_byte(out, &pos, current.r);
                        put_byte(out, &pos, current.g);
                        put_byte(out, &pos, current.b);
                    }
                }
            } else {
                put_byte(out, &pos, QOI_OP_RGBA);
                put_byte(out, &pos, current.r);
                put_byte(out, &pos, current.g);
                put_byte(out, &pos, current.b);
                put_byte(out, &pos, current.a);
            }
        }

        previous = current;
    }

    /* End marker: seven 0x00 bytes followed by 0x01. */
    for (unsigned int i = 0; i < 7u; ++i) {
        put_byte(out, &pos, 0u);
    }
    put_byte(out, &pos, 1u);

    *out_len = pos;
    return out;
}

packer_status packer_write_qoi(const char *path,
                               const uint8_t *pixels,
                               uint32_t width,
                               uint32_t height,
                               int channels)
{
    if (path == NULL) {
        return PACKER_ERR_INVALID;
    }

    size_t encoded_len = 0;
    uint8_t *encoded =
        packer_encode_qoi(pixels, width, height, channels, &encoded_len);
    if (encoded == NULL) {
        return PACKER_ERR_ENCODE;
    }

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        free(encoded);
        return PACKER_ERR_IO;
    }

    size_t written = fwrite(encoded, 1, encoded_len, fp);
    int close_error = fclose(fp);
    free(encoded);

    if (written != encoded_len || close_error != 0) {
        return PACKER_ERR_IO;
    }
    return PACKER_OK;
}
