#include "Support/WidgetTest.h"

#include <string>
#include <vector>

namespace Carbon
{
    class InteractionTests : public WidgetTest
    {
    protected:
        // Three buttons in a column, 100 x 30 each, at y = 0, 40 and 80.
        void BuildButtons()
        {
            static const char* const Names[3] = {"A", "B", "C"};
            for (int i = 0; i < 3; i++)
            {
                const Rect rect(0.0f, float(i) * 40.0f, 100.0f, 30.0f);
                ButtonBehaviorOptions options;
                options.Disabled = m_Disabled[i];
                m_Results[i] = ButtonBehavior(HashID(Names[i]), rect, options);
                m_Clicks[i] += m_Results[i].Clicked ? 1 : 0;
            }
        }

        Builder Buttons()
        {
            return [this] { BuildButtons(); };
        }

        static ID Name(const char* name) { return HashID(name); }

        Interaction m_Results[3];
        int m_Clicks[3] = {0, 0, 0};
        bool m_Disabled[3] = {false, false, false};
    };

    TEST_F(InteractionTests, HoverFollowsThePointer)
    {
        Frame(Buttons());
        EXPECT_FALSE(m_Results[0].Hovered);

        MoveMouse(Vec2(50.0f, 15.0f), Buttons());
        EXPECT_TRUE(m_Results[0].Hovered);
        EXPECT_FALSE(m_Results[1].Hovered);
        EXPECT_EQ(GetHoveredID(), Name("A"));
        EXPECT_TRUE(GetIO().WantsMouse());

        MoveMouse(Vec2(50.0f, 55.0f), Buttons());
        EXPECT_FALSE(m_Results[0].Hovered);
        EXPECT_TRUE(m_Results[1].Hovered);

        MoveMouse(Vec2(300.0f, 300.0f), Buttons());
        EXPECT_FALSE(m_Results[1].Hovered);
        EXPECT_FALSE(GetIO().WantsMouse());

        GetIO().AddMousePosEvent(50.0f, 15.0f);
        Frame(Buttons());
        Frame(Buttons());
        GetIO().AddMouseLeaveEvent();
        Frame(Buttons());
        Frame(Buttons());
        EXPECT_FALSE(m_Results[0].Hovered);
    }

    TEST_F(InteractionTests, ClickIsPressThenReleaseOverTheItem)
    {
        MoveMouse(Vec2(50.0f, 15.0f), Buttons());
        PressMouse(Buttons());
        EXPECT_TRUE(m_Results[0].Pressed);
        EXPECT_EQ(m_Clicks[0], 0); // nothing happens on press
        EXPECT_EQ(GetActiveID(), Name("A"));

        ReleaseMouse(Buttons());
        EXPECT_EQ(m_Clicks[0], 1);
        EXPECT_FALSE(m_Results[0].Pressed);
        EXPECT_FALSE(GetActiveID().IsValid());

        Frame(Buttons());
        EXPECT_EQ(m_Clicks[0], 1); // exactly one frame
    }

    TEST_F(InteractionTests, FastClickInASingleFrameStillRegisters)
    {
        // Move, press and release all arrive between two frames, as with a very fast click or a slow frame.
        GetIO().AddMousePosEvent(50.0f, 55.0f);
        GetIO().AddMouseButtonEvent(MouseButton::Left, true);
        GetIO().AddMouseButtonEvent(MouseButton::Left, false);
        for (int i = 0; i < 4; i++)
            Frame(Buttons());
        EXPECT_EQ(m_Clicks[1], 1);
        EXPECT_EQ(m_Clicks[0] + m_Clicks[2], 0);
    }

