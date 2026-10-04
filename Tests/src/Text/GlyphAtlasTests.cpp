#include <gtest/gtest.h>

#include <random>
#include <vector>

#include "Carbon/Text/Internal/GlyphAtlas.h"

namespace Carbon::Internal
{
    namespace
    {
        bool Overlaps(const AtlasRegion& a, const AtlasRegion& b)
        {
            return a.X < b.X + b.Width && b.X < a.X + a.Width && a.Y < b.Y + b.Height && b.Y < a.Y + a.Height;
        }

        // Inserts a bitmap filled with `value` and returns its region.
        bool InsertFilled(GlyphAtlas& atlas, uint32_t width, uint32_t height, uint8_t value, AtlasRegion& region)
        {
            const std::vector<uint8_t> pixels(static_cast<size_t>(width) * height, value);
            return atlas.Insert(width, height, pixels.data(), static_cast<int32_t>(width), region);
        }

        bool IsFilledWith(const GlyphAtlas& atlas, const AtlasRegion& region, uint8_t value)
        {
            for (uint32_t y = region.Y; y < region.Y + region.Height; y++)
            {
                for (uint32_t x = region.X; x < region.X + region.Width; x++)
                {
                    if (atlas.GetPixels()[static_cast<size_t>(y) * atlas.GetWidth() + x] != value)
                        return false;
                }
            }
            return true;
        }
    } // namespace

    TEST(GlyphAtlasTests, RegionsStayInBoundsAndNeverOverlap)
    {
        GlyphAtlas atlas(256, 256, 256);
        std::mt19937 random(1234);
        std::uniform_int_distribution<uint32_t> size(3, 24);

        std::vector<AtlasRegion> regions;
        for (int i = 0; i < 200; i++)
        {
            AtlasRegion region;
            if (!InsertFilled(atlas, size(random), size(random), static_cast<uint8_t>(1 + i % 250), region))
                break;
            EXPECT_LE(region.X + region.Width, atlas.GetWidth());
            EXPECT_LE(region.Y + region.Height, atlas.GetHeight());
            regions.push_back(region);
        }
        ASSERT_GT(regions.size(), 100u);

        for (size_t a = 0; a < regions.size(); a++)
        {
            for (size_t b = a + 1; b < regions.size(); b++)
                EXPECT_FALSE(Overlaps(regions[a], regions[b])) << "regions " << a << " and " << b;
        }

        // Each glyph's pixels are intact: later insertions did not overwrite earlier ones.
        for (size_t i = 0; i < regions.size(); i++)
            EXPECT_TRUE(IsFilledWith(atlas, regions[i], static_cast<uint8_t>(1 + i % 250)));
    }

    TEST(GlyphAtlasTests, GlyphsAreSeparatedByAGutter)
    {
        GlyphAtlas atlas(64, 64, 64);
        AtlasRegion first;
        AtlasRegion second;
        ASSERT_TRUE(InsertFilled(atlas, 10, 10, 255, first));
        ASSERT_TRUE(InsertFilled(atlas, 10, 10, 255, second));

        // Nothing touches the border, and neighbours leave at least one empty texel between them.
        EXPECT_GE(first.X, 1u);
        EXPECT_GE(first.Y, 1u);
        const bool separatedHorizontally =
            second.X >= first.X + first.Width + 1 || first.X >= second.X + second.Width + 1;
        const bool separatedVertically =
            second.Y >= first.Y + first.Height + 1 || first.Y >= second.Y + second.Height + 1;
        EXPECT_TRUE(separatedHorizontally || separatedVertically);
    }

    TEST(GlyphAtlasTests, PacksTightly)
    {
        // 16x16 glyphs with a one-texel gutter need 17x17 cells: a 256x256 atlas holds 15 x 15 of them.
        GlyphAtlas atlas(256, 256, 256);
        int count = 0;
        AtlasRegion region;
        while (InsertFilled(atlas, 16, 16, 200, region))
            count++;
        EXPECT_EQ(count, 225);
    }

