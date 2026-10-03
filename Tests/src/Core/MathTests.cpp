#include <gtest/gtest.h>

#include "Carbon/Carbon.h"

namespace Carbon
{
    TEST(Vec2Tests, Arithmetic)
    {
        const Vec2 a(1.0f, 2.0f);
        const Vec2 b(3.0f, 5.0f);
        EXPECT_EQ(a + b, Vec2(4.0f, 7.0f));
        EXPECT_EQ(b - a, Vec2(2.0f, 3.0f));
        EXPECT_EQ(a * 2.0f, Vec2(2.0f, 4.0f));
        EXPECT_EQ(2.0f * a, Vec2(2.0f, 4.0f));
        EXPECT_EQ(a * b, Vec2(3.0f, 10.0f));
        EXPECT_EQ(b / 2.0f, Vec2(1.5f, 2.5f));
        EXPECT_EQ(-a, Vec2(-1.0f, -2.0f));
        EXPECT_FLOAT_EQ(Dot(a, b), 13.0f);
        EXPECT_FLOAT_EQ(Vec2(3.0f, 4.0f).GetLength(), 5.0f);
    }

    TEST(Vec2Tests, MinMaxLerp)
    {
        const Vec2 a(1.0f, 8.0f);
        const Vec2 b(3.0f, 4.0f);
        EXPECT_EQ(Min(a, b), Vec2(1.0f, 4.0f));
        EXPECT_EQ(Max(a, b), Vec2(3.0f, 8.0f));
        EXPECT_EQ(Lerp(a, b, 0.0f), a);
        EXPECT_EQ(Lerp(a, b, 1.0f), b);
        EXPECT_EQ(Lerp(a, b, 0.5f), Vec2(2.0f, 6.0f));
    }

    TEST(RectTests, Accessors)
    {
        const Rect rect(10.0f, 20.0f, 100.0f, 50.0f);
        EXPECT_EQ(rect.GetMin(), Vec2(10.0f, 20.0f));
        EXPECT_EQ(rect.GetMax(), Vec2(110.0f, 70.0f));
        EXPECT_EQ(rect.GetCenter(), Vec2(60.0f, 45.0f));
        EXPECT_EQ(rect.GetSize(), Vec2(100.0f, 50.0f));
        EXPECT_EQ(Rect::FromMinMax(Vec2(10.0f, 20.0f), Vec2(110.0f, 70.0f)), rect);
        EXPECT_EQ(Rect::FromCenter(Vec2(60.0f, 45.0f), Vec2(100.0f, 50.0f)), rect);
        EXPECT_FALSE(rect.IsEmpty());
        EXPECT_TRUE(Rect(0.0f, 0.0f, 0.0f, 10.0f).IsEmpty());
    }

    TEST(RectTests, ContainsExcludesFarEdges)
    {
        const Rect rect(0.0f, 0.0f, 10.0f, 10.0f);
        EXPECT_TRUE(rect.Contains(Vec2(0.0f, 0.0f)));
        EXPECT_TRUE(rect.Contains(Vec2(9.99f, 9.99f)));
        EXPECT_FALSE(rect.Contains(Vec2(10.0f, 5.0f)));
        EXPECT_FALSE(rect.Contains(Vec2(5.0f, 10.0f)));
        EXPECT_FALSE(rect.Contains(Vec2(-0.01f, 5.0f)));
    }

    TEST(RectTests, IntersectionAndUnion)
    {
        const Rect a(0.0f, 0.0f, 10.0f, 10.0f);
        const Rect b(5.0f, 5.0f, 10.0f, 10.0f);
        const Rect c(20.0f, 20.0f, 5.0f, 5.0f);
        EXPECT_TRUE(a.Intersects(b));
        EXPECT_FALSE(a.Intersects(c));
        EXPECT_EQ(a.GetIntersection(b), Rect(5.0f, 5.0f, 5.0f, 5.0f));
        EXPECT_TRUE(a.GetIntersection(c).IsEmpty());
        EXPECT_EQ(a.GetUnion(c), Rect(0.0f, 0.0f, 25.0f, 25.0f));
        // Touching edges share no area.
        EXPECT_FALSE(a.Intersects(Rect(10.0f, 0.0f, 5.0f, 5.0f)));
    }

    TEST(RectTests, InsetExpandOffset)
    {
        const Rect rect(10.0f, 10.0f, 100.0f, 60.0f);
        EXPECT_EQ(rect.Inset(EdgeInsets(5.0f)), Rect(15.0f, 15.0f, 90.0f, 50.0f));
        EXPECT_EQ(rect.Inset(EdgeInsets(10.0f, 20.0f)), Rect(20.0f, 30.0f, 80.0f, 20.0f));
        EXPECT_EQ(rect.Inset(EdgeInsets(1.0f, 2.0f, 3.0f, 4.0f)), Rect(11.0f, 12.0f, 96.0f, 54.0f));
        // Insets larger than the rectangle clamp to an empty size instead of going negative.
        EXPECT_EQ(rect.Inset(EdgeInsets(80.0f)).GetSize(), Vec2(0.0f, 0.0f));
        EXPECT_EQ(rect.Expand(2.0f), Rect(8.0f, 8.0f, 104.0f, 64.0f));
        EXPECT_EQ(rect.Offset(Vec2(-10.0f, 5.0f)), Rect(0.0f, 15.0f, 100.0f, 60.0f));
    }

