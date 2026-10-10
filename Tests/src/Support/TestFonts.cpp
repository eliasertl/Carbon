#include "Support/TestFonts.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include <stb_image_write.h>

namespace Carbon::TestFonts
{
    namespace
    {
        // Big-endian writing, as every OpenType table is stored.
        struct Writer
        {
            std::vector<uint8_t> Bytes;

            void U8(uint32_t value) { Bytes.push_back(static_cast<uint8_t>(value)); }
            void U16(uint32_t value)
            {
                U8(value >> 8);
                U8(value);
            }
            void I16(int32_t value) { U16(static_cast<uint32_t>(value) & 0xFFFF); }
            void U32(uint32_t value)
            {
                U16(value >> 16);
                U16(value);
            }
            void Append(const std::vector<uint8_t>& bytes) { Bytes.insert(Bytes.end(), bytes.begin(), bytes.end()); }
            uint32_t Size() const { return static_cast<uint32_t>(Bytes.size()); }
        };

        constexpr uint32_t Tag(const char (&name)[5])
        {
            return (static_cast<uint32_t>(name[0]) << 24) | (static_cast<uint32_t>(name[1]) << 16) |
                   (static_cast<uint32_t>(name[2]) << 8) | static_cast<uint32_t>(name[3]);
        }

        // Puts the tables into one file: the offset table, the table directory sorted by tag, and the tables,
        // each on a four-byte boundary.
        std::vector<uint8_t> Assemble(const std::map<uint32_t, std::vector<uint8_t>>& tables)
        {
            const uint32_t count = static_cast<uint32_t>(tables.size());
            uint32_t power = 1;
            uint32_t selector = 0;
            while (power * 2 <= count)
            {
                power *= 2;
                selector++;
            }
            Writer file;
            file.U32(0x00010000);
            file.U16(count);
            file.U16(power * 16);
            file.U16(selector);
            file.U16(count * 16 - power * 16);

            uint32_t offset = 12 + count * 16;
            for (const auto& [tag, data] : tables)
            {
                uint32_t checksum = 0;
                for (size_t i = 0; i < data.size(); i++)
                    checksum += static_cast<uint32_t>(data[i]) << (24 - 8 * (i % 4));
                file.U32(tag);
                file.U32(checksum);
                file.U32(offset);
                file.U32(static_cast<uint32_t>(data.size()));
                offset += (static_cast<uint32_t>(data.size()) + 3) & ~3u;
            }
            for (const auto& entry : tables)
            {
                file.Append(entry.second);
                while (file.Size() % 4 != 0)
                    file.U8(0);
            }
            return file.Bytes;
        }

        struct Square
        {
            int32_t X0 = 0;
            int32_t Y0 = 0;
            int32_t X1 = 0;
            int32_t Y1 = 0;
        };

        // A glyph of the test fonts: its advance, its outline (none for an empty glyph) and its code point.
        struct TestGlyph
        {
            uint16_t Advance = UnitsPerEm;
            bool HasOutline = false;
            Square Outline;
            char32_t Codepoint = 0;
        };

        std::vector<uint8_t> MakeHead(bool hasOutlines)
        {
            Writer head;
            head.U32(0x00010000);
            head.U32(0x00010000);
            head.U32(0);
            head.U32(0x5F0F3CF5);
            head.U16(0x000B);
            head.U16(UnitsPerEm);
            for (int i = 0; i < 4; i++)
                head.U32(0); // created, modified
            head.I16(0);
            head.I16(0);
            head.I16(UnitsPerEm);
            head.I16(UnitsPerEm);
            head.U16(0);
            head.U16(8);
            head.I16(2);
            head.I16(hasOutlines ? 1 : 0); // long offsets in loca
            head.I16(0);
            return head.Bytes;
        }

