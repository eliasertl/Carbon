#include <cmath>
#include <vector>

#include "Support/WidgetTest.h"

namespace Carbon
{
    class TouchTests : public WidgetTest
    {
    protected:
        // A button at (0, 0, 100, 30).
        Builder ButtonInterface()
        {
            return [this]
            {
                const Interaction interaction = ButtonBehavior(HashID("button"), Rect(0.0f, 0.0f, 100.0f, 30.0f));
                m_Button = interaction;
                m_Clicks += interaction.Clicked ? 1 : 0;
                m_EverHovered = m_EverHovered || interaction.Hovered;
            };
        }

        Interaction m_Button;
        int m_Clicks = 0;
        bool m_EverHovered = false;
    };

    TEST_F(TouchTests, AFingerDrivesThePointerOnlyWhileItIsDown)
    {
        IO& io = GetIO();
        io.AddTouchEvent(TouchPhase::Began, 7, 40.0f, 20.0f);
        RunFrame();
        // The pointer arrives first; the press waits a frame, as after a mouse move.
        NewFrame();
        EXPECT_TRUE(IsMousePosValid());
        EXPECT_EQ(GetMousePos(), Vec2(40.0f, 20.0f));
        EXPECT_TRUE(IsMousePressed());
        EXPECT_EQ(GetPointerType(), PointerType::Touch);
        EndFrame();

        io.AddTouchEvent(TouchPhase::Moved, 7, 45.0f, 25.0f);
        NewFrame();
        EXPECT_EQ(GetMousePos(), Vec2(45.0f, 25.0f));
        EXPECT_TRUE(IsMouseDown());
        EndFrame();

        io.AddTouchEvent(TouchPhase::Ended, 7, 46.0f, 26.0f);
        NewFrame();
        EXPECT_TRUE(IsMouseReleased());
        EXPECT_TRUE(IsMousePosValid()); // released where the finger lifted
        EndFrame();
        NewFrame();
        EXPECT_FALSE(IsMousePosValid()); // then the pointer is gone: nothing stays hovered
        EndFrame();
    }

    TEST_F(TouchTests, ATapClicksWithoutEverHovering)
    {
        Frame(ButtonInterface());
        Tap(Vec2(50.0f, 15.0f), ButtonInterface());
        EXPECT_EQ(m_Clicks, 1);
        EXPECT_FALSE(m_EverHovered);
        EXPECT_FALSE(GetHoveredID().IsValid());
        EXPECT_FALSE(GetActiveID().IsValid());
    }

    TEST_F(TouchTests, AFastTapWithinOneFrameStillClicks)
    {
        Frame(ButtonInterface());
        GetIO().AddTouchEvent(TouchPhase::Began, 1, 50.0f, 15.0f);
        GetIO().AddTouchEvent(TouchPhase::Ended, 1, 50.0f, 15.0f);
        for (int i = 0; i < 6; i++)
            Frame(ButtonInterface());
        EXPECT_EQ(m_Clicks, 1);
    }

    TEST_F(TouchTests, ACancelledTouchActivatesNothing)
    {
        Frame(ButtonInterface());
        TouchDown(Vec2(50.0f, 15.0f), ButtonInterface());
        EXPECT_TRUE(m_Button.Pressed);
        GetIO().AddTouchEvent(TouchPhase::Cancelled, 1, 50.0f, 15.0f);
        Frame(ButtonInterface());
        Frame(ButtonInterface());
        EXPECT_EQ(m_Clicks, 0);
        EXPECT_FALSE(m_Button.Pressed);
    }

    TEST_F(TouchTests, LiftingOutsideTheControlDoesNotClick)
    {
        Frame(ButtonInterface());
        TouchDown(Vec2(50.0f, 15.0f), ButtonInterface());
        TouchMove(Vec2(50.0f, 200.0f), ButtonInterface());
        TouchUp(Vec2(50.0f, 200.0f), ButtonInterface());
        EXPECT_EQ(m_Clicks, 0);
    }

