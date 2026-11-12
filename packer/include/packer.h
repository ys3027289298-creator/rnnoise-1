#ifndef PACKER_H
#define PACKER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PACKER_OK = 0,
    PACKER_ERR_INVALID = 1,
    PACKER_ERR_IO = 2,
    PACKER_ERR_ENCODE = 3
} packer_status;

/*
 * Encode an 8-bit sRGB image (channels = 3 for RGB, 4 for RGBA) to a PNG
 * file. This entry point wraps lodepng and its signature is the established
 * PNG encoding interface of the packer: it must not change.
 */
packer_status packer_write_png(const char *path,
                               const uint8_t *pixels,
                               uint32_t width,
                               uint32_t height,
                               int channels);

/*
 * Encode the same kind of image buffer to a QOI file (QOI spec v1.0).
 * Writes the "qoif" header, chooses per-pixel ops between the previous
 * pixel and the 64-entry spec hash index, and appends the 0x01 + seven
 * 0x00 end marker. Returns PACKER_OK on success.
 */
packer_status packer_write_qoi(const char *path,
                               const uint8_t *pixels,
                               uint32_t width,
                               uint32_t height,
                               int channels);

/* In-memory QOI encoding, useful for tests. *out_len receives the size.
 * The caller owns the returned buffer and frees it with packer_free(). */
uint8_t *packer_encode_qoi(const uint8_t *pixels,
                           uint32_t width,
                           uint32_t height,
                           int channels,
                           size_t *out_len);

void packer_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* PACKER_H */
