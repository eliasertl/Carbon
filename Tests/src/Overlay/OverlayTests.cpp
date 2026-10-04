#include "Support/WidgetTest.h"

namespace Carbon
{
    class OverlayTests : public WidgetTest
    {
    protected:
        static ID Popup() { return HashID("popup"); }
        static ID Upper() { return HashID("upper"); }
        static ID Under() { return HashID("under"); }

        // A button that covers the whole display, and above it an overlay with two 120 x 30 buttons. With the
        // default padding of 12 and spacing of 8 the overlay is 144 x 92.
        void Build()
        {
            ButtonBehaviorOptions underOptions;
            underOptions.IsDefault = true;
            m_Under = ButtonBehavior(Under(), Rect(0.0f, 0.0f, 800.0f, 600.0f), underOptions);
            m_UnderClicks += m_Under.Clicked ? 1 : 0;

            m_IsShown = false;
            if (BeginOverlay(Popup(), m_Options))
            {
                m_IsShown = true;
                m_ItemRect = AllocateItem(Vec2(120.0f, 30.0f));
                m_ItemID = GetID("item");
                m_Item = ButtonBehavior(m_ItemID, m_ItemRect);
                m_ItemClicks += m_Item.Clicked ? 1 : 0;
                m_SecondID = GetID("second");
                m_Second = ButtonBehavior(m_SecondID, AllocateItem(Vec2(120.0f, 30.0f)));
                if (m_ClosesItself)
                    CloseCurrentOverlay();
                EndOverlay();
            }
            m_OverlayIndexCount = GetDrawList().GetIndices(DrawLayer::Overlay, 0).size();

            if (m_BuildsUpper)
            {
                OverlayOptions upper;
                upper.Placement = OverlayPlacement::Center;
                if (BeginOverlay(Upper(), upper))
                {
                    m_UpperID = GetID("upper item");
                    ButtonBehavior(m_UpperID, AllocateItem(Vec2(50.0f, 20.0f)));
                    EndOverlay();
                }
            }
        }

        Builder Interface()
        {
            return [this] { Build(); };
        }

        void SetUp() override
        {
            WidgetTest::SetUp();
            m_Options.Anchor = Rect(100.0f, 100.0f, 80.0f, 24.0f);
        }

        OverlayOptions m_Options;
        Interaction m_Under;
        Interaction m_Item;
        Interaction m_Second;
        Rect m_ItemRect;
        ID m_ItemID;
        ID m_SecondID;
        ID m_UpperID;
        int m_UnderClicks = 0;
        int m_ItemClicks = 0;
        bool m_IsShown = false;
        bool m_BuildsUpper = false;
        bool m_ClosesItself = false;
        size_t m_OverlayIndexCount = 0;
    };

    TEST_F(OverlayTests, ShowsOnlyWhileOpen)
    {
        Frame(Interface());
        EXPECT_FALSE(m_IsShown);
        EXPECT_FALSE(IsOverlayOpen(Popup()));
        EXPECT_FALSE(IsAnyOverlayOpen());

        OpenOverlay(Popup());
        EXPECT_TRUE(IsOverlayOpen(Popup()));
        Frame(Interface());
        EXPECT_TRUE(m_IsShown);
        EXPECT_TRUE(IsAnyOverlayOpen());

        CloseOverlay(Popup());
        Frame(Interface());
        EXPECT_FALSE(m_IsShown);
    }

    TEST_F(OverlayTests, IsHiddenWhileItsSizeIsUnknown)
    {
        OpenOverlay(Popup());
        // The first frame only measures: nothing is drawn, but another frame is requested.
        Frame(Interface());
        EXPECT_EQ(m_OverlayIndexCount, 0u);
        EXPECT_TRUE(IsAnimating());

        Settle(Interface());
        EXPECT_GT(m_OverlayIndexCount, 0u);
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(OverlayTests, SitsBelowItsAnchor)
    {
        OpenOverlay(Popup());
        Settle(Interface());
        // Anchor bottom 124 + gap 4, then 12 points of padding.
        EXPECT_EQ(m_ItemRect, Rect(112.0f, 140.0f, 120.0f, 30.0f));
    }

    TEST_F(OverlayTests, AlignsAlongTheAnchor)
    {
        m_Options.Alignment = Alignment::Trailing;
        OpenOverlay(Popup());
        Settle(Interface());
        // The overlay is 144 wide and ends where the anchor ends, at x = 180.
        EXPECT_FLOAT_EQ(m_ItemRect.X, 180.0f - 144.0f + 12.0f);

        m_Options.Alignment = Alignment::Center;
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_ItemRect.X, 140.0f - 72.0f + 12.0f);
    }