        std::vector<uint8_t> MakeHhea(uint16_t glyphCount)
        {
            Writer hhea;
            hhea.U32(0x00010000);
            hhea.I16(800);
            hhea.I16(-200);
            hhea.I16(0);
            hhea.U16(UnitsPerEm);
            hhea.I16(0);
            hhea.I16(0);
            hhea.I16(UnitsPerEm);
            hhea.I16(1);
            hhea.I16(0);
            hhea.I16(0);
            for (int i = 0; i < 4; i++)
                hhea.I16(0);
            hhea.I16(0);
            hhea.U16(glyphCount);
            return hhea.Bytes;
        }

        std::vector<uint8_t> MakeMaxp(uint16_t glyphCount)
        {
            Writer maxp;
            maxp.U32(0x00010000);
            maxp.U16(glyphCount);
            maxp.U16(4); // points
            maxp.U16(1); // contours
            for (int i = 0; i < 2; i++)
                maxp.U16(0);
            maxp.U16(2); // zones
            for (int i = 0; i < 8; i++)
                maxp.U16(0);
            return maxp.Bytes;
        }

        std::vector<uint8_t> MakeOs2()
        {
            Writer os2;
            os2.U16(4);
            os2.I16(UnitsPerEm / 2);
            os2.U16(400);
            os2.U16(5);
            os2.U16(0);
            for (int i = 0; i < 10; i++)
                os2.I16(0);
            os2.I16(0);
            for (int i = 0; i < 10; i++)
                os2.U8(0);
            for (int i = 0; i < 4; i++)
                os2.U32(0);
            os2.U32(Tag("TEST"));
            os2.U16(0x0040);
            os2.U16(0x0020);
            os2.U16(0xFFFF);
            os2.I16(800);
            os2.I16(-200);
            os2.I16(0);
            os2.U16(800);
            os2.U16(200);
            os2.U32(1);
            os2.U32(0);
            os2.I16(500);
            os2.I16(700);
            os2.U16(0);
            os2.U16(32);
            os2.U16(1);
            return os2.Bytes;
        }

        std::vector<uint8_t> MakePost()
        {
            Writer post;
            post.U32(0x00030000);
            post.U32(0);
            post.I16(-100);
            post.I16(50);
            for (int i = 0; i < 5; i++)
                post.U32(0);
            return post.Bytes;
        }

        std::vector<uint8_t> MakeName()
        {
            Writer name;
            name.U16(0);
            name.U16(0);
            name.U16(6);
            return name.Bytes;
        }

        // cmap with one format 12 subtable (Windows, full Unicode).
        std::vector<uint8_t> MakeCmap(const std::vector<TestGlyph>& glyphs)
        {
            std::vector<std::pair<char32_t, uint16_t>> mappings;
            for (size_t glyph = 0; glyph < glyphs.size(); glyph++)
            {
                if (glyphs[glyph].Codepoint != 0)
                    mappings.emplace_back(glyphs[glyph].Codepoint, static_cast<uint16_t>(glyph));
            }
            std::sort(mappings.begin(), mappings.end());

            Writer cmap;
            cmap.U16(0);
            cmap.U16(1);
            cmap.U16(3);
            cmap.U16(10);
            cmap.U32(12);
            cmap.U16(12);
            cmap.U16(0);
            cmap.U32(16 + static_cast<uint32_t>(mappings.size()) * 12);
            cmap.U32(0);
            cmap.U32(static_cast<uint32_t>(mappings.size()));
            for (const auto& [codepoint, glyph] : mappings)
            {
                cmap.U32(codepoint);
                cmap.U32(codepoint);
                cmap.U32(glyph);
            }
            return cmap.Bytes;
        }

        std::vector<uint8_t> MakeHmtx(const std::vector<TestGlyph>& glyphs)
        {
            Writer hmtx;
            for (const TestGlyph& glyph : glyphs)
            {
                hmtx.U16(glyph.Advance);
                hmtx.I16(glyph.HasOutline ? glyph.Outline.X0 : 0);
            }
            return hmtx.Bytes;
        }

