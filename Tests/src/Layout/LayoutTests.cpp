#include "Support/ContextTest.h"

#include <cmath>

namespace Carbon
{
    // Layout is single-pass: sizes that depend on content are known one frame late. `Settle` runs enough frames
    // for that; tests that check first-frame behaviour say so explicitly.
    class LayoutTests : public ContextTest
    {
    protected:
        Rect Item(float width, float height, const ItemOptions& options = {})
        {
            return AllocateItem(Vec2(width, height), options);
        }
    };

    TEST_F(LayoutTests, VStackFlowsDownwardWithSpacing)
    {
        Rect a, b, c, stack;
        Frame(
            [&]
            {
                BeginVStack({.Spacing = 10.0f});
                a = Item(100.0f, 20.0f);
                b = Item(50.0f, 30.0f);
                c = Item(80.0f, 10.0f);
                EndVStack();
                stack = GetLastItemRect();
            });
        // Leading flow needs no measurements: correct in the very first frame.
        EXPECT_EQ(a, Rect(0.0f, 0.0f, 100.0f, 20.0f));
        EXPECT_EQ(b, Rect(0.0f, 30.0f, 50.0f, 30.0f));
        EXPECT_EQ(c, Rect(0.0f, 70.0f, 80.0f, 10.0f));
        // The stack fits its content, and reports that size to its parent in the same frame.
        EXPECT_EQ(stack, Rect(0.0f, 0.0f, 100.0f, 80.0f));
    }

    TEST_F(LayoutTests, HStackFlowsSidewaysWithoutSameLine)
    {
        Rect a, b, stack;
        Settle(
            [&]
            {
                BeginHStack({.Spacing = 6.0f, .Alignment = VerticalAlignment::Top});
                a = Item(40.0f, 20.0f);
                b = Item(60.0f, 30.0f);
                EndHStack();
                stack = GetLastItemRect();
            });
        EXPECT_EQ(a, Rect(0.0f, 0.0f, 40.0f, 20.0f));
        EXPECT_EQ(b, Rect(46.0f, 0.0f, 60.0f, 30.0f));
        EXPECT_EQ(stack, Rect(0.0f, 0.0f, 106.0f, 30.0f));
    }

    TEST_F(LayoutTests, DefaultSpacingComesFromTheTheme)
    {
        Rect a, b;
        Frame(
            [&]
            {
                BeginVStack();
                a = Item(10.0f, 10.0f);
                b = Item(10.0f, 10.0f);
                EndVStack();
            });
        EXPECT_FLOAT_EQ(b.Y - a.GetBottom(), GetStyleVar(StyleVar::Spacing));
    }

    TEST_F(LayoutTests, PaddingInsetsTheContent)
    {
        Rect item, uniform, perEdge;
        Frame(
            [&]
            {
                BeginVStack({.Padding = 20.0f});
                item = Item(100.0f, 40.0f);
                EndVStack();
                uniform = GetLastItemRect();

                BeginVStack({.Padding = EdgeInsets(1.0f, 2.0f, 3.0f, 4.0f)});
                Item(10.0f, 10.0f);
                EndVStack();
                perEdge = GetLastItemRect();
            });
        EXPECT_EQ(item, Rect(20.0f, 20.0f, 100.0f, 40.0f));
        EXPECT_EQ(uniform.GetSize(), Vec2(140.0f, 80.0f));
        EXPECT_EQ(perEdge.GetSize(), Vec2(14.0f, 16.0f));
    }

    TEST_F(LayoutTests, VStackAlignsItemsHorizontally)
    {
        Rect leading, center, trailing;
        Settle(
            [&]
            {
                BeginVStack({.Spacing = 0.0f, .Alignment = Alignment::Leading});
                Item(100.0f, 10.0f);
                leading = Item(40.0f, 10.0f);
                EndVStack();

                BeginVStack({.Spacing = 0.0f, .Alignment = Alignment::Center});
                Item(100.0f, 10.0f);
                center = Item(40.0f, 10.0f);
                EndVStack();

                BeginVStack({.Spacing = 0.0f, .Alignment = Alignment::Trailing});
                Item(100.0f, 10.0f);
                trailing = Item(40.0f, 10.0f);
                EndVStack();
            });
        EXPECT_FLOAT_EQ(leading.X, 0.0f);
        EXPECT_FLOAT_EQ(center.X, 30.0f);
        EXPECT_FLOAT_EQ(trailing.X, 60.0f);
    }