    TEST_F(InteractionTests, DraggingOffCancelsTheClick)
    {
        MoveMouse(Vec2(50.0f, 15.0f), Buttons());
        PressMouse(Buttons());

        // While the button is held, the pointer leaves: the pressed look goes away, and no other item reacts.
        GetIO().AddMousePosEvent(50.0f, 55.0f);
        Frame(Buttons());
        Frame(Buttons());
        EXPECT_FALSE(m_Results[0].Pressed);
        EXPECT_FALSE(m_Results[1].Hovered);
        EXPECT_EQ(GetActiveID(), Name("A"));

        ReleaseMouse(Buttons());
        EXPECT_EQ(m_Clicks[0], 0);
        EXPECT_EQ(m_Clicks[1], 0);

        // Coming back before releasing restores the press and still clicks.
        MoveMouse(Vec2(50.0f, 15.0f), Buttons());
        PressMouse(Buttons());
        GetIO().AddMousePosEvent(50.0f, 200.0f);
        Frame(Buttons());
        GetIO().AddMousePosEvent(50.0f, 15.0f);
        Frame(Buttons());
        EXPECT_TRUE(m_Results[0].Pressed);
        ReleaseMouse(Buttons());
        EXPECT_EQ(m_Clicks[0], 1);
    }

    TEST_F(InteractionTests, TopmostOfOverlappingItemsWins)
    {
        // A clickable row with a button on top of it, as in a list with a delete button per row.
        int rowClicks = 0;
        int buttonClicks = 0;
        Interaction row;
        Interaction button;
        const Builder build = [&]
        {
            row = ButtonBehavior(Name("row"), Rect(0.0f, 0.0f, 300.0f, 40.0f));
            button = ButtonBehavior(Name("delete"), Rect(240.0f, 5.0f, 50.0f, 30.0f));
            rowClicks += row.Clicked ? 1 : 0;
            buttonClicks += button.Clicked ? 1 : 0;
        };

        Click(Vec2(260.0f, 20.0f), build);
        EXPECT_EQ(buttonClicks, 1);
        EXPECT_EQ(rowClicks, 0);
        EXPECT_FALSE(row.Hovered);

        Click(Vec2(100.0f, 20.0f), build);
        EXPECT_EQ(rowClicks, 1);
        EXPECT_EQ(buttonClicks, 1);
    }

    TEST_F(InteractionTests, HigherLayersWinRegardlessOfSubmissionOrder)
    {
        int overlayClicks = 0;
        int contentClicks = 0;
        const Builder build = [&]
        {
            // The overlay is submitted first, the content after it; the overlay is still on top.
            GetDrawList().PushLayer(DrawLayer::Overlay);
            overlayClicks += ButtonBehavior(Name("overlay"), Rect(0.0f, 0.0f, 100.0f, 100.0f)).Clicked ? 1 : 0;
            GetDrawList().PopLayer();
            contentClicks += ButtonBehavior(Name("content"), Rect(0.0f, 0.0f, 100.0f, 100.0f)).Clicked ? 1 : 0;
        };
        Click(Vec2(50.0f, 50.0f), build);
        EXPECT_EQ(overlayClicks, 1);
        EXPECT_EQ(contentClicks, 0);
    }

    TEST_F(InteractionTests, ClippedPartsOfAnItemDoNotReact)
    {
        Interaction item;
        const Builder build = [&]
        {
            BeginScrollView("clip", {.Width = 100.0f, .Height = 50.0f});
            item = ButtonBehavior(Name("tall"), Rect(0.0f, 0.0f, 100.0f, 200.0f));
            EndScrollView();
        };
        MoveMouse(Vec2(50.0f, 25.0f), build);
        EXPECT_TRUE(item.Hovered);
        MoveMouse(Vec2(50.0f, 120.0f), build); // inside the item, outside the scroll view
        EXPECT_FALSE(item.Hovered);
    }

