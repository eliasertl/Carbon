# Third-party notices

Carbon is licensed under the MIT License (see [LICENSE](LICENSE)). It uses the third-party components below.
Each one, except Dawn and Emscripten's GLFW port, is a git submodule under `ThirdParty/`, pinned to the listed
version; the authoritative license text is the file named in the last column inside that submodule.
`cmake --install` copies this file to `share/doc/Carbon` and the license texts of everything that ends up inside
the installed libraries (Public Sans, JetBrains Mono, Phosphor, FreeType, HarfBuzz, stb) to
`share/doc/Carbon/Licenses`.

## Linked into the `Carbon` library

| Component | Version | License | License file |
| --- | --- | --- | --- |
| [FreeType](https://freetype.org) | 2.14.3 | FreeType License (FTL) | `ThirdParty/FreeType/docs/FTL.TXT` |
| [HarfBuzz](https://harfbuzz.github.io) (with its raster library, which paints color glyphs) | 14.5.1 | "Old MIT" license | `ThirdParty/HarfBuzz/COPYING` |
| [stb](https://github.com/nothings/stb) (`stb_image.h` 2.30, PNG only, decodes the glyphs of color fonts) | commit `2c980bb` | MIT or public domain (Unlicense) | `ThirdParty/Stb/LICENSE` |
| [Dawn](https://dawn.googlesource.com/dawn) | provided by the host | BSD 3-Clause | `LICENSE` in the Dawn repository |

HarfBuzz's raster library is built from the HarfBuzz submodule without libpng, and FreeType without its optional
PNG, zlib, bzip2 and Brotli support, so no further third-party code ends up in the library.

FreeType is dual-licensed (FTL or GPLv2); Carbon uses it under the FreeType License. As that license requires:

> Portions of this software are copyright © The FreeType Project (www.freetype.org). All rights reserved.

Dawn is not bundled and is used only by the WebGPU backend. The host application provides it (Carbon is
developed against Dawn commit `9115802`) and is responsible for its notices. The other backends call graphics
APIs of the system or SDK (Vulkan, OpenGL, OpenGL ES / WebGL 2, Direct3D 11 and 9) and bundle nothing.

## Embedded in the `Carbon` library (fonts)

| Component | Version | License | License file |
| --- | --- | --- | --- |
| [Public Sans](https://github.com/uswds/public-sans) (variable, roman and italic) | 2.001 | SIL Open Font License 1.1 | `ThirdParty/PublicSans/OFL.txt` |
| [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) (variable, upright and italic) | 2.304 | SIL Open Font License 1.1 | `ThirdParty/JetBrainsMono/OFL.txt` |
| [Phosphor Icons](https://phosphoricons.com) (regular, bold and fill fonts) | 2.1.2 | MIT | `ThirdParty/Phosphor/LICENSE` |

Public Sans — Copyright 2015 The Public Sans Project Authors (https://github.com/uswds/public-sans). The font
files are embedded unmodified and under their original name. The SIL Open Font License permits bundling and
embedding the fonts with software provided the copyright notice and license accompany the distribution: if you
ship a product built with Carbon, include this notice and the text of `OFL.txt`. The fonts themselves remain
under the OFL and are not relicensed under Carbon's MIT License.

JetBrains Mono — Copyright 2020 The JetBrains Mono Project Authors (https://github.com/JetBrains/JetBrainsMono).
It is embedded unmodified, under its original name and under the same terms as Public Sans: if you ship a product
built with Carbon, include this notice and the text of its `OFL.txt` (installed as `JetBrainsMono-OFL.txt`).

Phosphor Icons — Copyright (c) 2020-2021 Phosphor Icons. The MIT license requires the copyright and permission
notice to be included with copies of the fonts; include `ThirdParty/Phosphor/LICENSE` when you redistribute.

## Used only by the examples, tests and benchmarks (not part of the libraries)

| Component | Version | License | License file |
| --- | --- | --- | --- |
| [GLFW](https://www.glfw.org) | 3.5.1 | zlib/libpng | `ThirdParty/GLFW/LICENSE.md` |
| [stb](https://github.com/nothings/stb) (`stb_image_write.h` 1.16, writes PNG files) | commit `2c980bb` | MIT or public domain (Unlicense) | `ThirdParty/Stb/LICENSE` |
| [GoogleTest](https://github.com/google/googletest) | 1.18.0 | BSD 3-Clause | `ThirdParty/GoogleTest/LICENSE` |
| [Google Benchmark](https://github.com/google/benchmark) | 1.9.5 | Apache License 2.0 | `ThirdParty/GoogleBenchmark/LICENSE` |
| [Emscripten GLFW port](https://github.com/pongasoft/emscripten-glfw) (`contrib.glfw3`, GLFW 3.4 API; web builds only, replaces the GLFW submodule there) | fetched by Emscripten | Apache License 2.0 | the license file in that repository |

The examples add the system's CJK and emoji fonts as fallbacks at run time when they are installed; they are read
from the system and never bundled.