    TEST_F(LayoutTests, HStackAlignsItemsVertically)
    {
        Rect top, center, bottom;
        Settle(
            [&]
            {
                BeginHStack({.Alignment = VerticalAlignment::Top});
                Item(10.0f, 50.0f);
                top = Item(10.0f, 20.0f);
                EndHStack();

                BeginHStack(); // center is the default
                Item(10.0f, 50.0f);
                center = Item(10.0f, 20.0f);
                EndHStack();

                BeginHStack({.Alignment = VerticalAlignment::Bottom});
                Item(10.0f, 50.0f);
                bottom = Item(10.0f, 20.0f);
                EndHStack();
            });
        const float spacing = GetStyleVar(StyleVar::Spacing);
        EXPECT_FLOAT_EQ(top.Y, 0.0f);
        EXPECT_FLOAT_EQ(center.Y, 50.0f + spacing + 15.0f);
        EXPECT_FLOAT_EQ(bottom.Y, 2.0f * (50.0f + spacing) + 30.0f);
    }

    TEST_F(LayoutTests, AlignmentInAFixedSizeStackIsRightOnTheFirstFrame)
    {
        Rect item;
        Frame(
            [&]
            {
                BeginVStack({.Alignment = Alignment::Center, .Width = 200.0f});
                item = Item(50.0f, 10.0f);
                EndVStack();
            });
        EXPECT_FLOAT_EQ(item.X, 75.0f);
    }

    TEST_F(LayoutTests, SpacerPushesFollowingItemsToTheFarEnd)
    {
        Rect cancel, save;
        Settle(
            [&]
            {
                BeginHStack({.Spacing = 8.0f, .Width = 300.0f});
                Spacer();
                cancel = Item(70.0f, 24.0f);
                save = Item(50.0f, 24.0f);
                EndHStack();
            });
        EXPECT_FLOAT_EQ(save.GetRight(), 300.0f);
        EXPECT_FLOAT_EQ(cancel.GetRight(), save.X - 8.0f);
        EXPECT_FLOAT_EQ(cancel.X, 300.0f - 50.0f - 8.0f - 70.0f);
    }

    TEST_F(LayoutTests, SpacersShareFreeSpaceByWeight)
    {
        Rect a, b, c;
        Settle(
            [&]
            {
                BeginHStack({.Spacing = 0.0f, .Width = 400.0f});
                a = Item(40.0f, 10.0f);
                Spacer();
                b = Item(40.0f, 10.0f);
                Spacer({.Weight = 3.0f});
                c = Item(40.0f, 10.0f);
                EndHStack();
            });
        // 400 - 120 = 280 points of free space, split 1 : 3.
        EXPECT_FLOAT_EQ(b.X, 40.0f + 70.0f);
        EXPECT_FLOAT_EQ(c.X, 400.0f - 40.0f);
        EXPECT_FLOAT_EQ(c.X - b.GetRight(), 210.0f);
    }

    TEST_F(LayoutTests, SpacerMinimumAndFixedLength)
    {
        Rect a, b, c, stack;
        Settle(
            [&]
            {
                // A stack that fits its content has no free space: flexible spacers collapse to their minimum.
                BeginHStack({.Spacing = 0.0f});
                a = Item(10.0f, 10.0f);
                Spacer({.MinLength = 12.0f});
                b = Item(10.0f, 10.0f);
                Spacer({.Length = 30.0f});
                c = Item(10.0f, 10.0f);
                Spacer();
                EndHStack();
                stack = GetLastItemRect();
            });
        EXPECT_FLOAT_EQ(b.X, 22.0f);
        EXPECT_FLOAT_EQ(c.X, 62.0f);
        EXPECT_FLOAT_EQ(stack.Width, 72.0f);
    }