    TEST_F(InteractionTests, ActivateOnPressAndRepeat)
    {
        int presses = 0;
        const Builder build = [&]
        {
            ButtonBehaviorOptions options;
            options.ActivateOnPress = true;
            options.Repeat = true;
            presses += ButtonBehavior(Name("stepper"), Rect(0.0f, 0.0f, 40.0f, 40.0f), options).Clicked ? 1 : 0;
        };
        MoveMouse(Vec2(20.0f, 20.0f), build);
        PressMouse(build);
        EXPECT_EQ(presses, 1); // immediately, on the press

        // Held for a second: repeats after the initial delay, like a held key.
        for (int i = 0; i < 60; i++)
            Frame(build);
        EXPECT_GE(presses, 10);
        EXPECT_LE(presses, 15);

        const int held = presses;
        ReleaseMouse(build);
        EXPECT_EQ(presses, held); // the release itself does not activate
    }

    TEST_F(InteractionTests, DisabledItemsIgnoreInput)
    {
        m_Disabled[0] = true;
        Click(Vec2(50.0f, 15.0f), Buttons());
        EXPECT_EQ(m_Clicks[0], 0);
        EXPECT_FALSE(m_Results[0].Hovered);

        // The same through the scoped form, which also dims what is drawn inside.
        int clicks = 0;
        float opacityInside = 1.0f;
        const Builder build = [&]
        {
            PushDisabled();
            PushDisabled(false); // a nested "enabled" scope cannot re-enable
            EXPECT_TRUE(IsDisabled());
            opacityInside = GetDrawList().GetOpacity();
            clicks += ButtonBehavior(Name("scoped"), Rect(0.0f, 0.0f, 100.0f, 30.0f)).Clicked ? 1 : 0;
            PopDisabled();
            PopDisabled();
            EXPECT_FALSE(IsDisabled());
        };
        Click(Vec2(50.0f, 15.0f), build);
        EXPECT_EQ(clicks, 0);
        EXPECT_FLOAT_EQ(opacityInside, GetStyleVar(StyleVar::DisabledOpacity));
    }

    TEST_F(InteractionTests, UnbalancedPushDisabledIsReported)
    {
        Frame([] { PushDisabled(); });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("Unbalanced PushDisabled"), std::string::npos);