        // glyf and loca: each outline is a square, one contour of four points drawn clockwise.
        std::pair<std::vector<uint8_t>, std::vector<uint8_t>> MakeGlyf(const std::vector<TestGlyph>& glyphs)
        {
            Writer glyf;
            Writer loca;
            for (const TestGlyph& glyph : glyphs)
            {
                loca.U32(glyf.Size());
                if (!glyph.HasOutline)
                    continue;
                const Square& square = glyph.Outline;
                glyf.I16(1);
                glyf.I16(square.X0);
                glyf.I16(square.Y0);
                glyf.I16(square.X1);
                glyf.I16(square.Y1);
                glyf.U16(3);
                glyf.U16(0);
                for (int i = 0; i < 4; i++)
                    glyf.U8(0x01); // on the curve, coordinates as 16-bit deltas
                const int32_t xs[4] = {square.X0, square.X0, square.X1, square.X1};
                const int32_t ys[4] = {square.Y0, square.Y1, square.Y1, square.Y0};
                int32_t x = 0;
                for (const int32_t value : xs)
                {
                    glyf.I16(value - x);
                    x = value;
                }
                int32_t y = 0;
                for (const int32_t value : ys)
                {
                    glyf.I16(value - y);
                    y = value;
                }
                while (glyf.Size() % 4 != 0)
                    glyf.U8(0);
            }
            loca.U32(glyf.Size());
            return {glyf.Bytes, loca.Bytes};
        }

        std::map<uint32_t, std::vector<uint8_t>> MakeOutlineTables(const std::vector<TestGlyph>& glyphs)
        {
            const uint16_t count = static_cast<uint16_t>(glyphs.size());
            std::map<uint32_t, std::vector<uint8_t>> tables;
            tables[Tag("head")] = MakeHead(true);
            tables[Tag("hhea")] = MakeHhea(count);
            tables[Tag("maxp")] = MakeMaxp(count);
            tables[Tag("OS/2")] = MakeOs2();
            tables[Tag("post")] = MakePost();
            tables[Tag("name")] = MakeName();
            tables[Tag("cmap")] = MakeCmap(glyphs);
            tables[Tag("hmtx")] = MakeHmtx(glyphs);
            auto [glyf, loca] = MakeGlyf(glyphs);
            tables[Tag("glyf")] = std::move(glyf);
            tables[Tag("loca")] = std::move(loca);
            return tables;
        }

        constexpr Square Em = {0, 0, UnitsPerEm, UnitsPerEm};

        void AppendColor(Writer& writer, uint32_t rgb)
        {
            writer.U8(rgb & 0xFF);
            writer.U8((rgb >> 8) & 0xFF);
            writer.U8((rgb >> 16) & 0xFF);
            writer.U8(0xFF);
        }

        // A square image: `top` in the upper half, `bottom` in the lower one.
        std::vector<uint8_t> MakePng(uint32_t size, uint32_t top, uint32_t bottom)
        {
            std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
            for (size_t i = 0; i < pixels.size(); i += 4)
            {
                const uint32_t rgb = i / 4 < static_cast<size_t>(size) * (size / 2) ? top : bottom;
                pixels[i + 0] = static_cast<uint8_t>((rgb >> 16) & 0xFF);
                pixels[i + 1] = static_cast<uint8_t>((rgb >> 8) & 0xFF);
                pixels[i + 2] = static_cast<uint8_t>(rgb & 0xFF);
                pixels[i + 3] = 0xFF;
            }
            std::vector<uint8_t> png;
            stbi_write_png_to_func(
                [](void* context, void* data, int length)
                {
                    std::vector<uint8_t>& out = *static_cast<std::vector<uint8_t>*>(context);
                    out.insert(out.end(), static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + length);
                },
                &png, static_cast<int>(size), static_cast<int>(size), 4, pixels.data(), static_cast<int>(size) * 4);
            return png;
        }
    } // namespace