    TEST_F(LayoutTests, FillItemsShareTheFreeSpace)
    {
        Rect one, two, fixed;
        Settle(
            [&]
            {
                BeginHStack({.Spacing = 10.0f, .Width = 320.0f});
                one = Item(0.0f, 20.0f, {.Width = Size::Fill()});
                two = Item(0.0f, 20.0f, {.Width = Size::Fill(2.0f)});
                fixed = Item(60.0f, 20.0f);
                EndHStack();
            });
        // 320 - 60 - 2 * 10 = 240 points, split 1 : 2.
        EXPECT_FLOAT_EQ(one.Width, 80.0f);
        EXPECT_FLOAT_EQ(two.Width, 160.0f);
        EXPECT_FLOAT_EQ(two.X, 90.0f);
        EXPECT_FLOAT_EQ(fixed.X, 260.0f);
        EXPECT_FLOAT_EQ(fixed.GetRight(), 320.0f);
    }

    TEST_F(LayoutTests, FillAcrossTheAxisTakesTheFullExtent)
    {
        Rect row, fixedOverride;
        Settle(
            [&]
            {
                BeginVStack({.Padding = 10.0f, .Width = 220.0f});
                row = Item(30.0f, 20.0f, {.Width = Size::Fill()});
                fixedOverride = Item(30.0f, 20.0f, {.Width = 120.0f, .Height = 44.0f});
                EndVStack();
            });
        EXPECT_EQ(row, Rect(10.0f, 10.0f, 200.0f, 20.0f));
        EXPECT_EQ(fixedOverride.GetSize(), Vec2(120.0f, 44.0f));
    }

    TEST_F(LayoutTests, FillItemsDoNotInflateAStackThatFitsItsContent)
    {
        // A full-width separator in a fitting stack takes the width of the widest real item and never feeds
        // back into it.
        Rect separator, stack;
        Settle(
            [&]
            {
                BeginVStack({.Spacing = 0.0f});
                Item(140.0f, 20.0f);
                separator = Item(0.0f, 1.0f, {.Width = Size::Fill()});
                Item(90.0f, 20.0f);
                EndVStack();
                stack = GetLastItemRect();
            });
        EXPECT_FLOAT_EQ(separator.Width, 140.0f);
        EXPECT_FLOAT_EQ(stack.Width, 140.0f);
    }

    TEST_F(LayoutTests, NestedStacks)
    {
        Rect title, toggle, cancel, save, buttons, outer;
        Settle(
            [&]
            {
                BeginVStack({.Spacing = 12.0f, .Padding = 20.0f, .Width = 360.0f});
                title = Item(120.0f, 32.0f);
                toggle = Item(160.0f, 22.0f);
                BeginHStack({.Spacing = 8.0f, .Width = Size::Fill()});
                Spacer();
                cancel = Item(70.0f, 24.0f);
                save = Item(50.0f, 24.0f);
                EndHStack();
                buttons = GetLastItemRect();
                EndVStack();
                outer = GetLastItemRect();
            });
        EXPECT_EQ(title, Rect(20.0f, 20.0f, 120.0f, 32.0f));
        EXPECT_EQ(toggle, Rect(20.0f, 64.0f, 160.0f, 22.0f));
        // The button row spans the content width; its buttons sit at the trailing edge.
        EXPECT_EQ(buttons, Rect(20.0f, 98.0f, 320.0f, 24.0f));
        EXPECT_FLOAT_EQ(save.GetRight(), 340.0f);
        EXPECT_FLOAT_EQ(cancel.GetRight(), save.X - 8.0f);
        EXPECT_FLOAT_EQ(cancel.Y, 98.0f);
        EXPECT_EQ(outer, Rect(0.0f, 0.0f, 360.0f, 142.0f));
    }

