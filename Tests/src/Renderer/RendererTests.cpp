#include "Support/BackendTest.h"

#include <cmath>

namespace Carbon
{
    namespace
    {
        // Channels are 8-bit, and blending rounds: allow two steps.
        constexpr float ChannelTolerance = 2.5f / 255.0f;

        void ExpectColorNear(const Color& actual, const Color& expected, float tolerance = ChannelTolerance)
        {
            EXPECT_NEAR(actual.R, expected.R, tolerance);
            EXPECT_NEAR(actual.G, expected.G, tolerance);
            EXPECT_NEAR(actual.B, expected.B, tolerance);
        }
    } // namespace

    using RendererTests = BackendTest;

    TEST_P(RendererTests, PixelAlignedRectHasCrispEdges)
    {
        const Color red = Color::FromHex(0xFF3B30);
        const RenderedImage image = RenderFrame(64.0f, 48.0f, 1.0f, Color::White(), [&](DrawList& drawList)
                                                { drawList.AddRect(Rect(10.0f, 8.0f, 20.0f, 16.0f), red); });
        ASSERT_EQ(image.Width, 64u);
        ASSERT_EQ(image.Height, 48u);

        ExpectColorNear(image.GetPixel(20, 16), red);           // inside
        ExpectColorNear(image.GetPixel(10, 8), red);            // first pixel inside, top-left
        ExpectColorNear(image.GetPixel(29, 23), red);           // last pixel inside, bottom-right
        ExpectColorNear(image.GetPixel(9, 16), Color::White()); // one pixel outside on each side: untouched
        ExpectColorNear(image.GetPixel(30, 16), Color::White());
        ExpectColorNear(image.GetPixel(20, 7), Color::White());
        ExpectColorNear(image.GetPixel(20, 24), Color::White());
    }

    TEST_P(RendererTests, ShaderMatchesTheCpuSquircleFunction)
    {
        // Black squircle on white: each pixel's darkness is the shader's coverage. It must match the CPU shape
        // function, which the unit tests verify, at fractional content scales too.
        const Rect shape(12.0f, 10.0f, 70.0f, 44.0f);
        const float radius = 16.0f;
        for (const float contentScale : {1.0f, 1.5f, 2.0f})
        {
            for (const float smoothing : {0.0f, 0.6f, 1.0f})
            {
                const RenderedImage image =
                    RenderFrame(96.0f, 64.0f, contentScale, Color::White(), [&](DrawList& drawList)
                                { drawList.AddSquircle(shape, Color::Black(), radius, smoothing); });

                float worst = 0.0f;
                for (uint32_t y = 0; y < image.Height; y++)
                {
                    for (uint32_t x = 0; x < image.Width; x++)
                    {
                        const Vec2 point = Vec2(float(x) + 0.5f, float(y) + 0.5f) / contentScale - shape.GetCenter();
                        const float distance = SquircleDistance(point, shape.GetSize() * 0.5f, radius, smoothing);
                        const float expected = SquircleCoverage(distance, contentScale);
                        const float actual = 1.0f - image.GetPixel(x, y).R;
                        worst = std::max(worst, std::abs(actual - expected));
                    }
                }
                EXPECT_LT(worst, 0.02f) << "scale " << contentScale << ", smoothing " << smoothing;
            }
        }
    }

    TEST_P(RendererTests, StrokeLiesInsideTheShape)
    {
        const Rect shape(10.0f, 10.0f, 60.0f, 40.0f);
        const RenderedImage image = RenderFrame(80.0f, 60.0f, 2.0f, Color::White(), [&](DrawList& drawList)
                                                { drawList.AddSquircleStroke(shape, Color::Black(), 0.0f, 3.0f); });

        // Sampled along the horizontal center line, in pixels (scale 2): outside, stroke, hollow middle.
        const uint32_t y = 60;
        ExpectColorNear(image.GetPixel(18, y), Color::White()); // 9.25 pt: outside
        ExpectColorNear(image.GetPixel(21, y), Color::Black()); // 10.75 pt: in the stroke
        ExpectColorNear(image.GetPixel(25, y), Color::Black()); // 12.75 pt: still in the 3 pt stroke
        ExpectColorNear(image.GetPixel(27, y), Color::White()); // 13.75 pt: past it
        ExpectColorNear(image.GetPixel(80, y), Color::White()); // center
    }

