# Packer: PNG + QOI output

Self-contained image packing helper for the resource bundle. PNG output
keeps going through lodepng; QOI output is a new, from-scratch encoder
implementing the [QOI spec v1.0](https://qoiformat.org/qoi-specification.pdf).

## Layout

- `include/packer.h` — public interface.
  - `packer_write_png(...)`: existing PNG encoding interface, signature
    frozen; it only wraps lodepng.
  - `packer_write_qoi(...)` / `packer_encode_qoi(...)`: new QOI path.
- `src/png_writer.cpp` — lodepng wrapper (C++).
- `src/qoi_writer.c` — QOI encoder in plain C11.
- `third_party/lodepng` — upstream lodepng, unmodified.
- `third_party/qoi/qoi.h` — official reference library, used only by the
  test as the reference decoder/encoder; it is not linked into the encoder.
- `tests/test_qoi.c` — the four required cases.

## Encoder details (per spec)

- 14-byte header: magic `qoif`, big-endian width/height, channels (3 or 4),
  colorspace byte `0` (sRGB + linear alpha).
- 64-entry running index, hash
  `(r*3 + g*5 + b*7 + a*11) mod 64`, starting from an all-zero table with
  the implicit previous pixel `(0,0,0,255)`.
- Per-pixel op choice: `QOI_OP_RUN` for repeats (flushed at run 62 or end
  of stream), then `QOI_OP_INDEX` on a hash hit, then `QOI_OP_DIFF`,
  `QOI_OP_LUMA`, `QOI_OP_RGB`, and `QOI_OP_RGBA` when alpha changes.
- Stream ends with the 8-byte marker `0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x01`.

The test proves byte-identical output to the official reference encoder,
which implies identical op selection and hash behavior, and decodes every
stream back with the official reference decoder.

## Build and run

```sh
cd packer
make check          # builds and writes artifacts to ../output
```

The test program takes the output directory as its only argument
(defaults to `output`), generates four 128x128 RGBA images (gradient,
solid, transparent, deterministic noise), writes each as both `.png` and
`.qoi`, reads the QOI back through the reference decoder, and prints the
raw/PNG/QOI byte counts plus the number of differing pixels. It reads no
keyboard input and exits non-zero on any mismatch.