    TEST_F(TouchTests, ASecondFingerDoesNotMoveThePointer)
    {
        TouchDown(Vec2(10.0f, 10.0f), ButtonInterface(), 1);
        GetIO().AddTouchEvent(TouchPhase::Began, 2, 300.0f, 300.0f);
        NewFrame();
        EXPECT_EQ(GetMousePos(), Vec2(10.0f, 10.0f));
        EXPECT_EQ(GetTouchCount(), 2);
        EXPECT_EQ(GetTouchPosition(1), Vec2(300.0f, 300.0f));
        EndFrame();
    }

    TEST_F(TouchTests, AMouseMovedByTheHostDuringATouchIsIgnored)
    {
        TouchDown(Vec2(10.0f, 10.0f), ButtonInterface());
        GetIO().AddMousePosEvent(500.0f, 500.0f);
        NewFrame();
        EXPECT_EQ(GetMousePos(), Vec2(10.0f, 10.0f));
        EndFrame();
    }

    TEST_F(TouchTests, TouchModeFollowsTheLatestPointer)
    {
        Frame([] { EXPECT_FALSE(IsTouchMode()); });
        GetIO().AddTouchEvent(TouchPhase::Began, 1, 10.0f, 10.0f);
        Frame([] { EXPECT_TRUE(IsTouchMode()); });
        GetIO().AddTouchEvent(TouchPhase::Ended, 1, 10.0f, 10.0f);
        RunFrame();
        RunFrame();
        Frame([] { EXPECT_TRUE(IsTouchMode()); }); // it stays until other pointer input arrives
        GetIO().AddMousePosEvent(20.0f, 20.0f);
        Frame([] { EXPECT_FALSE(IsTouchMode()); });

        // A pen counts as touch.
        GetIO().AddTouchEvent(TouchPhase::Began, 2, 10.0f, 10.0f, PointerType::Pen);
        Frame([] { EXPECT_TRUE(IsTouchMode()); });
    }

    TEST_F(TouchTests, TheHostSetsTheDefaultAndCanForceTouchMode)
    {
        GetIO().SetDefaultPointerType(PointerType::Touch);
        Frame([] { EXPECT_TRUE(IsTouchMode()); });
        GetIO().AddMousePosEvent(20.0f, 20.0f);
        Frame([] { EXPECT_FALSE(IsTouchMode()); });

        GetIO().SetTouchModeOverride(true);
        Frame([] { EXPECT_TRUE(IsTouchMode()); });
        GetIO().SetTouchModeOverride(std::nullopt);
        Frame([] { EXPECT_FALSE(IsTouchMode()); });
    }

    TEST_F(TouchTests, TheSizeClassFollowsTheDisplayWidth)
    {
        GetIO().SetDisplaySize(390.0f, 844.0f); // an iPhone in portrait
        Frame([] { EXPECT_EQ(GetSizeClass(), SizeClass::Compact); });
        GetIO().SetDisplaySize(844.0f, 390.0f); // the same in landscape
        Frame([] { EXPECT_EQ(GetSizeClass(), SizeClass::Regular); });
        GetIO().SetDisplaySize(744.0f, 1133.0f); // an iPad mini in portrait
        Frame([] { EXPECT_EQ(GetSizeClass(), SizeClass::Regular); });
        GetIO().SetDisplaySize(CompactWidthLimit - 1.0f, 600.0f);
        Frame([] { EXPECT_TRUE(IsCompactWidth()); });
        GetIO().SetDisplaySize(CompactWidthLimit, 600.0f);
        Frame([] { EXPECT_FALSE(IsCompactWidth()); });

        // The host can force either class.
        GetIO().SetDisplaySize(390.0f, 844.0f);
        GetIO().SetSizeClassOverride(SizeClass::Regular);
        Frame([] { EXPECT_EQ(GetSizeClass(), SizeClass::Regular); });
        GetIO().SetDisplaySize(1200.0f, 800.0f);
        GetIO().SetSizeClassOverride(SizeClass::Compact);
        Frame([] { EXPECT_EQ(GetSizeClass(), SizeClass::Compact); });
    }