    TEST_P(RendererTests, ClipRectBecomesAScissor)
    {
        const Color blue = Color::FromHex(0x007AFF);
        const RenderedImage image = RenderFrame(64.0f, 64.0f, 1.0f, Color::White(),
                                                [&](DrawList& drawList)
                                                {
                                                    drawList.PushClipRect(Rect(16.0f, 16.0f, 16.0f, 16.0f));
                                                    drawList.AddRect(Rect(0.0f, 0.0f, 64.0f, 64.0f), blue);
                                                    drawList.PopClipRect();
                                                });
        ExpectColorNear(image.GetPixel(24, 24), blue);
        ExpectColorNear(image.GetPixel(16, 16), blue);
        ExpectColorNear(image.GetPixel(31, 31), blue);
        ExpectColorNear(image.GetPixel(15, 24), Color::White());
        ExpectColorNear(image.GetPixel(32, 24), Color::White());
        ExpectColorNear(image.GetPixel(24, 15), Color::White());
        ExpectColorNear(image.GetPixel(24, 32), Color::White());
    }

    TEST_P(RendererTests, LayersDrawBackToFrontAndAlphaBlends)
    {
        const Color green = Color::FromHex(0x34C759);
        const Color red = Color::FromHex(0xFF3B30);
        const RenderedImage image =
            RenderFrame(64.0f, 32.0f, 1.0f, Color::White(),
                        [&](DrawList& drawList)
                        {
                            // The overlay is submitted first but must end up on top.
                            drawList.PushLayer(DrawLayer::Overlay);
                            drawList.AddRect(Rect(8.0f, 8.0f, 16.0f, 16.0f), green);
                            drawList.PopLayer();
                            drawList.AddRect(Rect(0.0f, 0.0f, 32.0f, 32.0f), red);
                            // Half-transparent black over white.
                            drawList.AddRect(Rect(40.0f, 0.0f, 16.0f, 16.0f), Color::Black().WithAlpha(0.5f));
                        });
        ExpectColorNear(image.GetPixel(16, 16), green);
        ExpectColorNear(image.GetPixel(4, 4), red);
        ExpectColorNear(image.GetPixel(48, 8), Color(0.5f, 0.5f, 0.5f));
    }

    TEST_P(RendererTests, TextIsRasterizedIntoTheAtlasAndDrawn)
    {
        TextSpec spec;
        spec.Size = 32.0f;
        spec.Weight = FontWeight::Black;
        const RenderedImage image = RenderFrame(120.0f, 48.0f, 1.0f, Color::White(), [&](DrawList& drawList)
                                                { drawList.AddText(Vec2(8.0f, 4.0f), "HHH", spec, Color::Black()); });

        // Count ink: heavy capital H's cover a good part of their line, and nothing is drawn far from the text.
        size_t darkPixels = 0;
        for (uint32_t y = 0; y < image.Height; y++)
        {
            for (uint32_t x = 0; x < image.Width; x++)
            {
                if (image.GetPixel(x, y).R < 0.2f)
                    darkPixels++;
            }
        }
        EXPECT_GT(darkPixels, 600u);
        EXPECT_LT(darkPixels, 2500u);
        ExpectColorNear(image.GetPixel(115, 44), Color::White());
        ExpectColorNear(image.GetPixel(2, 2), Color::White());

        // Vertical stems are crisp: somewhere on the middle row there are fully black pixels.
        bool hasSolidInk = false;
        for (uint32_t x = 0; x < image.Width; x++)
            hasSolidInk = hasSolidInk || image.GetPixel(x, 24).R < 0.02f;
        EXPECT_TRUE(hasSolidInk);
    }