    TEST(GlyphAtlasTests, GrowsWhenFullAndKeepsExistingGlyphs)
    {
        GlyphAtlas atlas(64, 64, 256);
        const uint32_t initialGeneration = atlas.GetGeneration();

        std::vector<AtlasRegion> regions;
        for (int i = 0; i < 60; i++)
        {
            AtlasRegion region;
            ASSERT_TRUE(InsertFilled(atlas, 20, 20, static_cast<uint8_t>(i + 1), region));
            regions.push_back(region);
        }

        EXPECT_GT(atlas.GetWidth(), 64u);
        EXPECT_LE(atlas.GetWidth(), 256u);
        EXPECT_GT(atlas.GetGeneration(), initialGeneration);
        EXPECT_EQ(atlas.GetPixels().size(), static_cast<size_t>(atlas.GetWidth()) * atlas.GetHeight());

        // Texel coordinates handed out before the growth are still valid and still hold the same pixels.
        for (size_t i = 0; i < regions.size(); i++)
        {
            EXPECT_TRUE(IsFilledWith(atlas, regions[i], static_cast<uint8_t>(i + 1)));
            for (size_t other = i + 1; other < regions.size(); other++)
                EXPECT_FALSE(Overlaps(regions[i], regions[other]));
        }
    }

    TEST(GlyphAtlasTests, FailsOnlyWhenTheMaximumSizeIsFull)
    {
        GlyphAtlas atlas(32, 32, 64);
        AtlasRegion region;
        int count = 0;
        while (InsertFilled(atlas, 30, 30, 255, region))
            count++;
        // A 64x64 atlas has room for four 30x30 glyphs (31x31 cells, one-texel border).
        EXPECT_EQ(count, 4);
        EXPECT_EQ(atlas.GetWidth(), 64u);
        EXPECT_EQ(atlas.GetHeight(), 64u);

        // A glyph larger than the maximum size can never fit.
        EXPECT_FALSE(InsertFilled(atlas, 100, 100, 255, region));
    }

    TEST(GlyphAtlasTests, TracksDirtyRows)
    {
        GlyphAtlas atlas(128, 128, 128);
        EXPECT_FALSE(atlas.IsDirty());

        AtlasRegion region;
        ASSERT_TRUE(InsertFilled(atlas, 10, 12, 255, region));
        EXPECT_TRUE(atlas.IsDirty());
        EXPECT_EQ(atlas.GetDirtyMinY(), region.Y);
        EXPECT_EQ(atlas.GetDirtyMaxY(), region.Y + region.Height);

        atlas.ClearDirty();
        EXPECT_FALSE(atlas.IsDirty());

        // Empty glyphs (spaces) succeed without touching the atlas.
        ASSERT_TRUE(atlas.Insert(0, 0, nullptr, 0, region));
        EXPECT_EQ(region.Width, 0u);
        EXPECT_FALSE(atlas.IsDirty());
    }

    TEST(GlyphAtlasTests, ClearEmptiesTheAtlasAndBumpsTheGeneration)
    {
        GlyphAtlas atlas(64, 64, 64);
        AtlasRegion region;
        while (InsertFilled(atlas, 20, 20, 255, region))
        {
        }
        const uint32_t generation = atlas.GetGeneration();

        atlas.Clear();
        EXPECT_GT(atlas.GetGeneration(), generation);
        for (uint8_t pixel : atlas.GetPixels())
            ASSERT_EQ(pixel, 0);
        EXPECT_TRUE(InsertFilled(atlas, 20, 20, 255, region));
    }

    TEST(GlyphAtlasTests, RespectsThePitchOfTheSourceBitmap)
    {
        GlyphAtlas atlas(64, 64, 64);
        // A 3x2 bitmap stored with 5 bytes per row; the padding bytes must not be copied.
        const uint8_t pixels[10] = {1, 2, 3, 99, 99, 4, 5, 6, 99, 99};
        AtlasRegion region;
        ASSERT_TRUE(atlas.Insert(3, 2, pixels, 5, region));
        const std::vector<uint8_t>& stored = atlas.GetPixels();
        const size_t row0 = static_cast<size_t>(region.Y) * atlas.GetWidth() + region.X;
        const size_t row1 = row0 + atlas.GetWidth();
        EXPECT_EQ(stored[row0], 1);
        EXPECT_EQ(stored[row0 + 2], 3);
        EXPECT_EQ(stored[row0 + 3], 0);
        EXPECT_EQ(stored[row1], 4);
        EXPECT_EQ(stored[row1 + 2], 6);
    }
} // namespace Carbon::Internal