    TEST_F(TouchTests, LosingFocusDropsTheFingersWithoutAClick)
    {
        Frame(ButtonInterface());
        TouchDown(Vec2(50.0f, 15.0f), ButtonInterface());
        GetIO().AddFocusEvent(false);
        Frame(ButtonInterface());
        GetIO().AddFocusEvent(true);
        Frame(ButtonInterface());
        EXPECT_EQ(m_Clicks, 0);
        EXPECT_EQ(GetTouchCount(), 0);
        EXPECT_FALSE(IsMousePosValid());
    }

    // ---- Scrolling ------------------------------------------------------------------------------------------------

    class TouchScrollTests : public WidgetTest
    {
    protected:
        // A 200 x 100 scroll view with ten rows of 30 points (200 points to scroll); the first row is a button.
        Builder List()
        {
            return [this]
            {
                BeginScrollView("list", {.Width = 200.0f, .Height = 100.0f, .Spacing = 0.0f});
                const Rect first = AllocateItem(Vec2(180.0f, 30.0f));
                m_Clicks += ButtonBehavior(HashID("row"), first).Clicked ? 1 : 0;
                m_FirstRow = first;
                for (int i = 1; i < 10; i++)
                    AllocateItem(Vec2(180.0f, 30.0f));
                EndScrollView();
            };
        }

        // Where the content is shown: how far the first row moved up.
        float GetShownOffset() const { return -m_FirstRow.Y; }

        Rect m_FirstRow;
        int m_Clicks = 0;
    };

    TEST_F(TouchScrollTests, TheContentFollowsTheFinger)
    {
        Settle(List());
        TouchDown(Vec2(100.0f, 80.0f), List());
        // Past the slop the scroll view takes the finger; from then on the content moves with it, one to one.
        TouchMove(Vec2(100.0f, 60.0f), List());
        TouchMove(Vec2(100.0f, 50.0f), List());
        const float before = GetShownOffset();
        TouchMove(Vec2(100.0f, 30.0f), List());
        EXPECT_NEAR(GetShownOffset() - before, 20.0f, 0.5f);
        EXPECT_EQ(m_Clicks, 0);
    }

    TEST_F(TouchScrollTests, ADragThatStartsOnAControlScrollsInsteadOfActivatingIt)
    {
        Settle(List());
        TouchDown(Vec2(100.0f, 15.0f), List());
        TouchDrag(Vec2(100.0f, 15.0f), Vec2(100.0f, -15.0f), 3, List());
        TouchUp(Vec2(100.0f, -15.0f), List());
        EXPECT_EQ(m_Clicks, 0);
        EXPECT_GT(GetShownOffset(), 10.0f);
    }

    TEST_F(TouchScrollTests, AMoveWithinTheSlopIsStillATap)
    {
        Settle(List());
        TouchDown(Vec2(100.0f, 15.0f), List());
        TouchMove(Vec2(104.0f, 19.0f), List());
        TouchUp(Vec2(104.0f, 19.0f), List());
        EXPECT_EQ(m_Clicks, 1);
        EXPECT_FLOAT_EQ(GetShownOffset(), 0.0f);
    }

    TEST_F(TouchScrollTests, AThrownListKeepsMovingAndSlowsDown)
    {
        Settle(List());
        TouchDown(Vec2(100.0f, 90.0f), List());
        TouchDrag(Vec2(100.0f, 90.0f), Vec2(100.0f, 60.0f), 6, List()); // 5 points per frame: 300 points per second
        GetIO().AddTouchEvent(TouchPhase::Ended, 1, 100.0f, 60.0f);
        Frame(List());
        const float released = GetShownOffset();
        Frame(List());
        Frame(List());
        const float afterTwo = GetShownOffset();
        EXPECT_GT(afterTwo, released + 4.0f); // momentum
        for (int i = 0; i < 20; i++)
            Frame(List());
        const float later = GetShownOffset();
        Frame(List());
        Frame(List());
        EXPECT_LT(GetShownOffset() - later, afterTwo - released); // slowing down
        Settle(List(), 300);
        const float rest = GetShownOffset();
        EXPECT_GE(rest, 0.0f);
        EXPECT_LE(rest, 200.0f);
        EXPECT_NEAR(GetScrollOffset("list").Y, rest, 0.5f); // shown on whole pixels
    }

