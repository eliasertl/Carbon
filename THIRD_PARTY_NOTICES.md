# Third-party notices

Carbon is licensed under the MIT License (see [LICENSE](LICENSE)). It uses the third-party components below.
Each one is a git submodule under `ThirdParty/`, pinned to the listed version; the authoritative license text is
the file named in the last column inside that submodule.

## Linked into the `Carbon` library

| Component | Version | License | License file |
| --- | --- | --- | --- |
| [FreeType](https://freetype.org) | 2.14.3 | FreeType License (FTL) | `ThirdParty/FreeType/docs/FTL.TXT` |
| [HarfBuzz](https://harfbuzz.github.io) | 14.5.1 | "Old MIT" license | `ThirdParty/HarfBuzz/COPYING` |
| [Dawn](https://dawn.googlesource.com/dawn) | provided by the host | BSD 3-Clause | `LICENSE` in the Dawn repository |

FreeType is dual-licensed (FTL or GPLv2); Carbon uses it under the FreeType License. As that license requires:

> Portions of this software are copyright © The FreeType Project (www.freetype.org). All rights reserved.

Dawn is not bundled. The host application provides it and is responsible for its notices.

## Embedded in the `Carbon` library (fonts)

| Component | Version | License | License file |
| --- | --- | --- | --- |
| [Public Sans](https://github.com/uswds/public-sans) (variable, roman and italic) | 2.001 | SIL Open Font License 1.1 | `ThirdParty/PublicSans/OFL.txt` |
| [Phosphor Icons](https://phosphoricons.com) (regular, bold and fill fonts) | 2.1.2 | MIT | `ThirdParty/Phosphor/LICENSE` |

Public Sans — Copyright 2015 The Public Sans Project Authors (https://github.com/uswds/public-sans). The font
files are embedded unmodified and under their original name. The SIL Open Font License permits bundling and
embedding the fonts with software provided the copyright notice and license accompany the distribution: if you
ship a product built with Carbon, include this notice and the text of `OFL.txt`. The fonts themselves remain
under the OFL and are not relicensed under Carbon's MIT License.

Phosphor Icons — Copyright (c) 2020-2021 Phosphor Icons. The MIT license requires the copyright and permission
notice to be included with copies of the fonts; include `ThirdParty/Phosphor/LICENSE` when you redistribute.

## Used only by the examples and tests (not part of the libraries)

| Component | Version | License | License file |
| --- | --- | --- | --- |
| [GLFW](https://www.glfw.org) | 3.5.1 | zlib/libpng | `ThirdParty/GLFW/LICENSE.md` |
| [stb](https://github.com/nothings/stb) (`stb_image_write.h` 1.16) | commit `2c980bb` | MIT or public domain (Unlicense) | `ThirdParty/Stb/LICENSE` |
| [GoogleTest](https://github.com/google/googletest) | 1.18.0 | BSD 3-Clause | `ThirdParty/GoogleTest/LICENSE` |