    std::vector<uint8_t> MakeColrFont()
    {
        // 0 .notdef, 1 the color glyph (its outline is what renderers without color draw), 2 the outer layer,
        // 3 the inner layer, 4 the plain glyph, 5 the second color glyph (outer layer only), 6 the joiner.
        std::vector<TestGlyph> glyphs(7);
        glyphs[1] = {UnitsPerEm, true, Em, ColorCodepoint};
        glyphs[2] = {UnitsPerEm, true, Em, 0};
        glyphs[3] = {UnitsPerEm, true, {100, 500, 500, 900}, 0}; // y points up: the top-left quarter
        glyphs[4] = {UnitsPerEm, true, Em, PlainCodepoint};
        glyphs[5] = {UnitsPerEm, true, Em, SecondColorCodepoint};
        glyphs[6] = {0, false, {}, ZwjCodepoint};
        std::map<uint32_t, std::vector<uint8_t>> tables = MakeOutlineTables(glyphs);

        Writer colr;
        colr.U16(0);
        colr.U16(2); // base glyphs
        colr.U32(14);
        colr.U32(14 + 2 * 6);
        colr.U16(3); // layers
        colr.U16(1);
        colr.U16(0);
        colr.U16(2);
        colr.U16(5);
        colr.U16(2);
        colr.U16(1);
        colr.U16(2); // layer 0: outer square, palette entry 0
        colr.U16(0);
        colr.U16(3); // layer 1: inner square, palette entry 1
        colr.U16(1);
        colr.U16(2); // layer 2: the second glyph's square
        colr.U16(0);
        tables[Tag("COLR")] = colr.Bytes;

        Writer cpal;
        cpal.U16(0);
        cpal.U16(2);
        cpal.U16(1);
        cpal.U16(2);
        cpal.U32(14);
        cpal.U16(0);
        AppendColor(cpal, OuterColor);
        AppendColor(cpal, InnerColor);
        tables[Tag("CPAL")] = cpal.Bytes;
        return Assemble(tables);
    }

    std::vector<uint8_t> MakeCbdtFont(uint8_t ppem)
    {
        // 0 .notdef, 1 and 2 the color glyphs, 3 the joiner. No outlines.
        std::vector<TestGlyph> glyphs(4);
        glyphs[1].Codepoint = ColorCodepoint;
        glyphs[2].Codepoint = SecondColorCodepoint;
        glyphs[3] = {0, false, {}, ZwjCodepoint};
        const uint16_t count = static_cast<uint16_t>(glyphs.size());

        std::map<uint32_t, std::vector<uint8_t>> tables;
        tables[Tag("head")] = MakeHead(false);
        tables[Tag("hhea")] = MakeHhea(count);
        tables[Tag("maxp")] = MakeMaxp(count);
        tables[Tag("OS/2")] = MakeOs2();
        tables[Tag("post")] = MakePost();
        tables[Tag("name")] = MakeName();
        tables[Tag("cmap")] = MakeCmap(glyphs);
        tables[Tag("hmtx")] = MakeHmtx(glyphs);

        // CBDT: the header, then per glyph small metrics, the PNG's length and the PNG.
        const std::vector<uint8_t> png = MakePng(ppem, OuterColor, InnerColor);
        Writer cbdt;
        cbdt.U16(3);
        cbdt.U16(0);
        std::vector<uint32_t> offsets;
        for (int glyph = 0; glyph < 2; glyph++)
        {
            offsets.push_back(cbdt.Size() - 4);
            cbdt.U8(ppem); // height
            cbdt.U8(ppem); // width
            cbdt.U8(0);    // bearing x
            cbdt.U8(ppem); // bearing y: the top of the image, above the baseline
            cbdt.U8(ppem); // advance
            cbdt.U32(static_cast<uint32_t>(png.size()));
            cbdt.Append(png);
        }
        offsets.push_back(cbdt.Size() - 4);
        tables[Tag("CBDT")] = cbdt.Bytes;

        // CBLC: one strike for glyphs 1 and 2, an index subtable of format 1 pointing into CBDT.
        Writer cblc;
        cblc.U16(3);
        cblc.U16(0);
        cblc.U32(1);
        cblc.U32(8 + 48);        // the index subtable array, after this size record
        cblc.U32(8 + 8 + 4 * 3); // its size with the subtable
        cblc.U32(1);
        cblc.U32(0);
        for (int metrics = 0; metrics < 2; metrics++)
        {
            cblc.U8(ppem * 4 / 5);
            cblc.U8(static_cast<uint8_t>(-static_cast<int>(ppem / 5)));
            cblc.U8(ppem);
            for (int i = 0; i < 9; i++)
                cblc.U8(0);
        }
        cblc.U16(1);
        cblc.U16(2);
        cblc.U8(ppem);
        cblc.U8(ppem);
        cblc.U8(32);
        cblc.U8(1);
        cblc.U16(1); // array: glyphs 1 to 2, subtable right after the array
        cblc.U16(2);
        cblc.U32(8);
        cblc.U16(1);  // index format 1
        cblc.U16(17); // image format 17: small metrics and PNG
        cblc.U32(4);  // image data after CBDT's header
        for (const uint32_t offset : offsets)
            cblc.U32(offset);
        tables[Tag("CBLC")] = cblc.Bytes;
        return Assemble(tables);
    }

