#include "Support/ContextTest.h"

namespace Carbon
{
    namespace
    {
        const Rect Display(0.0f, 0.0f, 400.0f, 300.0f);

        Rect GetBounds(std::span<const DrawVertex> vertices, size_t firstVertex)
        {
            Vec2 min = vertices[firstVertex].Position;
            Vec2 max = min;
            for (size_t i = firstVertex; i < firstVertex + 4; i++)
            {
                min = Min(min, vertices[i].Position);
                max = Max(max, vertices[i].Position);
            }
            return Rect::FromMinMax(min, max);
        }
    } // namespace

    TEST(DrawListTests, RectEmitsOnePaddedQuad)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        drawList.AddRect(Rect(10.0f, 20.0f, 100.0f, 50.0f), Color::FromHex(0x336699));

        const std::span<const DrawVertex> vertices = drawList.GetVertices();
        const std::span<const DrawIndex> indices = drawList.GetIndices(DrawLayer::Content);
        ASSERT_EQ(vertices.size(), 4u);
        ASSERT_EQ(indices.size(), 6u);

        // The quad is one point larger on each side to leave room for the antialiased edge.
        EXPECT_EQ(GetBounds(vertices, 0), Rect(9.0f, 19.0f, 102.0f, 52.0f));
        // Local coordinates are relative to the shape's center.
        EXPECT_EQ(vertices[0].Local, Vec2(-51.0f, -26.0f));
        EXPECT_EQ(vertices[2].Local, Vec2(51.0f, 26.0f));
        EXPECT_EQ(vertices[0].Color, Color::FromHex(0x336699).ToRGBA8());

        const std::span<const DrawPrimitive> primitives = drawList.GetPrimitives();
        ASSERT_EQ(primitives.size(), 1u);
        EXPECT_EQ(primitives[0].Kind, DrawPrimitiveKind::Squircle);
        EXPECT_EQ(primitives[0].HalfSize, Vec2(50.0f, 25.0f));
        EXPECT_FLOAT_EQ(primitives[0].Radius, 0.0f);