    TEST_F(TouchScrollTests, PulledBeyondTheTopItResistsAndSpringsBack)
    {
        Settle(List());
        TouchDown(Vec2(100.0f, 10.0f), List());
        TouchDrag(Vec2(100.0f, 10.0f), Vec2(100.0f, 90.0f), 8, List());
        // The finger moved 80 points past the slop; the content follows less than that.
        const float pulled = -GetShownOffset();
        EXPECT_GT(pulled, 10.0f);
        EXPECT_LT(pulled, 70.0f);
        EXPECT_EQ(GetScrollOffset("list").Y, 0.0f); // the target stays in range
        TouchUp(Vec2(100.0f, 90.0f), List());
        Settle(List(), 120);
        EXPECT_NEAR(GetShownOffset(), 0.0f, 0.01f);
    }

    TEST_F(TouchScrollTests, AThrowPastTheEndBouncesBack)
    {
        Settle(List());
        // Fast: 40 points per frame towards the end.
        TouchDown(Vec2(100.0f, 95.0f), List());
        TouchDrag(Vec2(100.0f, 95.0f), Vec2(100.0f, -105.0f), 5, List());
        GetIO().AddTouchEvent(TouchPhase::Ended, 1, 100.0f, -105.0f);
        float largest = 0.0f;
        for (int i = 0; i < 200; i++)
        {
            Frame(List());
            largest = std::max(largest, GetShownOffset());
        }
        EXPECT_GT(largest, 200.5f);                   // it went past the end
        EXPECT_NEAR(GetShownOffset(), 200.0f, 0.01f); // and came back to it
    }

    TEST_F(TouchScrollTests, TheMouseWheelBehavesAsBefore)
    {
        Settle(List());
        MoveMouse(Vec2(100.0f, 50.0f), List());
        GetIO().AddMouseWheelEvent(0.0f, -1.0f);
        Settle(List(), 90);
        EXPECT_NEAR(GetShownOffset(), 48.0f, 0.01f);
    }

    // ---- Drag controls keep their drag ------------------------------------------------------------------------------