    TEST_P(RendererTests, HostTexturesAreDrawnAndTinted)
    {
        // A 2x2 texture: red, green / blue, white.
        const uint8_t texels[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
        const size_t texture = m_Harness->CreateTexture(2, 2, texels);

        const RenderedImage image =
            RenderFrame(96.0f, 48.0f, 1.0f, Color::Black(),
                        [&](DrawList& drawList)
                        {
                            const TextureID id = m_Harness->GetTextureID(texture);
                            EXPECT_NE(id, TextureID());
                            drawList.AddImage(id, Rect(8.0f, 8.0f, 32.0f, 32.0f));
                            // The same texture, tinted: only the red channel survives.
                            drawList.AddImage(id, Rect(56.0f, 8.0f, 32.0f, 32.0f), Rect(0.0f, 0.0f, 1.0f, 1.0f),
                                              Color(1.0f, 0.0f, 0.0f));
                        });

        // Texel centers of the 2x2 image land at 1/4 and 3/4 of the rectangle. The samples are taken just
        // outside of them, where linear filtering clamps to a single texel.
        ExpectColorNear(image.GetPixel(15, 15), Color(1.0f, 0.0f, 0.0f));
        ExpectColorNear(image.GetPixel(32, 15), Color(0.0f, 1.0f, 0.0f));
        ExpectColorNear(image.GetPixel(15, 32), Color(0.0f, 0.0f, 1.0f));
        ExpectColorNear(image.GetPixel(32, 32), Color(1.0f, 1.0f, 1.0f));
        ExpectColorNear(image.GetPixel(4, 4), Color::Black()); // outside the image

        ExpectColorNear(image.GetPixel(63, 15), Color(1.0f, 0.0f, 0.0f)); // red texel stays red
        ExpectColorNear(image.GetPixel(80, 15), Color::Black());          // green texel is tinted away
        ExpectColorNear(image.GetPixel(80, 32), Color(1.0f, 0.0f, 0.0f)); // white texel becomes red
    }

    TEST_P(RendererTests, BuffersGrowAndAreReusedAcrossFrames)
    {
        // A small frame, a large frame (forcing the buffers to grow), then a small one again.
        const Color gray = Color(0.5f, 0.5f, 0.5f);
        const std::function<void(DrawList&)> small = [&](DrawList& drawList)
        { drawList.AddRect(Rect(0.0f, 0.0f, 8.0f, 8.0f), gray); };
        const std::function<void(DrawList&)> large = [&](DrawList& drawList)
        {
            for (int i = 0; i < 4000; i++)
            {
                const float x = float(i % 64);
                const float y = float((i / 64) % 64);
                drawList.AddSquircle(Rect(x, y, 1.0f, 1.0f), gray, float(i % 7) * 0.05f);
            }
        };

        ExpectColorNear(RenderFrame(64.0f, 64.0f, 1.0f, Color::White(), small).GetPixel(4, 4), gray);
        const RenderedImage filled = RenderFrame(64.0f, 64.0f, 1.0f, Color::White(), large);
        ExpectColorNear(filled.GetPixel(20, 20), gray);
        ExpectColorNear(filled.GetPixel(60, 40), gray);
        const RenderedImage again = RenderFrame(64.0f, 64.0f, 1.0f, Color::White(), small);
        ExpectColorNear(again.GetPixel(4, 4), gray);
        ExpectColorNear(again.GetPixel(20, 20), Color::White());
    }

    TEST_P(RendererTests, AFrameRenderedAgainLooksTheSameAndANewFrameReplacesIt)
    {
        // A backend uploads a frame's geometry once. Rendering the frame again, as a host does that redraws a
        // window, must draw from what is there; the next frame must not.
        const Color red = Color::FromHex(0xFF3B30);
        const Color blue = Color::FromHex(0x0A84FF);
        const TextSpec spec = GetTextSpec(TextStyle::Title1);
        const RenderedImage first = RenderFrame(96.0f, 48.0f, 1.0f, Color::White(),
                                                [&](DrawList& drawList)
                                                {
                                                    drawList.AddSquircle(Rect(8.0f, 8.0f, 30.0f, 30.0f), red, 8.0f);
                                                    drawList.AddText(Vec2(44.0f, 10.0f), "Aa", spec, Color::Black());
                                                });
        ExpectColorNear(first.GetPixel(22, 22), red);

        for (int i = 0; i < 2; i++)
        {
            const RenderedImage again = m_Harness->RenderFrame(96, 48, Color::White());
            ASSERT_EQ(again.Pixels.size(), first.Pixels.size());
            EXPECT_EQ(again.Pixels, first.Pixels) << "redraw " << i;
        }

        const RenderedImage next = RenderFrame(96.0f, 48.0f, 1.0f, Color::White(), [&](DrawList& drawList)
                                               { drawList.AddRect(Rect(50.0f, 8.0f, 30.0f, 30.0f), blue); });
        ExpectColorNear(next.GetPixel(22, 22), Color::White());
        ExpectColorNear(next.GetPixel(64, 22), blue);
        EXPECT_EQ(m_Harness->RenderFrame(96, 48, Color::White()).Pixels, next.Pixels);
    }

    TEST_P(RendererTests, EmptyFrameLeavesTheTargetUntouched)
    {
        const Color background = Color::FromHex(0x123456);
        const RenderedImage image = RenderFrame(16.0f, 16.0f, 1.0f, background, [](DrawList&) {});
        ExpectColorNear(image.GetPixel(8, 8), background);
    }

    CB_INSTANTIATE_BACKEND_TESTS(RendererTests);
} // namespace Carbon