    std::vector<uint8_t> MakeSbixFont(uint16_t ppem)
    {
        // The glyphs of the CBDT font: 0 .notdef, 1 and 2 the color glyphs, 3 the joiner. No outlines.
        std::vector<TestGlyph> glyphs(4);
        glyphs[1].Codepoint = ColorCodepoint;
        glyphs[2].Codepoint = SecondColorCodepoint;
        glyphs[3] = {0, false, {}, ZwjCodepoint};
        const uint16_t count = static_cast<uint16_t>(glyphs.size());

        std::map<uint32_t, std::vector<uint8_t>> tables;
        tables[Tag("head")] = MakeHead(false);
        tables[Tag("hhea")] = MakeHhea(count);
        tables[Tag("maxp")] = MakeMaxp(count);
        tables[Tag("OS/2")] = MakeOs2();
        tables[Tag("post")] = MakePost();
        tables[Tag("name")] = MakeName();
        tables[Tag("cmap")] = MakeCmap(glyphs);
        tables[Tag("hmtx")] = MakeHmtx(glyphs);

        // sbix: the header with one strike, then the strike: its size, a data offset per glyph and one past the
        // last, and the data of glyphs 1 and 2 (origin, graphic type and the PNG). Glyphs 0 and 3 have no image.
        const std::vector<uint8_t> png = MakePng(ppem, OuterColor, InnerColor);
        const uint32_t imageSize = 4 + 4 + static_cast<uint32_t>(png.size());
        const uint32_t firstImage = 4 + 4 * (count + 1u);
        Writer sbix;
        sbix.U16(1); // version
        sbix.U16(1); // flags: bit 0 is always set
        sbix.U32(1);
        sbix.U32(12); // the strike, after this header
        sbix.U16(ppem);
        sbix.U16(72); // pixels per inch
        sbix.U32(firstImage);
        sbix.U32(firstImage);
        sbix.U32(firstImage + imageSize);
        sbix.U32(firstImage + 2 * imageSize);
        sbix.U32(firstImage + 2 * imageSize);
        for (int glyph = 0; glyph < 2; glyph++)
        {
            sbix.I16(0); // origin: the image's bottom left on the baseline
            sbix.I16(0);
            sbix.U32(Tag("png "));
            sbix.Append(png);
        }
        tables[Tag("sbix")] = sbix.Bytes;
        return Assemble(tables);
    }

    std::vector<uint8_t> MakePlainFont()
    {
        std::vector<TestGlyph> glyphs(5);
        glyphs[1] = {UnitsPerEm, true, Em, ColorCodepoint};
        glyphs[2] = {UnitsPerEm, true, Em, PlainCodepoint};
        glyphs[3] = {0, false, {}, ZwjCodepoint};
        glyphs[4] = {UnitsPerEm, true, Em, SecondColorCodepoint};
        return Assemble(MakeOutlineTables(glyphs));
    }
} // namespace Carbon::TestFonts