    TEST_F(LayoutTests, JustifyPlacesContentAlongTheAxis)
    {
        Rect top, center, bottom, trailing;
        Settle(
            [&]
            {
                BeginHStack({.Spacing = 0.0f, .Alignment = VerticalAlignment::Top});
                BeginVStack({.Justify = VerticalAlignment::Top, .Height = 200.0f});
                top = Item(10.0f, 50.0f);
                EndVStack();
                BeginVStack({.Justify = VerticalAlignment::Center, .Height = 200.0f});
                center = Item(10.0f, 50.0f);
                EndVStack();
                BeginVStack({.Justify = VerticalAlignment::Bottom, .Height = 200.0f});
                bottom = Item(10.0f, 50.0f);
                EndVStack();
                EndHStack();

                BeginHStack({.Justify = Alignment::Trailing, .Width = 300.0f});
                trailing = Item(40.0f, 10.0f);
                EndHStack();
            });
        EXPECT_FLOAT_EQ(top.Y, 0.0f);
        EXPECT_FLOAT_EQ(center.Y, 75.0f);
        EXPECT_FLOAT_EQ(bottom.Y, 150.0f);
        EXPECT_FLOAT_EQ(trailing.GetRight(), 300.0f);
    }

    TEST_F(LayoutTests, RootFlowsLikeAVerticalStack)
    {
        GetIO().SetDisplaySize(400.0f, 300.0f);
        Rect a, b, footer, content;
        Settle(
            [&]
            {
                content = GetContentRect();
                a = Item(100.0f, 20.0f);
                b = Item(100.0f, 20.0f);
                Spacer();
                footer = Item(100.0f, 30.0f);
            });
        const float spacing = GetStyleVar(StyleVar::Spacing);
        EXPECT_EQ(a.GetMin(), Vec2(0.0f, 0.0f));
        EXPECT_FLOAT_EQ(b.Y, 20.0f + spacing);
        EXPECT_FLOAT_EQ(footer.GetBottom(), 300.0f);
        EXPECT_EQ(content, Rect(0.0f, 0.0f, 400.0f, 300.0f));

        // Layout functions are only valid inside a frame; outside, the misuse is reported.
        GetContentRect();
        EXPECT_EQ(m_AssertMessages.size(), 1u);
        m_AssertMessages.clear();
    }

    TEST_F(LayoutTests, FillStackTakesTheRemainingDisplay)
    {
        GetIO().SetDisplaySize(500.0f, 400.0f);
        Rect header, body, content;
        Settle(
            [&]
            {
                BeginVStack({.Spacing = 0.0f, .Width = Size::Fill(), .Height = Size::Fill()});
                header = Item(100.0f, 40.0f);
                BeginVStack({.Padding = 10.0f, .Width = Size::Fill(), .Height = Size::Fill()});
                content = GetContentRect();
                Item(10.0f, 10.0f);
                EndVStack();
                body = GetLastItemRect();
                EndVStack();
            });
        EXPECT_EQ(body, Rect(0.0f, 40.0f, 500.0f, 360.0f));
        EXPECT_EQ(content, Rect(10.0f, 50.0f, 480.0f, 340.0f));
    }

    TEST_F(LayoutTests, CursorIsAnEscapeHatchForAbsolutePlacement)
    {
        Rect first, placed, next;
        Vec2 cursorBefore, cursorAfter;
        Frame(
            [&]
            {
                BeginVStack({.Spacing = 5.0f, .Padding = 10.0f});
                first = Item(50.0f, 20.0f);
                cursorBefore = GetCursorPos();
                SetCursorPos(Vec2(200.0f, 120.0f));
                placed = Item(30.0f, 30.0f);
                cursorAfter = GetCursorPos();
                next = Item(50.0f, 20.0f);
                EndVStack();
            });
        EXPECT_EQ(cursorBefore, Vec2(10.0f, 35.0f));
        EXPECT_EQ(placed, Rect(200.0f, 120.0f, 30.0f, 30.0f));
        // The flow continues below the placed item, back at the stack's leading edge.
        EXPECT_EQ(cursorAfter, Vec2(10.0f, 155.0f));
        EXPECT_EQ(next.GetMin(), Vec2(10.0f, 155.0f));
    }

    TEST_F(LayoutTests, ItemOriginsSnapToWholePixels)
    {
        GetIO().SetContentScale(1.5f);
        Rect centered, second;
        Settle(
            [&]
            {
                BeginVStack({.Spacing = 3.3f, .Alignment = Alignment::Center, .Width = 201.0f});
                centered = Item(50.0f, 10.1f);
                second = Item(20.0f, 10.0f);
                EndVStack();
            });
        for (const Rect& rect : {centered, second})
        {
            EXPECT_NEAR(rect.X * 1.5f, std::round(rect.X * 1.5f), 1e-3f);
            EXPECT_NEAR(rect.Y * 1.5f, std::round(rect.Y * 1.5f), 1e-3f);
        }
        // Snapping moves an item by at most half a pixel.
        EXPECT_NEAR(centered.X, 75.5f, 0.34f);
    }

