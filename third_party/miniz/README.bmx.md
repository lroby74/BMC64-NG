# miniz 3.1.2 (BMX vendored copy)

BMX vendors the upstream `miniz.c` and `miniz.h` files for the
allocation-free `tinfl_decompress()` raw-Deflate decoder used by the updater
and `tdefl_compress()` zlib stream encoder used by local PNG screenshots.
The updater parses ZIP records itself and does not use miniz's ZIP APIs.

- Upstream: <https://github.com/richgel999/miniz>
- Release: `3.1.2` (published 2026-07-01)
- Release archive: `miniz-3.1.2.zip`
- Archive SHA-256:
  `f0446d863f9c19926ad9483c523fdc42e42b8d4a6a431d27e09d49c79a140d9a`
- `miniz.c` SHA-256:
  `e2c1aeb66eef9191d8c3feb164db2def2335a61d039bf04ed849f6b042433b30`
- `miniz.h` SHA-256:
  `b53b62ed122e559b8f679e3cb787a0b0035fe87a58f909da0e44931678f4e85f`
- License: MIT/Unlicense-compatible upstream text in `LICENSE`

`miniz_tinfl.c` is the only BMX-authored file in this directory. It fixes the
compile-time profile to low-level inflate/deflate without stdio, internal
allocation, zlib compatibility wrappers, or ZIP archive APIs. The screenshot
worker owns the compressor state and streams one filtered row at a time. Keep the two upstream files byte-for-
byte unchanged when updating the pin.

