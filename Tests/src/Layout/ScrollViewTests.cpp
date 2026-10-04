#include "Support/ContextTest.h"

#include <vector>

namespace Carbon
{
    class ScrollViewTests : public ContextTest
    {
    protected:
        using Builder = std::function<void()>;

        // A 200 x 100 scroll view with ten rows of 30 points: 300 points of content, 200 to scroll.
        void BuildList()
        {
            m_Rows.clear();
            BeginScrollView("list", {.Width = 200.0f, .Height = 100.0f, .Spacing = 0.0f});
            for (int i = 0; i < 10; i++)
            {
                const Rect row = AllocateItem(Vec2(180.0f, 30.0f));
                GetDrawList().AddRect(row, RowColor);
                m_Rows.push_back(row);
            }
            EndScrollView();
            m_Viewport = GetLastItemRect();
        }

        // Scrolls with the pointer over the list and lets the offset animation finish.
        void Scroll(float notches)
        {
            GetIO().AddMousePosEvent(50.0f, 50.0f);
            Frame([&] { BuildList(); }); // the list learns that it is under the pointer
            GetIO().AddMouseWheelEvent(0.0f, notches);
            Settle([&] { BuildList(); }, 90);
        }

        // The scroll indicator is the only thing drawn that is not a row.
        bool HasIndicator() const
        {
            for (const DrawVertex& vertex : GetDrawData().Vertices)
            {
                if ((vertex.Color & 0x00FFFFFFu) != (RowColor.ToRGBA8() & 0x00FFFFFFu))
                    return true;
            }
            return false;
        }

        // Long enough for the indicator, which flashes when a view appears or scrolls, to fade out again.
        static constexpr int IdleFrames = 120;
        static constexpr Color RowColor = Color(1.0f, 0.0f, 0.0f);

        std::vector<Rect> m_Rows;
        Rect m_Viewport;
    };

    TEST_F(ScrollViewTests, ContentIsClippedToTheViewport)
    {
        Settle([&] { BuildList(); }, IdleFrames);
        EXPECT_EQ(m_Viewport, Rect(0.0f, 0.0f, 200.0f, 100.0f));

        // Rows are laid out even when they are out of view...
        EXPECT_FLOAT_EQ(m_Rows[9].Y, 270.0f);

        // ...but only the rows that intersect the viewport produce geometry (rows 0-3; row 3 is cut by the
        // scissor rectangle), and their draw command carries the viewport as its clip rectangle.
        const DrawData& drawData = GetDrawData();
        EXPECT_EQ(drawData.Vertices.size(), 4u * 4u);
        ASSERT_EQ(drawData.Commands.size(), 1u);
        EXPECT_EQ(drawData.Commands[0].ClipRect, Rect(0.0f, 0.0f, 200.0f, 100.0f));
    }

    TEST_F(ScrollViewTests, TheViewportIsAnItemOfItsParent)
    {
        Rect after;
        Settle(
            [&]
            {
                BeginVStack({.Spacing = 10.0f, .Padding = 20.0f});
                BuildList();
                after = AllocateItem(Vec2(50.0f, 20.0f));
                EndVStack();
            });
        EXPECT_EQ(m_Viewport, Rect(20.0f, 20.0f, 200.0f, 100.0f));
        // The 300 points of content do not push the next item down; the viewport's height does.
        EXPECT_FLOAT_EQ(after.Y, 130.0f);
    }

    TEST_F(ScrollViewTests, WheelScrollsTheViewUnderThePointer)
    {
        Settle([&] { BuildList(); });
        EXPECT_FLOAT_EQ(m_Rows[0].Y, 0.0f);

        Scroll(-1.0f); // one notch down
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 48.0f);
        EXPECT_FLOAT_EQ(m_Rows[0].Y, -48.0f);
        EXPECT_FLOAT_EQ(m_Rows[2].Y, 12.0f);

