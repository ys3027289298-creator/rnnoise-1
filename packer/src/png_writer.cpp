#include "packer.h"

#include <cstdio>
#include <vector>

#include "lodepng.h"

/* Stable PNG encoding interface: a thin wrapper over lodepng. Do not change
 * its signature; the new QOI output path is added separately. */
extern "C" packer_status packer_write_png(const char *path,
                                          const uint8_t *pixels,
                                          uint32_t width,
                                          uint32_t height,
                                          int channels)
{
    if (path == nullptr || pixels == nullptr ||
        width == 0 || height == 0 ||
        (channels != 3 && channels != 4)) {
        return PACKER_ERR_INVALID;
    }

    LodePNGColorType color_type =
        (channels == 4) ? LCT_RGBA : LCT_RGB;

    std::vector<unsigned char> buffer;
    unsigned error = lodepng::encode(
        buffer, pixels, width, height, color_type, 8);
    if (error != 0) {
        return PACKER_ERR_ENCODE;
    }

    std::FILE *fp = std::fopen(path, "wb");
    if (fp == nullptr) {
        return PACKER_ERR_IO;
    }

    size_t written =
        std::fwrite(buffer.data(), 1, buffer.size(), fp);
    int close_error = std::fclose(fp);
    if (written != buffer.size() || close_error != 0) {
        return PACKER_ERR_IO;
    }
    return PACKER_OK;
}