        const DrawIndex expected[6] = {0, 1, 2, 0, 2, 3};
        for (size_t i = 0; i < 6; i++)
            EXPECT_EQ(indices[i], expected[i]);
    }

    TEST(DrawListTests, PaddingIsAtLeastOnePixel)
    {
        DrawList drawList;
        drawList.Reset(Display, 0.5f); // one pixel is two points wide
        drawList.AddRect(Rect(10.0f, 10.0f, 20.0f, 20.0f), Color::White());
        EXPECT_EQ(GetBounds(drawList.GetVertices(), 0), Rect(8.0f, 8.0f, 24.0f, 24.0f));
    }

    TEST(DrawListTests, SquircleRadiusClampsToHalfTheShortestSide)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        drawList.AddSquircle(Rect(0.0f, 0.0f, 100.0f, 20.0f), Color::White(), 500.0f, 0.6f);
        drawList.AddSquircle(Rect(0.0f, 0.0f, 100.0f, 20.0f), Color::White(), -3.0f, 4.0f);

        const std::span<const DrawPrimitive> primitives = drawList.GetPrimitives();
        ASSERT_EQ(primitives.size(), 2u);
        EXPECT_FLOAT_EQ(primitives[0].Radius, 10.0f);
        EXPECT_FLOAT_EQ(primitives[0].Smoothing, 0.6f);
        EXPECT_FLOAT_EQ(primitives[1].Radius, 0.0f);
        EXPECT_FLOAT_EQ(primitives[1].Smoothing, 1.0f);
    }

    TEST(DrawListTests, IdenticalShapesShareAPrimitive)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        for (int row = 0; row < 5; row++)
            drawList.AddSquircle(Rect(0.0f, float(row) * 30.0f, 200.0f, 24.0f), Color::White(), 6.0f);

        EXPECT_EQ(drawList.GetPrimitives().size(), 1u);
        EXPECT_EQ(drawList.GetVertices().size(), 20u);
        EXPECT_EQ(drawList.GetCommands(DrawLayer::Content).size(), 1u);
    }

    TEST(DrawListTests, StrokeAndFocusRing)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        const Rect control(100.0f, 100.0f, 80.0f, 24.0f);
        drawList.AddSquircleStroke(control, Color::Black(), 6.0f, 1.0f);
        drawList.AddFocusRing(control, Color::FromHex(0x007AFF), 6.0f, 3.0f, 1.0f);

        const std::span<const DrawPrimitive> primitives = drawList.GetPrimitives();
        ASSERT_EQ(primitives.size(), 2u);
        EXPECT_EQ(primitives[0].Kind, DrawPrimitiveKind::SquircleStroke);
        EXPECT_FLOAT_EQ(primitives[0].StrokeWidth, 1.0f);

        // The ring's outer edge is offset + width outside the control and its corner stays concentric.
        EXPECT_EQ(primitives[1].Kind, DrawPrimitiveKind::SquircleStroke);
        EXPECT_EQ(primitives[1].HalfSize, Vec2(44.0f, 16.0f));
        EXPECT_FLOAT_EQ(primitives[1].Radius, 10.0f);
        EXPECT_FLOAT_EQ(primitives[1].StrokeWidth, 3.0f);

        // A zero-width stroke draws nothing.
        drawList.AddSquircleStroke(control, Color::Black(), 6.0f, 0.0f);
        EXPECT_EQ(drawList.GetVertices().size(), 8u);
    }

    TEST(DrawListTests, EmptyAndInvertedRectanglesDrawNothing)
    {
        // Layout can hand out rectangles without area, or with a negative size while measurements are missing.
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        const Rect inverted(50.0f, 50.0f, -4.0f, -20.0f);
        const Rect flat(50.0f, 50.0f, 30.0f, 0.0f);
        for (const Rect& rect : {inverted, flat})
        {
            drawList.AddRect(rect, Color::Black());
            drawList.AddSquircle(rect, Color::Black(), 8.0f);
            drawList.AddSquircleStroke(rect, Color::Black(), 8.0f, 1.0f);
            drawList.AddShadow(rect, Color::Black(), 8.0f, 4.0f);
            drawList.AddImage(TextureID{3}, rect);
            drawList.ResolveDeferredSquircle(drawList.AddDeferredSquircle(Color::Black()), rect, 8.0f);
        }
        // Only the two deferred shapes exist, and they stay degenerate.
        ASSERT_EQ(drawList.GetVertices().size(), 8u);
        for (const DrawVertex& vertex : drawList.GetVertices())
            EXPECT_EQ(vertex.Position, Vec2(0.0f, 0.0f));
    }

    TEST(DrawListTests, CircleIsASquircleWithoutSmoothing)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        drawList.AddCircle(Vec2(50.0f, 50.0f), 10.0f, Color::White());
        const DrawPrimitive& primitive = drawList.GetPrimitives()[0];
        EXPECT_EQ(primitive.HalfSize, Vec2(10.0f, 10.0f));
        EXPECT_FLOAT_EQ(primitive.Radius, 10.0f);
        EXPECT_FLOAT_EQ(primitive.Smoothing, 0.0f);
        EXPECT_EQ(GetBounds(drawList.GetVertices(), 0), Rect(39.0f, 39.0f, 22.0f, 22.0f));
    }

    TEST(DrawListTests, LineIsARotatedPill)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        // A vertical line, 2 points wide, from (50, 10) to (50, 110).
        drawList.AddLine(Vec2(50.0f, 10.0f), Vec2(50.0f, 110.0f), Color::White(), 2.0f);

        const DrawPrimitive& primitive = drawList.GetPrimitives()[0];
        // In the line's own frame it is 100 long plus round caps, and 2 wide.
        EXPECT_EQ(primitive.HalfSize, Vec2(51.0f, 1.0f));
        EXPECT_FLOAT_EQ(primitive.Radius, 1.0f);

        const Rect bounds = GetBounds(drawList.GetVertices(), 0);
        EXPECT_NEAR(bounds.X, 48.0f, 1e-4f);
        EXPECT_NEAR(bounds.Width, 4.0f, 1e-4f);
        EXPECT_NEAR(bounds.Y, 8.0f, 1e-4f);
        EXPECT_NEAR(bounds.Height, 104.0f, 1e-4f);

        // Flat caps do not extend past the end points.
        drawList.AddLine(Vec2(0.0f, 50.0f), Vec2(100.0f, 50.0f), Color::White(), 2.0f, false);
        EXPECT_EQ(drawList.GetPrimitives()[1].HalfSize, Vec2(50.0f, 1.0f));
        EXPECT_FLOAT_EQ(drawList.GetPrimitives()[1].Radius, 0.0f);
    }

    TEST(DrawListTests, ShadowQuadCoversTheBlur)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        drawList.AddShadow(Rect(100.0f, 100.0f, 50.0f, 50.0f), Color::Black().WithAlpha(0.3f), 8.0f, 12.0f,
                           Vec2(0.0f, 4.0f));
        const DrawPrimitive& primitive = drawList.GetPrimitives()[0];
        EXPECT_EQ(primitive.Kind, DrawPrimitiveKind::Shadow);
        EXPECT_FLOAT_EQ(primitive.Softness, 12.0f);
        EXPECT_EQ(GetBounds(drawList.GetVertices(), 0), Rect(87.0f, 91.0f, 76.0f, 76.0f));
    }

    TEST(DrawListTests, ShapesOutsideTheClipRectAreDropped)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        drawList.AddRect(Rect(500.0f, 10.0f, 20.0f, 20.0f), Color::White()); // right of the display
        EXPECT_TRUE(drawList.GetVertices().empty());

        drawList.PushClipRect(Rect(0.0f, 0.0f, 100.0f, 100.0f));
        drawList.AddRect(Rect(150.0f, 10.0f, 20.0f, 20.0f), Color::White()); // outside the clip
        drawList.AddRect(Rect(90.0f, 10.0f, 20.0f, 20.0f), Color::White());  // straddles the edge: kept
        drawList.PopClipRect();
        EXPECT_EQ(drawList.GetVertices().size(), 4u);

        // Fully transparent and empty shapes are dropped too.
        drawList.AddRect(Rect(10.0f, 10.0f, 20.0f, 20.0f), Color::Transparent());
        drawList.AddRect(Rect(10.0f, 10.0f, 0.0f, 20.0f), Color::White());
        EXPECT_EQ(drawList.GetVertices().size(), 4u);
    }

    TEST(DrawListTests, ClipRectsNestByIntersection)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        EXPECT_EQ(drawList.GetClipRect(), Display);

        drawList.PushClipRect(Rect(50.0f, 50.0f, 200.0f, 200.0f));
        drawList.PushClipRect(Rect(0.0f, 100.0f, 100.0f, 500.0f));
        EXPECT_EQ(drawList.GetClipRect(), Rect(50.0f, 100.0f, 50.0f, 150.0f));
        drawList.PopClipRect();

        drawList.PushClipRect(Rect(0.0f, 0.0f, 10.0f, 10.0f), false);
        EXPECT_EQ(drawList.GetClipRect(), Rect(0.0f, 0.0f, 10.0f, 10.0f));
        drawList.PopClipRect();

        EXPECT_FALSE(drawList.IsBalanced());
        drawList.PopClipRect();
        EXPECT_TRUE(drawList.IsBalanced());
    }

    TEST(DrawListTests, CommandsSplitOnClipChange)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        drawList.AddRect(Rect(0.0f, 0.0f, 10.0f, 10.0f), Color::White());
        drawList.AddRect(Rect(20.0f, 0.0f, 10.0f, 10.0f), Color::White());

        const Rect clip(0.0f, 0.0f, 100.0f, 100.0f);
        drawList.PushClipRect(clip);
        drawList.AddRect(Rect(40.0f, 0.0f, 10.0f, 10.0f), Color::White());
        drawList.PopClipRect();
        drawList.AddRect(Rect(60.0f, 0.0f, 10.0f, 10.0f), Color::White());

        const std::span<const DrawCommand> commands = drawList.GetCommands(DrawLayer::Content);
        ASSERT_EQ(commands.size(), 3u);
        EXPECT_EQ(commands[0].ClipRect, Display);
        EXPECT_EQ(commands[0].IndexOffset, 0u);
        EXPECT_EQ(commands[0].IndexCount, 12u);
        EXPECT_EQ(commands[1].ClipRect, clip);
        EXPECT_EQ(commands[1].IndexOffset, 12u);
        EXPECT_EQ(commands[1].IndexCount, 6u);
        EXPECT_EQ(commands[2].ClipRect, Display);
        EXPECT_EQ(commands[2].IndexOffset, 18u);
    }

    TEST(DrawListTests, OnlyTextureChangesSplitCommands)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        const Rect unit(0.0f, 0.0f, 1.0f, 1.0f);
        const TextureID photo{42};
        const TextureID other{43};

        // Shapes and glyphs share the atlas command; untextured shapes also ride along with an image.
        drawList.AddRect(Rect(0.0f, 0.0f, 10.0f, 10.0f), Color::White());
        drawList.AddGlyph(Rect(0.0f, 0.0f, 8.0f, 8.0f), unit, Color::White());
        drawList.AddRect(Rect(0.0f, 0.0f, 10.0f, 10.0f), Color::White());
        drawList.AddImage(photo, Rect(0.0f, 0.0f, 50.0f, 50.0f));
        drawList.AddRect(Rect(0.0f, 0.0f, 10.0f, 10.0f), Color::White());
        drawList.AddImage(photo, Rect(60.0f, 0.0f, 50.0f, 50.0f));
        drawList.AddImage(other, Rect(120.0f, 0.0f, 50.0f, 50.0f));
        drawList.AddGlyph(Rect(0.0f, 0.0f, 8.0f, 8.0f), unit, Color::White());

        const std::span<const DrawCommand> commands = drawList.GetCommands(DrawLayer::Content);
        ASSERT_EQ(commands.size(), 4u);
        EXPECT_EQ(commands[0].Texture, TextureID());
        EXPECT_EQ(commands[0].IndexCount, 18u);
        EXPECT_EQ(commands[1].Texture, photo);
        EXPECT_EQ(commands[1].IndexCount, 18u);
        EXPECT_EQ(commands[2].Texture, other);
        EXPECT_EQ(commands[3].Texture, TextureID());
    }

    TEST(DrawListTests, GlyphsShareOnePrimitiveAndAreNotPadded)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        const Rect uv(0.25f, 0.5f, 0.125f, 0.25f);
        drawList.AddGlyph(Rect(10.0f, 10.0f, 8.0f, 12.0f), uv, Color::Black());
        drawList.AddRect(Rect(0.0f, 0.0f, 5.0f, 5.0f), Color::White());
        drawList.AddGlyph(Rect(20.0f, 10.0f, 8.0f, 12.0f), uv, Color::Black());

        const std::span<const DrawVertex> vertices = drawList.GetVertices();
        ASSERT_EQ(vertices.size(), 12u);
        EXPECT_EQ(GetBounds(vertices, 0), Rect(10.0f, 10.0f, 8.0f, 12.0f));
        EXPECT_EQ(vertices[0].UV, Vec2(0.25f, 0.5f));
        EXPECT_EQ(vertices[2].UV, Vec2(0.375f, 0.75f));
        EXPECT_EQ(vertices[0].Primitive, vertices[8].Primitive);
        EXPECT_EQ(drawList.GetPrimitives()[vertices[0].Primitive].Kind, DrawPrimitiveKind::Glyph);
    }

    TEST(DrawListTests, ImageUVsExtrapolateIntoThePadding)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        drawList.AddImage(TextureID{7}, Rect(10.0f, 10.0f, 100.0f, 50.0f));
        const std::span<const DrawVertex> vertices = drawList.GetVertices();
        // One point of padding is 1/100 of the width and 1/50 of the height in UV space.
        EXPECT_FLOAT_EQ(vertices[0].UV.X, -0.01f);
        EXPECT_FLOAT_EQ(vertices[0].UV.Y, -0.02f);
        EXPECT_FLOAT_EQ(vertices[2].UV.X, 1.01f);
        EXPECT_FLOAT_EQ(vertices[2].UV.Y, 1.02f);
        EXPECT_EQ(drawList.GetPrimitives()[0].Kind, DrawPrimitiveKind::Image);
    }

    TEST(DrawListTests, OpacityMultipliesAlphaAndNests)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        drawList.PushOpacity(0.5f);
        drawList.PushOpacity(0.5f);
        EXPECT_FLOAT_EQ(drawList.GetOpacity(), 0.25f);
        drawList.AddRect(Rect(0.0f, 0.0f, 10.0f, 10.0f), Color::White());
        drawList.PopOpacity();
        drawList.PopOpacity();
        EXPECT_EQ(drawList.GetVertices()[0].Color, Color(1.0f, 1.0f, 1.0f, 0.25f).ToRGBA8());

        // Zero opacity drops the shape entirely.
        drawList.PushOpacity(0.0f);
        drawList.AddRect(Rect(0.0f, 0.0f, 10.0f, 10.0f), Color::White());
        drawList.PopOpacity();
        EXPECT_EQ(drawList.GetVertices().size(), 4u);
    }

    TEST(DrawListTests, LayersMergeBackToFront)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);

        // Submitted out of order: overlay first, then content, then background.
        drawList.PushLayer(DrawLayer::Overlay);
        drawList.AddRect(Rect(0.0f, 0.0f, 30.0f, 30.0f), Color::White());
        drawList.PopLayer();
        drawList.AddRect(Rect(0.0f, 0.0f, 20.0f, 20.0f), Color::White());
        drawList.PushLayer(DrawLayer::Background);
        drawList.AddRect(Rect(0.0f, 0.0f, 10.0f, 10.0f), Color::White());
        drawList.PopLayer();
        EXPECT_EQ(drawList.GetLayer(), DrawLayer::Content);

        const DrawData& drawData = drawList.Finalize();
        ASSERT_EQ(drawData.Commands.size(), 3u);
        ASSERT_EQ(drawData.Indices.size(), 18u);

        // The first command draws the background quad (vertices 8..11), the last one the overlay (0..3).
        EXPECT_EQ(drawData.Commands[0].IndexOffset, 0u);
        EXPECT_EQ(drawData.Indices[drawData.Commands[0].IndexOffset], 8u);
        EXPECT_EQ(drawData.Indices[drawData.Commands[1].IndexOffset], 4u);
        EXPECT_EQ(drawData.Indices[drawData.Commands[2].IndexOffset], 0u);
        EXPECT_EQ(drawData.Vertices.size(), 12u);
    }

    TEST(DrawListTests, LayersHaveIndependentClipStacks)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        drawList.PushClipRect(Rect(0.0f, 0.0f, 50.0f, 50.0f));

        // A popover drawn from inside a clipped scroll view is not clipped by it.
        drawList.PushLayer(DrawLayer::Overlay);
        EXPECT_EQ(drawList.GetClipRect(), Display);
        drawList.AddRect(Rect(200.0f, 200.0f, 20.0f, 20.0f), Color::White());
        drawList.PopLayer();

        EXPECT_EQ(drawList.GetClipRect(), Rect(0.0f, 0.0f, 50.0f, 50.0f));
        drawList.PopClipRect();
        EXPECT_EQ(drawList.GetVertices().size(), 4u);
    }

    TEST(DrawListTests, ResetClearsEverything)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        drawList.PushClipRect(Rect(0.0f, 0.0f, 50.0f, 50.0f));
        drawList.PushLayer(DrawLayer::Tooltip);
        drawList.AddRect(Rect(0.0f, 0.0f, 10.0f, 10.0f), Color::White());
        drawList.Finalize();

        drawList.Reset(Rect(0.0f, 0.0f, 100.0f, 100.0f), 2.0f);
        EXPECT_TRUE(drawList.GetVertices().empty());
        EXPECT_TRUE(drawList.GetPrimitives().empty());
        EXPECT_TRUE(drawList.IsBalanced());
        EXPECT_EQ(drawList.GetClipRect(), Rect(0.0f, 0.0f, 100.0f, 100.0f));
        const DrawData& drawData = drawList.Finalize();
        EXPECT_TRUE(drawData.Commands.empty());
        EXPECT_FLOAT_EQ(drawData.ContentScale, 2.0f);
    }

    using DrawListFrameTests = ContextTest;

    TEST_F(DrawListFrameTests, UnbalancedClipRectIsReportedAtEndFrame)
    {
        NewFrame();
        GetDrawList().PushClipRect(Rect(0.0f, 0.0f, 10.0f, 10.0f));
        EndFrame();
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("Unbalanced draw list"), std::string::npos);
    }

    TEST_F(DrawListFrameTests, PopWithoutPushIsReported)
    {
        DrawList drawList;
        drawList.Reset(Display, 1.0f);
        drawList.PopClipRect();
        drawList.PopLayer();
        drawList.PopOpacity();
        EXPECT_EQ(m_AssertMessages.size(), 3u);
        EXPECT_TRUE(drawList.IsBalanced());
    }
} // namespace Carbon