    TEST_F(TouchScrollTests, ASliderKeepsItsDrag)
    {
        float value = 0.5f;
        float shown = 0.0f;
        const Builder build = [&]
        {
            BeginScrollView("list", {.Width = 300.0f, .Height = 100.0f});
            Slider("##slider", &value, 0.0f, 1.0f, {.Width = 200.0f});
            shown = GetItemRect().Y;
            AllocateItem(Vec2(100.0f, 400.0f));
            EndScrollView();
        };
        Settle(build);
        const Rect slider = Rect(0.0f, shown, 200.0f, 20.0f);
        const Vec2 knob(100.0f, slider.GetCenter().Y);
        TouchDown(knob, build);
        TouchDrag(knob, knob + Vec2(60.0f, 30.0f), 6, build);
        TouchUp(knob + Vec2(60.0f, 30.0f), build);
        EXPECT_GT(value, 0.6f);
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 0.0f);
    }

    // ---- Long press -----------------------------------------------------------------------------------------------

    TEST_F(TouchTests, AFingerThatRestsIsALongPress)
    {
        int longPresses = 0;
        const Builder build = [&]
        {
            m_Clicks += ButtonBehavior(HashID("button"), Rect(0.0f, 0.0f, 100.0f, 30.0f)).Clicked ? 1 : 0;
            longPresses += IsItemLongPressed() ? 1 : 0;
        };
        TouchDown(Vec2(50.0f, 15.0f), build);
        for (int i = 0; i < 25; i++)
            Frame(build);
        EXPECT_EQ(longPresses, 0); // 0.45 seconds
        for (int i = 0; i < 20; i++)
            Frame(build);
        EXPECT_EQ(longPresses, 1); // once
    }

    TEST_F(TouchTests, AFingerThatMovesIsNoLongPress)
    {
        int longPresses = 0;
        const Builder build = [&]
        {
            ButtonBehavior(HashID("button"), Rect(0.0f, 0.0f, 100.0f, 30.0f));
            longPresses += IsItemLongPressed() ? 1 : 0;
        };
        TouchDown(Vec2(20.0f, 15.0f), build);
        TouchMove(Vec2(40.0f, 15.0f), build);
        for (int i = 0; i < 60; i++)
            Frame(build);
        EXPECT_EQ(longPresses, 0);
    }

    TEST_F(TouchTests, AMouseHeldStillIsNoLongPress)
    {
        int longPresses = 0;
        const Builder build = [&]
        {
            ButtonBehavior(HashID("button"), Rect(0.0f, 0.0f, 100.0f, 30.0f));
            longPresses += IsItemLongPressed() ? 1 : 0;
        };
        MoveMouse(Vec2(50.0f, 15.0f), build);
        PressMouse(build);
        for (int i = 0; i < 60; i++)
            Frame(build);
        EXPECT_EQ(longPresses, 0);
    }

    TEST_F(TouchTests, ALongPressStartsADrag)
    {
        bool isDragging = false;
        int delivered = 0;
        const Builder build = [&]
        {
            isDragging = false;
            if (BeginDragSource(HashID("card"), Rect(10.0f, 10.0f, 100.0f, 40.0f)))
            {
                SetDragPayload("Task", 7);
                isDragging = true;
                AllocateItem(Vec2(60.0f, 20.0f));
                EndDragSource();
            }
            delivered += AcceptDrop(HashID("bin"), Rect(300.0f, 10.0f, 200.0f, 200.0f), "Task").IsDelivered ? 1 : 0;
        };
        // Moving at once is not a drag on a touchscreen.
        TouchDown(Vec2(50.0f, 30.0f), build);
        TouchDrag(Vec2(50.0f, 30.0f), Vec2(120.0f, 30.0f), 4, build);
        EXPECT_FALSE(isDragging);
        TouchUp(Vec2(120.0f, 30.0f), build);

        // Holding first lifts the card; then it follows the finger and drops where the finger lifts.
        TouchDown(Vec2(50.0f, 30.0f), build);
        for (int i = 0; i < 35; i++)
            Frame(build);
        EXPECT_TRUE(isDragging);
        TouchDrag(Vec2(50.0f, 30.0f), Vec2(400.0f, 100.0f), 6, build);
        EXPECT_TRUE(isDragging);
        TouchUp(Vec2(400.0f, 100.0f), build);
        Frame(build);
        EXPECT_EQ(delivered, 1);
    }

    // ---- Pans -----------------------------------------------------------------------------------------------------

    TEST_F(TouchTests, AnEdgePanWinsOverTheContentUnderIt)
    {
        Pan edge;
        Pan content;
        const Builder build = [&]
        {
            content = PanBehavior(HashID("content"), Rect(0.0f, 0.0f, 400.0f, 400.0f));
            edge = PanBehavior(HashID("edge"), Rect(0.0f, 0.0f, 20.0f, 400.0f),
                               {.Directions = PanDirections::Right, .Priority = 1});
        };
        TouchDown(Vec2(5.0f, 100.0f), build);
        TouchMove(Vec2(25.0f, 102.0f), build);
        TouchMove(Vec2(35.0f, 102.0f), build);
        EXPECT_TRUE(edge.Active);
        EXPECT_TRUE(edge.Began);
        EXPECT_FALSE(content.Active);
        TouchMove(Vec2(65.0f, 102.0f), build);
        EXPECT_FALSE(edge.Began);
        EXPECT_FLOAT_EQ(edge.Translation.X, 40.0f); // from where the pan was recognized
        GetIO().AddTouchEvent(TouchPhase::Ended, 1, 65.0f, 102.0f);
        Frame(build);
        EXPECT_TRUE(edge.Ended);
        EXPECT_FALSE(edge.Active);
        EXPECT_GT(edge.Velocity.X, 0.0f);

        // A pan to the left from the same edge is not the edge's.
        TouchUp(Vec2(65.0f, 102.0f), build);
        TouchDown(Vec2(15.0f, 100.0f), build);
        TouchMove(Vec2(-5.0f, 100.0f), build);
        TouchMove(Vec2(-10.0f, 100.0f), build);
        EXPECT_FALSE(edge.Active);
        EXPECT_TRUE(content.Active);
    }

    TEST_F(TouchTests, AMouseDragIsNoPan)
    {
        Pan pan;
        const Builder build = [&] { pan = PanBehavior(HashID("content"), Rect(0.0f, 0.0f, 400.0f, 400.0f)); };
        MoveMouse(Vec2(100.0f, 100.0f), build);
        PressMouse(build);
        for (int i = 1; i <= 5; i++)
        {
            GetIO().AddMousePosEvent(100.0f, 100.0f + 10.0f * static_cast<float>(i));
            Frame(build);
        }
        EXPECT_FALSE(pan.Active);
    }

    // ---- Zoom -----------------------------------------------------------------------------------------------------

    class ZoomTests : public WidgetTest
    {
    protected:
        // A 200 x 200 zoomable area at (100, 100).
        Builder Picture()
        {
            return [this] { m_Zoom = ZoomBehavior(HashID("picture"), Area, {.MinScale = 1.0f, .MaxScale = 3.0f}); };
        }

        // Two fingers 40 points apart around `center`, then apart by `distance`.
        void Pinch(Vec2 center, float distance)
        {
            GetIO().AddTouchEvent(TouchPhase::Began, 1, center.X - 20.0f, center.Y);
            GetIO().AddTouchEvent(TouchPhase::Began, 2, center.X + 20.0f, center.Y);
            for (int i = 0; i < 3; i++)
                Frame(Picture());
            for (int step = 1; step <= 5; step++)
            {
                const float half = 20.0f + (distance * 0.5f - 20.0f) * static_cast<float>(step) / 5.0f;
                GetIO().AddTouchEvent(TouchPhase::Moved, 1, center.X - half, center.Y);
                GetIO().AddTouchEvent(TouchPhase::Moved, 2, center.X + half, center.Y);
                Frame(Picture());
            }
        }

        void Lift()
        {
            GetIO().AddTouchEvent(TouchPhase::Ended, 2, 0.0f, 0.0f);
            GetIO().AddTouchEvent(TouchPhase::Ended, 1, 0.0f, 0.0f);
            for (int i = 0; i < 4; i++)
                Frame(Picture());
        }

        static constexpr Rect Area = Rect(100.0f, 100.0f, 200.0f, 200.0f);
        Zoom m_Zoom;
    };

    TEST_F(ZoomTests, PinchingScalesAroundTheFingers)
    {
        Frame(Picture());
        EXPECT_FLOAT_EQ(m_Zoom.Scale, 1.0f);
        const Vec2 center(150.0f, 150.0f);
        Pinch(center, 80.0f);
        EXPECT_TRUE(m_Zoom.Active);
        EXPECT_NEAR(m_Zoom.Scale, 2.0f, 0.01f);
        // The point between the fingers stays where it was: (50, 50) of the content is now drawn at 2x.
        const Rect zoomed = m_Zoom.GetZoomedRect(Area);
        EXPECT_NEAR(zoomed.X + 50.0f * m_Zoom.Scale, center.X, 0.5f);
        EXPECT_NEAR(zoomed.Y + 50.0f * m_Zoom.Scale, center.Y, 0.5f);
        Lift();
        Settle(Picture(), 60);
        EXPECT_FALSE(m_Zoom.Active);
        EXPECT_NEAR(m_Zoom.Scale, 2.0f, 0.01f); // within the limits it stays
    }

    TEST_F(ZoomTests, BeyondTheLimitsItResistsAndSpringsBack)
    {
        Frame(Picture());
        Pinch(Vec2(200.0f, 200.0f), 200.0f); // five times the distance
        EXPECT_GT(m_Zoom.Scale, 3.0f);
        EXPECT_LT(m_Zoom.Scale, 5.0f);
        Lift();
        Settle(Picture(), 90);
        EXPECT_NEAR(m_Zoom.Scale, 3.0f, 0.01f);

        // Pinching in below the smallest scale springs back to it too, with the content in place.
        Pinch(Vec2(200.0f, 200.0f), 4.0f);
        EXPECT_LT(m_Zoom.Scale, 3.0f);
        Lift();
        Settle(Picture(), 90);
        EXPECT_NEAR(m_Zoom.Scale, 1.0f, 0.01f);
        EXPECT_NEAR(m_Zoom.Offset.X, 0.0f, 0.01f);
        EXPECT_NEAR(m_Zoom.Offset.Y, 0.0f, 0.01f);
    }

    TEST_F(ZoomTests, TwoFingersPanZoomedContent)
    {
        Frame(Picture());
        Pinch(Vec2(200.0f, 200.0f), 80.0f);
        const Vec2 before = m_Zoom.Offset;
        for (int step = 1; step <= 4; step++)
        {
            const float dx = 10.0f * static_cast<float>(step);
            GetIO().AddTouchEvent(TouchPhase::Moved, 1, 160.0f + dx, 200.0f);
            GetIO().AddTouchEvent(TouchPhase::Moved, 2, 240.0f + dx, 200.0f);
            Frame(Picture());
        }
        EXPECT_NEAR(m_Zoom.Offset.X - before.X, 40.0f, 0.5f);
        EXPECT_NEAR(m_Zoom.Scale, 2.0f, 0.01f);
    }

    TEST_F(ZoomTests, ADoubleTapZoomsInAndOut)
    {
        Frame(Picture());
        Tap(Vec2(200.0f, 200.0f), Picture());
        Tap(Vec2(200.0f, 200.0f), Picture());
        Settle(Picture(), 60);
        EXPECT_NEAR(m_Zoom.Scale, 2.0f, 0.01f);
        Tap(Vec2(200.0f, 200.0f), Picture());
        Tap(Vec2(200.0f, 200.0f), Picture());
        Settle(Picture(), 60);
        EXPECT_NEAR(m_Zoom.Scale, 1.0f, 0.01f);
    }

    TEST_F(ZoomTests, AZoomableImageShowsPartOfItsTextureInItsFrame)
    {
        const Builder build = [&]
        {
            SetCursorPos(Area.GetMin());
            Image(MakeTextureID(uint64_t{42}), Area.GetSize(), {.Zoomable = true});
        };
        Settle(build);
        const auto uvWidth = [] { return GetDrawData().Vertices[1].UV.X - GetDrawData().Vertices[0].UV.X; };
        const float full = uvWidth();
        GetIO().AddTouchEvent(TouchPhase::Began, 1, 180.0f, 200.0f);
        GetIO().AddTouchEvent(TouchPhase::Began, 2, 220.0f, 200.0f);
        for (int i = 0; i < 3; i++)
            Frame(build);
        GetIO().AddTouchEvent(TouchPhase::Moved, 1, 160.0f, 200.0f);
        GetIO().AddTouchEvent(TouchPhase::Moved, 2, 240.0f, 200.0f);
        Frame(build);
        // Twice the size: half of the texture fills the same frame.
        const DrawVertex& corner = GetDrawData().Vertices[0];
        EXPECT_NEAR(corner.Position.X, Area.X - 1.0f, 1.0f);
        EXPECT_NEAR(uvWidth(), full * 0.5f, 0.02f);
    }

    TEST_F(ZoomTests, TheMouseDoesNotZoom)
    {
        Frame(Picture());
        MoveMouse(Vec2(200.0f, 200.0f), Picture());
        for (int i = 0; i < 2; i++)
        {
            PressMouse(Picture());
            ReleaseMouse(Picture());
        }
        Settle(Picture(), 30);
        EXPECT_FLOAT_EQ(m_Zoom.Scale, 1.0f);
    }
} // namespace Carbon