        m_AssertMessages.clear();
        Frame([] { PopDisabled(); });
        EXPECT_EQ(m_AssertMessages.size(), 1u);
    }

    TEST_F(InteractionTests, TabMovesFocusInSubmissionOrderAndWraps)
    {
        Frame(Buttons());
        EXPECT_FALSE(GetFocusedID().IsValid());
        EXPECT_FALSE(GetIO().WantsKeyboard());

        TapKey(Key::Tab, Buttons());
        EXPECT_EQ(GetFocusedID(), Name("A"));
        EXPECT_TRUE(m_Results[0].Focused);
        EXPECT_TRUE(m_Results[0].FocusVisible); // arrived by keyboard: the ring shows
        EXPECT_TRUE(GetIO().WantsKeyboard());

        TapKey(Key::Tab, Buttons());
        EXPECT_EQ(GetFocusedID(), Name("B"));
        TapKey(Key::Tab, Buttons());
        EXPECT_EQ(GetFocusedID(), Name("C"));
        TapKey(Key::Tab, Buttons());
        EXPECT_EQ(GetFocusedID(), Name("A")); // wraps around

        TapKey(Key::LeftShift, Key::Tab, Buttons());
        EXPECT_EQ(GetFocusedID(), Name("C")); // backwards, wrapping the other way
        TapKey(Key::LeftShift, Key::Tab, Buttons());
        EXPECT_EQ(GetFocusedID(), Name("B"));
    }

    TEST_F(InteractionTests, TabSkipsDisabledItems)
    {
        m_Disabled[1] = true;
        Frame(Buttons());
        TapKey(Key::Tab, Buttons());
        TapKey(Key::Tab, Buttons());
        EXPECT_EQ(GetFocusedID(), Name("C"));
    }

    TEST_F(InteractionTests, HeldTabKeepsMoving)
    {
        Frame(Buttons());
        GetIO().AddKeyEvent(Key::Tab, true);
        Frame(Buttons());
        EXPECT_EQ(GetFocusedID(), Name("A"));

        // Held for two thirds of a second: after the repeat delay, focus keeps moving on.
        int moves = 0;
        ID previous = GetFocusedID();
        for (int i = 0; i < 40; i++)
        {
            Frame(Buttons());
            moves += GetFocusedID() != previous ? 1 : 0;
            previous = GetFocusedID();
        }
        EXPECT_GE(moves, 4);
        EXPECT_LE(moves, 7);
    }

    TEST_F(InteractionTests, SpaceAndEnterActivateTheFocusedItem)
    {
        Frame(Buttons());
        TapKey(Key::Tab, Buttons());
        TapKey(Key::Tab, Buttons());

        GetIO().AddKeyEvent(Key::Space, true);
        Frame(Buttons());
        EXPECT_EQ(m_Clicks[1], 1);
        EXPECT_TRUE(m_Results[1].Pressed); // shown as pressed while the key is down
        Frame(Buttons());
        EXPECT_EQ(m_Clicks[1], 1); // holding does not repeat
        GetIO().AddKeyEvent(Key::Space, false);
        Frame(Buttons());
        EXPECT_FALSE(m_Results[1].Pressed);

        TapKey(Key::Enter, Buttons());
        EXPECT_EQ(m_Clicks[1], 2);
        TapKey(Key::KeypadEnter, Buttons());
        EXPECT_EQ(m_Clicks[1], 3);
        EXPECT_EQ(m_Clicks[0] + m_Clicks[2], 0);
    }

    TEST_F(InteractionTests, ClickingFocusesWithoutShowingTheRing)
    {
        Click(Vec2(50.0f, 55.0f), Buttons());
        EXPECT_EQ(GetFocusedID(), Name("B"));
        EXPECT_FALSE(m_Results[1].FocusVisible);
        EXPECT_FALSE(IsFocusVisible(Name("B")));

        // The next key press reveals it, and Tab continues from the clicked item.
        TapKey(Key::Tab, Buttons());
        EXPECT_EQ(GetFocusedID(), Name("C"));
        EXPECT_TRUE(m_Results[2].FocusVisible);
    }

    TEST_F(InteractionTests, ClickingEmptySpaceClearsFocus)
    {
        Frame(Buttons());
        TapKey(Key::Tab, Buttons());
        ASSERT_TRUE(GetFocusedID().IsValid());
        Click(Vec2(400.0f, 400.0f), Buttons());
        EXPECT_FALSE(GetFocusedID().IsValid());
    }

    TEST_F(InteractionTests, FocusIsDroppedWhenTheItemDisappears)
    {
        Frame(Buttons());
        TapKey(Key::Tab, Buttons());
        ASSERT_EQ(GetFocusedID(), Name("A"));
        RunFrame();
        RunFrame();
        EXPECT_FALSE(GetFocusedID().IsValid());
    }

    TEST_F(InteractionTests, FocusCanBeSetFromCode)
    {
        Frame(Buttons());
        SetFocus(Name("C"), true);
        Frame(Buttons());
        EXPECT_TRUE(m_Results[2].Focused);
        EXPECT_TRUE(m_Results[2].FocusVisible);
        EXPECT_TRUE(IsFocused(Name("C")));

        ClearFocus();
        Frame(Buttons());
        EXPECT_FALSE(m_Results[2].Focused);
    }

    TEST_F(InteractionTests, FocusRingHidesWhileTheHostWindowIsInactive)
    {
        Frame(Buttons());
        TapKey(Key::Tab, Buttons());
        EXPECT_TRUE(m_Results[0].FocusVisible);
        GetIO().AddFocusEvent(false);
        Frame(Buttons());
        EXPECT_TRUE(m_Results[0].Focused);
        EXPECT_FALSE(m_Results[0].FocusVisible);
    }

    TEST_F(InteractionTests, EnterActivatesTheDefaultButton)
    {
        int saveClicks = 0;
        int cancelClicks = 0;
        const Builder build = [&]
        {
            cancelClicks += ButtonBehavior(Name("cancel"), Rect(0.0f, 0.0f, 80.0f, 24.0f)).Clicked ? 1 : 0;
            ButtonBehaviorOptions options;
            options.IsDefault = true;
            saveClicks += ButtonBehavior(Name("save"), Rect(90.0f, 0.0f, 80.0f, 24.0f), options).Clicked ? 1 : 0;
        };

        // Nothing focused: Enter goes to the default button.
        Frame(build);
        TapKey(Key::Enter, build);
        EXPECT_EQ(saveClicks, 1);
        EXPECT_EQ(cancelClicks, 0);

        // Another button focused: Enter activates that one instead.
        TapKey(Key::Tab, build);
        ASSERT_EQ(GetFocusedID(), Name("cancel"));
        TapKey(Key::Enter, build);
        EXPECT_EQ(cancelClicks, 1);
        EXPECT_EQ(saveClicks, 1);

        // Space never triggers the default button.
        ClearFocus();
        TapKey(Key::Space, build);
        EXPECT_EQ(saveClicks, 1);
    }

    TEST_F(InteractionTests, DragCapturesThePointer)
    {
        DragInteraction drag;
        bool started = false;
        bool ended = false;
        const Builder build = [&]
        {
            drag = DragBehavior(Name("handle"), Rect(100.0f, 100.0f, 20.0f, 20.0f));
            started = started || drag.Started;
            ended = ended || drag.Ended;
        };

        MoveMouse(Vec2(110.0f, 110.0f), build);
        EXPECT_TRUE(drag.Hovered);
        PressMouse(build);
        EXPECT_TRUE(started);
        EXPECT_TRUE(drag.Active);
        EXPECT_EQ(drag.Delta, Vec2(0.0f, 0.0f));

        // Far outside the handle: still dragging.
        GetIO().AddMousePosEvent(300.0f, 150.0f);
        Frame(build);
        EXPECT_TRUE(drag.Active);
        EXPECT_EQ(drag.Delta, Vec2(190.0f, 40.0f));
        EXPECT_EQ(drag.Total, Vec2(190.0f, 40.0f));
        EXPECT_EQ(drag.Position, Vec2(300.0f, 150.0f));

        GetIO().AddMousePosEvent(310.0f, 150.0f);
        Frame(build);
        EXPECT_EQ(drag.Delta, Vec2(10.0f, 0.0f));
        EXPECT_EQ(drag.Total, Vec2(200.0f, 40.0f));

        ReleaseMouse(build);
        EXPECT_TRUE(ended);
        EXPECT_FALSE(drag.Active);
        Frame(build);
        EXPECT_FALSE(drag.Active);
    }

    TEST_F(InteractionTests, AnItemThatVanishesReleasesThePointer)
    {
        bool showFirst = true;
        Interaction second;
        const Builder build = [&]
        {
            if (showFirst)
                ButtonBehavior(Name("first"), Rect(0.0f, 0.0f, 100.0f, 30.0f));
            second = ButtonBehavior(Name("second"), Rect(0.0f, 40.0f, 100.0f, 30.0f));
        };
        MoveMouse(Vec2(50.0f, 15.0f), build);
        PressMouse(build);
        ASSERT_EQ(GetActiveID(), Name("first"));

        showFirst = false;
        Frame(build);
        EXPECT_FALSE(GetActiveID().IsValid());
        MoveMouse(Vec2(50.0f, 55.0f), build);
        EXPECT_TRUE(second.Hovered);
    }

    TEST_F(InteractionTests, LastItemQueries)
    {
        const Builder build = [&] { BuildButtons(); };
        MoveMouse(Vec2(50.0f, 95.0f), build);
        Frame(
            [&]
            {
                BuildButtons(); // the last item is C, under the pointer
                EXPECT_TRUE(IsItemHovered());
                EXPECT_FALSE(IsItemFocused());
                EXPECT_FALSE(IsItemActive());
                EXPECT_EQ(GetItemID(), Name("C"));
                EXPECT_EQ(GetItemRect(), Rect(0.0f, 80.0f, 100.0f, 30.0f));
            });
    }

    TEST_F(InteractionTests, FocusRingAnimatesInAndFollowsTheControl)
    {
        const Rect control(20.0f, 20.0f, 80.0f, 24.0f);
        const Builder build = [&]
        {
            ButtonBehavior(Name("ring"), control);
            DrawFocusRing(Name("ring"), control, 6.0f);
        };
        Frame(build);
        EXPECT_TRUE(GetDrawData().Vertices.empty()); // no focus, no ring

        GetIO().AddKeyEvent(Key::Tab, true);
        Frame(build);
        Frame(build);
        ASSERT_EQ(GetDrawData().Vertices.size(), 4u);
        const float earlyAlpha = GetQuadColor(0).A;
        EXPECT_GT(earlyAlpha, 0.0f);
        EXPECT_TRUE(IsAnimating());

        Settle(build, 40);
        const DrawPrimitive& ring = GetPrimitive(0);
        EXPECT_EQ(ring.Kind, DrawPrimitiveKind::SquircleStroke);
        EXPECT_GT(GetQuadColor(0).A, earlyAlpha);
        // Settled: the ring's outer edge is offset + width outside the control, corners concentric.
        const float outset = GetStyleVar(StyleVar::FocusRingOffset) + GetStyleVar(StyleVar::FocusRingWidth);
        EXPECT_NEAR(ring.HalfSize.X, 40.0f + outset, 0.05f);
        EXPECT_NEAR(ring.Radius, 6.0f + outset, 0.05f);
        EXPECT_FLOAT_EQ(ring.StrokeWidth, GetStyleVar(StyleVar::FocusRingWidth));
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(InteractionTests, FocusMovingIntoAScrollViewRevealsTheItem)
    {
        const Builder build = [&]
        {
            BeginScrollView("list", {.Width = 200.0f, .Height = 100.0f, .Spacing = 0.0f});
            for (int i = 0; i < 10; i++)
                ButtonBehavior(GetID(i), AllocateItem(Vec2(180.0f, 30.0f)));
            EndScrollView();
        };
        Settle(build);

        // Tab six times: the sixth row (y = 150..180) is far below the 100-point viewport.
        for (int i = 0; i < 6; i++)
            TapKey(Key::Tab, build);
        Settle(build, 60);
        const float offset = GetScrollOffset("list").Y;
        EXPECT_GT(offset, 80.0f - 1.0f);  // row bottom (180) is inside the viewport
        EXPECT_LT(offset, 150.0f + 1.0f); // row top (150) is too
    }

    TEST_F(InteractionTests, CursorRequestsReachTheHost)
    {
        std::vector<Cursor> shown;
        Context* context = nullptr;
        ContextDescription description;
        description.Callbacks.SetCursor = [&](Cursor cursor) { shown.push_back(cursor); };
        context = CreateContext(description);
        SetCurrentContext(context);
        GetIO().SetDisplaySize(400.0f, 300.0f);

        bool wantsIBeam = false;
        const Builder build = [&]
        {
            if (wantsIBeam)
                SetCursor(Cursor::IBeam);
        };
        Frame(build);
        EXPECT_TRUE(shown.empty()); // the arrow is the default: nothing to report

        wantsIBeam = true;
        Frame(build);
        Frame(build);
        ASSERT_EQ(shown.size(), 1u); // reported once, not every frame
        EXPECT_EQ(shown[0], Cursor::IBeam);

        wantsIBeam = false;
        Frame(build);
        ASSERT_EQ(shown.size(), 2u);
        EXPECT_EQ(shown[1], Cursor::Arrow);

        SetCurrentContext(m_Context);
        DestroyContext(context);
    }
} // namespace Carbon