    TEST_F(OverlayTests, FlipsWhenThereIsNoRoom)
    {
        m_Options.Anchor = Rect(100.0f, 540.0f, 80.0f, 24.0f);
        OpenOverlay(Popup());
        Settle(Interface());
        // Above the anchor: the overlay's bottom edge is one gap above it.
        EXPECT_FLOAT_EQ(m_ItemRect.Y, 540.0f - 4.0f - 92.0f + 12.0f);

        m_Options.Anchor = Rect(700.0f, 100.0f, 80.0f, 24.0f);
        m_Options.Placement = OverlayPlacement::Trailing;
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_ItemRect.X, 700.0f - 4.0f - 144.0f + 12.0f);
    }

    TEST_F(OverlayTests, StaysOnTheDisplay)
    {
        m_Options.Anchor = Rect(760.0f, 100.0f, 30.0f, 24.0f);
        OpenOverlay(Popup());
        Settle(Interface());
        // Pushed left so that it keeps a margin of 8 points to the display's edge.
        EXPECT_FLOAT_EQ(m_ItemRect.X, 800.0f - 8.0f - 144.0f + 12.0f);
    }

    TEST_F(OverlayTests, CenterAndTopIgnoreTheAnchor)
    {
        SetReduceMotion(true);
        m_Options.Placement = OverlayPlacement::Center;
        OpenOverlay(Popup());
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_ItemRect.X, (800.0f - 144.0f) * 0.5f + 12.0f);
        EXPECT_FLOAT_EQ(m_ItemRect.Y, (600.0f - 92.0f) * 0.5f + 12.0f);

        m_Options.Placement = OverlayPlacement::Top;
        m_Options.Gap = 20.0f;
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_ItemRect.Y, 20.0f + 12.0f);
    }

    TEST_F(OverlayTests, ItemsInsideReceiveClicks)
    {
        OpenOverlay(Popup());
        Settle(Interface());
        Click(m_ItemRect.GetCenter(), Interface());
        EXPECT_EQ(m_ItemClicks, 1);
        EXPECT_EQ(m_UnderClicks, 0);
        EXPECT_TRUE(m_IsShown);
    }

    TEST_F(OverlayTests, CoversWhatLiesBeneath)
    {
        MoveMouse(Vec2(120.0f, 135.0f), Interface());
        EXPECT_TRUE(m_Under.Hovered);

        OpenOverlay(Popup());
        Settle(Interface());
        // The pointer is over the overlay's padding: neither the item nor the button beneath is hovered.
        EXPECT_FALSE(m_Under.Hovered);
        EXPECT_FALSE(m_Item.Hovered);
        EXPECT_EQ(GetHoveredID(), Popup());

        Click(Vec2(120.0f, 135.0f), Interface());
        EXPECT_EQ(m_UnderClicks, 0);
        EXPECT_TRUE(m_IsShown);
    }

    TEST_F(OverlayTests, OutsideClickDismissesAndIsUsedUp)
    {
        OpenOverlay(Popup());
        Settle(Interface());
        Click(Vec2(600.0f, 400.0f), Interface());
        EXPECT_FALSE(m_IsShown);
        EXPECT_EQ(m_UnderClicks, 0);

        // The next click reaches the button again.
        Click(Vec2(600.0f, 400.0f), Interface());
        EXPECT_EQ(m_UnderClicks, 1);
    }

    TEST_F(OverlayTests, RightClickOutsideDismisses)
    {
        OpenOverlay(Popup());
        Settle(Interface());
        MoveMouse(Vec2(600.0f, 400.0f), Interface());
        PressMouse(Interface(), MouseButton::Right);
        EXPECT_FALSE(m_IsShown);
        ReleaseMouse(Interface(), MouseButton::Right);
    }

    TEST_F(OverlayTests, ModalOverlayIgnoresOutsideClicks)
    {
        m_Options.IsModal = true;
        OpenOverlay(Popup());
        Settle(Interface());
        Click(Vec2(600.0f, 400.0f), Interface());
        EXPECT_TRUE(m_IsShown);
        EXPECT_EQ(m_UnderClicks, 0);
        EXPECT_FALSE(m_Under.Hovered);
    }

    TEST_F(OverlayTests, NonCapturingOverlayLeavesTheRestUsable)
    {
        m_Options.DismissOnOutsideClick = false;
        OpenOverlay(Popup());
        Settle(Interface());
        Click(Vec2(600.0f, 400.0f), Interface());
        EXPECT_TRUE(m_IsShown);
        EXPECT_EQ(m_UnderClicks, 1);

        // Its own surface still covers what is beneath it.
        Click(Vec2(120.0f, 135.0f), Interface());
        EXPECT_EQ(m_UnderClicks, 1);
    }

    TEST_F(OverlayTests, EscapeClosesTheTopmostOverlayOnly)
    {
        m_BuildsUpper = true;
        OpenOverlay(Popup());
        OpenOverlay(Upper());
        Settle(Interface());
        EXPECT_TRUE(IsOverlayOpen(Upper()));

        TapKey(Key::Escape, Interface());
        EXPECT_FALSE(IsOverlayOpen(Upper()));
        EXPECT_TRUE(IsOverlayOpen(Popup()));

        TapKey(Key::Escape, Interface());
        EXPECT_FALSE(IsOverlayOpen(Popup()));
    }

    TEST_F(OverlayTests, EscapeCanBeTurnedOff)
    {
        m_Options.DismissOnEscape = false;
        OpenOverlay(Popup());
        Settle(Interface());
        TapKey(Key::Escape, Interface());
        EXPECT_TRUE(m_IsShown);
    }

    TEST_F(OverlayTests, ClosingAnOverlayClosesTheOnesAboveIt)
    {
        m_BuildsUpper = true;
        OpenOverlay(Popup());
        OpenOverlay(Upper());
        Settle(Interface());

        CloseOverlay(Popup());
        EXPECT_FALSE(IsOverlayOpen(Popup()));
        EXPECT_FALSE(IsOverlayOpen(Upper()));
    }

    TEST_F(OverlayTests, LaterOverlaysDrawAboveEarlierOnes)
    {
        m_BuildsUpper = true;
        OpenOverlay(Popup());
        OpenOverlay(Upper());
        size_t lower = 0;
        size_t upper = 0;
        Settle(
            [&]
            {
                Build();
                lower = GetDrawList().GetIndices(DrawLayer::Overlay, 0).size();
                upper = GetDrawList().GetIndices(DrawLayer::Overlay, 1).size();
            });
        EXPECT_GT(lower, 0u);
        EXPECT_GT(upper, 0u);

        // The upper overlay is in the middle of the display, away from the lower one, and takes clicks from it.
        Click(m_ItemRect.GetCenter(), Interface());
        EXPECT_EQ(m_ItemClicks, 0);
        EXPECT_FALSE(IsOverlayOpen(Upper()));
        EXPECT_TRUE(IsOverlayOpen(Popup()));
    }

    TEST_F(OverlayTests, AnOverlayThatIsNotSubmittedCloses)
    {
        OpenOverlay(Popup());
        Settle(Interface());
        RunFrame();
        EXPECT_FALSE(IsOverlayOpen(Popup()));
    }

    TEST_F(OverlayTests, AnOverlayOpenedLateInAFrameSurvivesIt)
    {
        Frame(
            [this]
            {
                Build();
                OpenOverlay(Popup());
            });
        EXPECT_TRUE(IsOverlayOpen(Popup()));
        Frame(Interface());
        EXPECT_TRUE(m_IsShown);
    }

    TEST_F(OverlayTests, CanCloseItselfWhileBuilding)
    {
        OpenOverlay(Popup());
        Settle(Interface());
        m_ClosesItself = true;
        Frame(Interface());
        EXPECT_FALSE(IsOverlayOpen(Popup()));
        Frame(Interface());
        EXPECT_FALSE(m_IsShown);
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(OverlayTests, TabStaysInsideTheOverlay)
    {
        Frame(Interface());
        SetFocus(Under(), true);
        Frame(Interface());
        ASSERT_EQ(GetFocusedID(), Under());

        OpenOverlay(Popup());
        Settle(Interface());
        EXPECT_FALSE(GetFocusedID().IsValid());

        TapKey(Key::Tab, Interface());
        EXPECT_EQ(GetFocusedID(), m_ItemID);
        TapKey(Key::Tab, Interface());
        EXPECT_EQ(GetFocusedID(), m_SecondID);
        TapKey(Key::Tab, Interface());
        EXPECT_EQ(GetFocusedID(), m_ItemID);
        TapKey(Key::LeftShift, Key::Tab, Interface());
        EXPECT_EQ(GetFocusedID(), m_SecondID);

        // Closing gives focus back to where it was.
        TapKey(Key::Escape, Interface());
        EXPECT_FALSE(m_IsShown);
        EXPECT_EQ(GetFocusedID(), Under());
        EXPECT_TRUE(IsFocusVisible(Under()));
    }

    TEST_F(OverlayTests, FocusNextAndPreviousStepLikeTab)
    {
        OpenOverlay(Popup());
        Settle(Interface());
        FocusNext();
        Frame(Interface());
        EXPECT_EQ(GetFocusedID(), m_ItemID);
        FocusNext();
        Frame(Interface());
        EXPECT_EQ(GetFocusedID(), m_SecondID);
        FocusPrevious();
        Frame(Interface());
        EXPECT_EQ(GetFocusedID(), m_ItemID);
    }

    TEST_F(OverlayTests, DefaultButtonBeneathDoesNotGetEnter)
    {
        Settle(Interface());
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_UnderClicks, 1);

        OpenOverlay(Popup());
        Settle(Interface());
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_UnderClicks, 1);
    }

    TEST_F(OverlayTests, WantsTheMouseWhileItHoldsThePointer)
    {
        Frame(Interface());
        OpenOverlay(Popup());
        m_Options.IsModal = true;
        MoveMouse(Vec2(700.0f, 500.0f),
                  [this]
                  {
                      if (BeginOverlay(Popup(), m_Options))
                          EndOverlay();
                  });
        EXPECT_TRUE(GetIO().WantsMouse());
    }

    TEST_F(OverlayTests, ScrimDimsTheDisplay)
    {
        m_Options.HasScrim = true;
        m_Options.IsModal = true;
        OpenOverlay(Popup());
        size_t firstIndex = 0;
        Settle(
            [&]
            {
                Build();
                const std::span<const DrawIndex> indices = GetDrawList().GetIndices(DrawLayer::Overlay, 0);
                firstIndex = indices.empty() ? 0 : indices[0];
            });
        // The first shape of the overlay's layer covers the display.
        const DrawData& drawData = GetDrawData();
        EXPECT_LE(drawData.Vertices[firstIndex].Position.X, 0.0f);
        EXPECT_GE(drawData.Vertices[firstIndex + 2].Position.X, 800.0f);
        EXPECT_GE(drawData.Vertices[firstIndex + 2].Position.Y, 600.0f);
    }

    TEST_F(OverlayTests, EndWithoutBeginIsReported)
    {
        Frame([] { EndOverlay(); });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("EndOverlay"), std::string::npos);
    }

    TEST_F(OverlayTests, MissingEndIsReportedOnceAndRecovered)
    {
        OpenOverlay(Popup());
        Frame([this] { BeginOverlay(Popup(), m_Options); });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("Unbalanced layout"), std::string::npos);

        Frame(Interface());
        EXPECT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_TRUE(m_IsShown);
    }

    TEST_F(OverlayTests, ScrollViewBeneathAModalOverlayDoesNotScroll)
    {
        m_Options.IsModal = true;
        const Builder build = [this]
        {
            BeginScrollView("scroll", {.Width = 300.0f, .Height = 200.0f});
            AllocateItem(Vec2(100.0f, 1000.0f));
            EndScrollView();
            if (BeginOverlay(Popup(), m_Options))
            {
                AllocateItem(Vec2(50.0f, 20.0f));
                EndOverlay();
            }
        };
        Settle(build);
        MoveMouse(Vec2(250.0f, 150.0f), build);
        GetIO().AddMouseWheelEvent(0.0f, -1.0f);
        Settle(build);
        EXPECT_GT(GetScrollOffset("scroll").Y, 0.0f);

        SetScrollOffset("scroll", Vec2(), false);
        OpenOverlay(Popup());
        Settle(build);
        GetIO().AddMouseWheelEvent(0.0f, -1.0f);
        Settle(build);
        EXPECT_EQ(GetScrollOffset("scroll").Y, 0.0f);
    }
} // namespace Carbon