    TEST(EdgeInsetsTests, Constructors)
    {
        const EdgeInsets all = 8.0f;
        EXPECT_EQ(all, EdgeInsets(8.0f, 8.0f, 8.0f, 8.0f));
        const EdgeInsets symmetric(4.0f, 6.0f);
        EXPECT_EQ(symmetric, EdgeInsets(4.0f, 6.0f, 4.0f, 6.0f));
        EXPECT_FLOAT_EQ(symmetric.GetHorizontal(), 8.0f);
        EXPECT_FLOAT_EQ(symmetric.GetVertical(), 12.0f);
    }

    TEST(ColorTests, FromHexAndBytes)
    {
        const Color blue = Color::FromHex(0x007AFF);
        EXPECT_FLOAT_EQ(blue.R, 0.0f);
        EXPECT_FLOAT_EQ(blue.G, 122.0f / 255.0f);
        EXPECT_FLOAT_EQ(blue.B, 1.0f);
        EXPECT_FLOAT_EQ(blue.A, 1.0f);
        EXPECT_EQ(Color::FromRGBA8(0, 122, 255), blue);
        EXPECT_FLOAT_EQ(Color::FromHex(0xFFFFFF, 0.5f).A, 0.5f);
    }

    TEST(ColorTests, PacksRedInLowestByte)
    {
        EXPECT_EQ(Color::FromRGBA8(0x11, 0x22, 0x33, 0x44).ToRGBA8(), 0x44332211u);
        // Out-of-range components clamp.
        EXPECT_EQ(Color(2.0f, -1.0f, 0.0f, 1.0f).ToRGBA8(), 0xFF0000FFu);
    }

    TEST(ColorTests, LerpEndpointsAndTransparency)
    {
        const Color red(1.0f, 0.0f, 0.0f, 1.0f);
        const Color blue(0.0f, 0.0f, 1.0f, 1.0f);
        EXPECT_EQ(Lerp(red, blue, 0.0f), red);
        EXPECT_EQ(Lerp(red, blue, 1.0f), blue);

        // Fading from fully transparent must not drag the transparent color's RGB into the blend.
        const Color clearBlack = Color::Transparent();
        const Color half = Lerp(clearBlack, red, 0.5f);
        EXPECT_FLOAT_EQ(half.R, 1.0f);
        EXPECT_FLOAT_EQ(half.A, 0.5f);
    }

    TEST(ContentScaleTests, ConvertsBetweenPointsAndPixels)
    {
        const ContentScale scale{1.5f};
        EXPECT_FLOAT_EQ(scale.ToPixels(10.0f), 15.0f);
        EXPECT_FLOAT_EQ(scale.ToPoints(15.0f), 10.0f);
        EXPECT_EQ(scale.ToPixels(Vec2(2.0f, 4.0f)), Vec2(3.0f, 6.0f));
        EXPECT_FLOAT_EQ(scale.GetPixelSize(), 1.0f / 1.5f);
    }

    TEST(ContentScaleTests, SnapsToPixelBoundaries)
    {
        const ContentScale scale{1.5f};
        // 10.4 pt = 15.6 px, which rounds to 16 px = 10.667 pt.
        EXPECT_FLOAT_EQ(scale.Snap(10.4f), 16.0f / 1.5f);
        // Snapped coordinates land on whole pixels.
        const float pixels = scale.ToPixels(scale.Snap(7.3f));
        EXPECT_NEAR(pixels, std::round(pixels), 1e-4f);

        const ContentScale identity{1.0f};
        EXPECT_FLOAT_EQ(identity.Snap(10.4f), 10.0f);
        EXPECT_FLOAT_EQ(identity.Snap(10.6f), 11.0f);

        const Rect snapped = ContentScale{2.0f}.Snap(Rect(0.2f, 0.3f, 10.1f, 10.1f));
        EXPECT_FLOAT_EQ(snapped.X, 0.0f);
        EXPECT_FLOAT_EQ(snapped.Y, 0.5f);
        EXPECT_FLOAT_EQ(snapped.GetRight(), 10.5f);
        EXPECT_FLOAT_EQ(snapped.GetBottom(), 10.5f);
    }

    TEST(MathTests, ScalarHelpers)
    {
        EXPECT_FLOAT_EQ(Lerp(2.0f, 4.0f, 0.25f), 2.5f);
        EXPECT_FLOAT_EQ(Saturate(-1.0f), 0.0f);
        EXPECT_FLOAT_EQ(Saturate(0.3f), 0.3f);
        EXPECT_FLOAT_EQ(Saturate(7.0f), 1.0f);
        EXPECT_TRUE(NearlyEqual(1.0f, 1.000001f));
        EXPECT_FALSE(NearlyEqual(1.0f, 1.1f));
    }
} // namespace Carbon