        Scroll(1.0f); // and back up
        EXPECT_FLOAT_EQ(m_Rows[0].Y, 0.0f);
    }

    TEST_F(ScrollViewTests, OffsetIsClampedToTheContent)
    {
        Settle([&] { BuildList(); });
        Scroll(-100.0f);
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 200.0f);
        EXPECT_FLOAT_EQ(m_Rows[9].GetBottom(), 100.0f); // the last row ends at the bottom of the viewport

        Scroll(100.0f);
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 0.0f);
        EXPECT_FLOAT_EQ(m_Rows[0].Y, 0.0f);
    }

    TEST_F(ScrollViewTests, ScrollingGlidesInsteadOfJumping)
    {
        Settle([&] { BuildList(); });
        GetIO().AddMousePosEvent(50.0f, 50.0f);
        Frame([&] { BuildList(); });
        GetIO().AddMouseWheelEvent(0.0f, -2.0f);
        Frame([&] { BuildList(); });

        // The target moved at once; the visible position is on its way.
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 96.0f);
        EXPECT_LT(m_Rows[0].Y, 0.0f);
        EXPECT_GT(m_Rows[0].Y, -96.0f);
        EXPECT_TRUE(IsAnimating());

        Settle([&] { BuildList(); }, 90);
        EXPECT_FLOAT_EQ(m_Rows[0].Y, -96.0f);
    }

    TEST_F(ScrollViewTests, WheelIsIgnoredWhenThePointerIsElsewhere)
    {
        Settle([&] { BuildList(); });
        GetIO().AddMousePosEvent(400.0f, 300.0f);
        Frame([&] { BuildList(); });
        GetIO().AddMouseWheelEvent(0.0f, -3.0f);
        Settle([&] { BuildList(); });
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 0.0f);
    }

    TEST_F(ScrollViewTests, InnermostScrollViewGetsTheWheel)
    {
        const auto build = [&]
        {
            BeginScrollView("outer", {.Width = 300.0f, .Height = 200.0f, .Spacing = 0.0f});
            AllocateItem(Vec2(100.0f, 50.0f));
            BeginScrollView("inner", {.Width = 200.0f, .Height = 100.0f, .Spacing = 0.0f});
            AllocateItem(Vec2(100.0f, 400.0f));
            EndScrollView();
            AllocateItem(Vec2(100.0f, 600.0f));
            EndScrollView();
        };
        Settle(build);

        // Pointer over the inner view.
        GetIO().AddMousePosEvent(50.0f, 100.0f);
        Frame(build);
        GetIO().AddMouseWheelEvent(0.0f, -1.0f);
        Settle(build, 60);
        float outerOffset = 0.0f;
        float innerOffset = 0.0f;
        Frame(
            [&]
            {
                build();
                outerOffset = GetScrollOffset("outer").Y;
                PushID("outer");
                innerOffset = GetScrollOffset("inner").Y; // the inner view lives in the outer view's ID scope
                PopID();
            });
        EXPECT_FLOAT_EQ(innerOffset, 48.0f);
        EXPECT_FLOAT_EQ(outerOffset, 0.0f);

        // Pointer over the outer view only.
        GetIO().AddMousePosEvent(250.0f, 20.0f);
        Frame(build);
        GetIO().AddMouseWheelEvent(0.0f, -1.0f);
        Settle(build, 60);
        Frame(
            [&]
            {
                build();
                outerOffset = GetScrollOffset("outer").Y;
            });
        EXPECT_FLOAT_EQ(outerOffset, 48.0f);
    }

    TEST_F(ScrollViewTests, HorizontalScrollViewLaysOutAndScrollsSideways)
    {
        std::vector<Rect> columns;
        const auto build = [&]
        {
            columns.clear();
            BeginScrollView("strip", {.Axis = Axis::Horizontal, .Width = 200.0f, .Height = 60.0f, .Spacing = 10.0f});
            for (int i = 0; i < 6; i++)
                columns.push_back(AllocateItem(Vec2(80.0f, 40.0f)));
            EndScrollView();
        };
        Settle(build);
        EXPECT_FLOAT_EQ(columns[1].X, 90.0f);
        EXPECT_FLOAT_EQ(columns[1].Y, 0.0f);

        // A plain (vertical) wheel scrolls a horizontal view sideways. Content: 6 * 80 + 5 * 10 = 530.
        GetIO().AddMousePosEvent(50.0f, 30.0f);
        Frame(build);
        GetIO().AddMouseWheelEvent(0.0f, -100.0f);
        Settle(build, 90);
        EXPECT_FLOAT_EQ(columns[5].GetRight(), 200.0f);
        EXPECT_FLOAT_EQ(columns[0].X, -330.0f);
    }

    TEST_F(ScrollViewTests, PaddingScrollsWithTheContent)
    {
        Rect first, last;
        const auto build = [&]
        {
            BeginScrollView("padded", {.Width = 200.0f, .Height = 100.0f, .Spacing = 0.0f, .Padding = 16.0f});
            first = AllocateItem(Vec2(50.0f, 150.0f));
            last = AllocateItem(Vec2(50.0f, 150.0f));
            EndScrollView();
        };
        Settle(build);
        EXPECT_EQ(first.GetMin(), Vec2(16.0f, 16.0f));

        GetIO().AddMousePosEvent(50.0f, 50.0f);
        Frame(build);
        GetIO().AddMouseWheelEvent(0.0f, -100.0f);
        Settle(build, 90);
        // Scrolled to the end, the bottom padding is visible below the last item.
        EXPECT_FLOAT_EQ(last.GetBottom(), 100.0f - 16.0f);
    }

    TEST_F(ScrollViewTests, OffsetSurvivesWhileTheViewIsHidden)
    {
        Settle([&] { BuildList(); });
        Scroll(-2.0f);
        EXPECT_FLOAT_EQ(m_Rows[0].Y, -96.0f);

        // The view is not built for a while (another tab is showing, say).
        for (int i = 0; i < 30; i++)
            RunFrame();

        Frame([&] { BuildList(); });
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 96.0f);
        EXPECT_FLOAT_EQ(m_Rows[0].Y, -96.0f); // back where it was, without animating from the top
    }

    TEST_F(ScrollViewTests, SetScrollOffsetJumpsOrGlides)
    {
        Settle([&] { BuildList(); });

        Frame(
            [&]
            {
                SetScrollOffset("list", Vec2(0.0f, 60.0f));
                BuildList();
            });
        EXPECT_FLOAT_EQ(m_Rows[0].Y, -60.0f);

        Frame(
            [&]
            {
                SetScrollOffset("list", Vec2(0.0f, 120.0f), true);
                BuildList();
            });
        EXPECT_LT(m_Rows[0].Y, -60.0f);
        EXPECT_GT(m_Rows[0].Y, -120.0f);

        // Offsets beyond the content are clamped.
        Frame(
            [&]
            {
                SetScrollOffset("list", Vec2(0.0f, 5000.0f));
                BuildList();
            });
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 200.0f);
        EXPECT_FLOAT_EQ(m_Rows[0].Y, -200.0f);
    }

    TEST_F(ScrollViewTests, IndicatorAppearsWhileScrollingAndFadesOut)
    {
        // It flashes when the view first appears, then fades away.
        Frame([&] { BuildList(); });
        Frame([&] { BuildList(); });
        EXPECT_TRUE(HasIndicator());
        Settle([&] { BuildList(); }, IdleFrames);
        EXPECT_FALSE(HasIndicator());
        EXPECT_FALSE(IsAnimating());

        GetIO().AddMousePosEvent(50.0f, 50.0f);
        Frame([&] { BuildList(); });
        GetIO().AddMouseWheelEvent(0.0f, -1.0f);
        for (int i = 0; i < 30; i++)
            Frame([&] { BuildList(); });
        EXPECT_TRUE(HasIndicator());

        // The thumb is a pill at the trailing edge whose length reflects the visible fraction (a third).
        const DrawData& drawData = GetDrawData();
        const DrawVertex* thumb = &drawData.Vertices[drawData.Vertices.size() - 4];
        const float width = thumb[2].Position.X - thumb[0].Position.X - 2.0f;
        const float length = thumb[2].Position.Y - thumb[0].Position.Y - 2.0f;
        EXPECT_FLOAT_EQ(width, GetStyleVar(StyleVar::ScrollIndicatorWidth));
        EXPECT_NEAR(length, 96.0f / 3.0f, 0.5f);
        EXPECT_NEAR(thumb[2].Position.X - 1.0f, 198.0f, 1e-3f);

        // Idle for a while: gone again.
        Settle([&] { BuildList(); }, IdleFrames);
        EXPECT_FALSE(HasIndicator());
    }

    TEST_F(ScrollViewTests, IndicatorCanBeDragged)
    {
        Settle([&] { BuildList(); }, IdleFrames);

        // The pointer over the indicator's lane, at the trailing edge, makes it appear.
        GetIO().AddMousePosEvent(196.0f, 10.0f);
        Frame([&] { BuildList(); });
        Frame([&] { BuildList(); });
        Frame([&] { BuildList(); });
        EXPECT_TRUE(HasIndicator());

        // The thumb covers a third of the 96-point track (32 points) and travels the other 64 while the
        // content scrolls 200: dragging it down by 32 points scrolls by 100.
        GetIO().AddMouseButtonEvent(MouseButton::Left, true);
        Frame([&] { BuildList(); });
        GetIO().AddMousePosEvent(196.0f, 42.0f);
        Frame([&] { BuildList(); });
        Frame([&] { BuildList(); });
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 100.0f);
        EXPECT_FLOAT_EQ(m_Rows[0].Y, -100.0f); // the content follows the thumb directly, without gliding

        // Dragging far past the end clamps, and the drag continues outside the view.
        GetIO().AddMousePosEvent(400.0f, 900.0f);
        Frame([&] { BuildList(); });
        Frame([&] { BuildList(); });
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 200.0f);

        GetIO().AddMouseButtonEvent(MouseButton::Left, false);
        Frame([&] { BuildList(); });
        EXPECT_FALSE(GetActiveID().IsValid());
    }

    TEST_F(ScrollViewTests, PageKeysScrollTheViewUnderThePointer)
    {
        const Builder build = [&] { BuildList(); };
        Settle(build);

        // No pointer anywhere: the keys go to the outermost scroll view.
        GetIO().AddKeyEvent(Key::PageDown, true);
        Frame(build);
        GetIO().AddKeyEvent(Key::PageDown, false);
        Settle(build, 60);
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 90.0f); // nine tenths of the 100-point viewport

        GetIO().AddKeyEvent(Key::End, true);
        Frame(build);
        GetIO().AddKeyEvent(Key::End, false);
        Frame(build);
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 200.0f);

        GetIO().AddKeyEvent(Key::PageUp, true);
        Frame(build);
        GetIO().AddKeyEvent(Key::PageUp, false);
        Frame(build);
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 110.0f);

        GetIO().AddKeyEvent(Key::Home, true);
        Frame(build);
        GetIO().AddKeyEvent(Key::Home, false);
        Frame(build);
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 0.0f);
    }

    TEST_F(ScrollViewTests, CodeAfterAScrollViewStillSeesItsOwnLastItem)
    {
        // The draggable indicator is an item of its own, but it must not replace the content's last item.
        ID lastItem;
        Settle(
            [&]
            {
                BeginScrollView("list", {.Width = 200.0f, .Height = 100.0f});
                AllocateItem(Vec2(100.0f, 500.0f));
                Button("Inside");
                EndScrollView();
                lastItem = GetItemID();
            });
        PushID("list");
        const ID button = GetID("Inside");
        PopID();
        EXPECT_EQ(lastItem, button);
    }

    TEST_F(ScrollViewTests, NoIndicatorWhenEverythingFits)
    {
        Settle(
            [&]
            {
                BeginScrollView("short", {.Width = 200.0f, .Height = 100.0f});
                GetDrawList().AddRect(AllocateItem(Vec2(50.0f, 40.0f)), Color::Black());
                EndScrollView();
            },
            3);
        EXPECT_EQ(GetDrawData().Vertices.size(), 4u);
        EXPECT_FLOAT_EQ(GetScrollOffset("short").Y, 0.0f);
    }

    TEST_F(ScrollViewTests, FillingScrollViewSurvivesItsFirstFrame)
    {
        // A scroll view that fills its parent has no size at all on its first frame, while its content is
        // already there. Nothing may be drawn with that empty viewport.
        GetIO().SetDisplaySize(300.0f, 200.0f);
        Rect viewport;
        const auto build = [&]
        {
            BeginVStack({.Width = Size::Fill(), .Height = Size::Fill()});
            BeginScrollView("page");
            AllocateItem(Vec2(100.0f, 1000.0f));
            EndScrollView();
            viewport = GetLastItemRect();
            EndVStack();
        };
        Frame(build);
        Settle(build);
        EXPECT_EQ(viewport, Rect(0.0f, 0.0f, 300.0f, 200.0f));
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(ScrollViewTests, UnbalancedScrollViewIsReportedOnce)
    {
        Frame([&] { BeginScrollView("open", {.Width = 100.0f, .Height = 100.0f}); });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("Unbalanced layout"), std::string::npos);

        m_AssertMessages.clear();
        Frame([&] { EndScrollView(); });
        EXPECT_EQ(m_AssertMessages.size(), 1u);
    }
} // namespace Carbon