    TEST_F(LayoutTests, StacksFromOneCallSiteInALoopAreDistinct)
    {
        Rect narrow[3];
        Settle(
            [&]
            {
                for (int i = 0; i < 3; i++)
                {
                    // Same source location three times, different content each time.
                    BeginVStack({.Spacing = 0.0f, .Alignment = Alignment::Center});
                    Item(100.0f + float(i) * 60.0f, 10.0f);
                    narrow[i] = Item(20.0f, 10.0f);
                    EndVStack();
                }
            });
        // Each stack centers against its own measured width.
        EXPECT_FLOAT_EQ(narrow[0].X, 40.0f);
        EXPECT_FLOAT_EQ(narrow[1].X, 70.0f);
        EXPECT_FLOAT_EQ(narrow[2].X, 100.0f);
    }

    TEST_F(LayoutTests, NewContainersAreHiddenForOneFrameThenFadeIn)
    {
        const auto build = [&]
        {
            BeginVStack({.Alignment = Alignment::Center});
            GetDrawList().AddRect(AllocateItem(Vec2(40.0f, 40.0f)), Color::Black());
            AllocateItem(Vec2(120.0f, 10.0f));
            EndVStack();
        };
        const auto getAlpha = [&]() -> int
        {
            const DrawData& drawData = GetDrawData();
            return drawData.Vertices.empty() ? 0 : int(drawData.Vertices[0].Color >> 24);
        };

        // Frame 1: measurements are missing, so the centered item would be misplaced. Nothing is drawn.
        Frame(build);
        EXPECT_TRUE(GetDrawData().Vertices.empty());
        EXPECT_TRUE(IsAnimating());

        // Frame 2 onwards: in place, fading in.
        Frame(build);
        const int early = getAlpha();
        EXPECT_GT(early, 0);
        EXPECT_LT(early, 255);
        EXPECT_FLOAT_EQ(GetDrawData().Vertices[0].Position.X, 40.0f - 1.0f); // centered, plus the quad's padding

        Frame(build, 0.05f);
        const int later = getAlpha();
        EXPECT_GT(later, early);

        Settle(build);
        EXPECT_EQ(getAlpha(), 255);
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(LayoutTests, AConditionalSiblingDoesNotDisturbOtherStacks)
    {
        bool showBanner = false;
        const auto build = [&]
        {
            if (showBanner)
            {
                BeginHStack();
                AllocateItem(Vec2(200.0f, 30.0f));
                EndHStack();
            }
            BeginVStack();
            GetDrawList().AddRect(AllocateItem(Vec2(40.0f, 40.0f)), Color::Black());
            EndVStack();
        };
        Settle(build);

        // The banner appears above. The stack below keeps its identity (it is identified by its call site, not
        // by its position), so it stays fully visible instead of fading in again.
        showBanner = true;
        Frame(build);
        const DrawData& drawData = GetDrawData();
        ASSERT_EQ(drawData.Vertices.size(), 4u);
        EXPECT_EQ(drawData.Vertices[0].Color >> 24, 255u);
        EXPECT_FLOAT_EQ(drawData.Vertices[0].Position.Y, 30.0f + GetStyleVar(StyleVar::Spacing) - 1.0f);
    }

    TEST_F(LayoutTests, ExplicitIDsKeepIdentityWhenOrderChanges)
    {
        bool swapped = false;
        Rect wideItem;
        const auto row = [&](std::string_view id, float width)
        {
            BeginVStack({.Spacing = 0.0f, .Alignment = Alignment::Center, .ID = id});
            AllocateItem(Vec2(width, 10.0f));
            const Rect item = AllocateItem(Vec2(20.0f, 10.0f));
            EndVStack();
            return item;
        };
        const auto build = [&]
        {
            if (swapped)
            {
                wideItem = row("wide", 200.0f);
                row("narrow", 60.0f);
            }
            else
            {
                row("narrow", 60.0f);
                wideItem = row("wide", 200.0f);
            }
        };
        Settle(build);
        EXPECT_FLOAT_EQ(wideItem.X, 90.0f);
        // After the swap the measurements travel with the IDs: correct immediately, no settling frame.
        swapped = true;
        Frame(build);
        EXPECT_FLOAT_EQ(wideItem.X, 90.0f);
    }

    TEST_F(LayoutTests, BackgroundIsDrawnBehindTheContentAtTheFinalSize)
    {
        const Color fill = Color::FromHex(0xF2F2F7);
        Settle(
            [&]
            {
                BeginVStack({.Padding = 12.0f, .Background = fill, .CornerRadius = 9.0f});
                GetDrawList().AddRect(AllocateItem(Vec2(100.0f, 30.0f)), Color::Black());
                EndVStack();
            });
        const DrawData& drawData = GetDrawData();
        ASSERT_EQ(drawData.Vertices.size(), 8u);

        // First quad: the background, covering content plus padding (and one point of antialiasing margin).
        EXPECT_EQ(drawData.Vertices[0].Color, fill.ToRGBA8());
        EXPECT_EQ(drawData.Vertices[0].Position, Vec2(-1.0f, -1.0f));
        EXPECT_EQ(drawData.Vertices[2].Position, Vec2(125.0f, 55.0f));
        const DrawPrimitive& background = drawData.Primitives[drawData.Vertices[0].Primitive];
        EXPECT_EQ(background.HalfSize, Vec2(62.0f, 27.0f));
        EXPECT_FLOAT_EQ(background.Radius, 9.0f);
        EXPECT_FLOAT_EQ(background.Smoothing, GetStyleVar(StyleVar::CornerSmoothing));

        // Second quad: the content, drawn after (above) it.
        EXPECT_EQ(drawData.Vertices[4].Color, Color::Black().ToRGBA8());
    }

    TEST_F(LayoutTests, BackgroundFollowsContentThatChangesSizeWithoutLag)
    {
        float height = 30.0f;
        const auto build = [&]
        {
            BeginVStack({.Background = Color::Black()});
            AllocateItem(Vec2(100.0f, height));
            EndVStack();
        };
        Settle(build);
        height = 80.0f;
        Frame(build);
        // The same frame the content grew, the background is already 80 points tall.
        const DrawData& drawData = GetDrawData();
        EXPECT_FLOAT_EQ(drawData.Primitives[drawData.Vertices[0].Primitive].HalfSize.Y, 40.0f);
    }

    TEST_F(LayoutTests, UnbalancedContainersAreReported)
    {
        Frame([&] { BeginVStack(); });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("Unbalanced layout"), std::string::npos);

        m_AssertMessages.clear();
        Frame([&] { EndVStack(); });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("never begun"), std::string::npos);

        m_AssertMessages.clear();
        Frame(
            [&]
            {
                BeginVStack();
                EndHStack();
            });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("Mismatched container"), std::string::npos);

        // The frame after a mistake is clean again.
        m_AssertMessages.clear();
        Frame(
            [&]
            {
                BeginVStack();
                EndVStack();
            });
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(LayoutTests, SettledLayoutIsNotAnimating)
    {
        float width = 50.0f;
        Rect item;
        const auto build = [&]
        {
            BeginHStack({.Width = 300.0f});
            Spacer();
            item = AllocateItem(Vec2(width, 20.0f));
            EndHStack();
        };
        Settle(build);
        EXPECT_FALSE(IsAnimating());
        EXPECT_FLOAT_EQ(item.GetRight(), 300.0f);

        // The content changes: positions that depend on it lag one frame, so another frame is requested.
        width = 90.0f;
        Frame(build);
        EXPECT_TRUE(IsAnimating());
        EXPECT_FLOAT_EQ(item.GetRight(), 340.0f);
        Frame(build);
        EXPECT_FLOAT_EQ(item.GetRight(), 300.0f);
        Frame(build);
        EXPECT_FALSE(IsAnimating());
    }
} // namespace Carbon
