#include <Carbon/Extensions/Extensions.h>

#include "Support/WidgetTest.h"

namespace Carbon
{
    // ---- Toolbar ------------------------------------------------------------------------------------------------

    class ToolbarTests : public WidgetTest
    {
    protected:
        enum class Middle
        {
            Nothing,
            Space,
            FlexibleSpace,
            Separator
        };

        // New and Delete with something between them, then Share, Info and a control.
        Builder Interface()
        {
            return [this]
            {
                BeginToolbar("bar", m_Options);
                m_Clicks[0] += ToolbarItem("New", {.Icon = Icons::Plus}) ? 1 : 0;
                Record(0, "New");
                switch (m_Middle)
                {
                    case Middle::Nothing:
                        break;
                    case Middle::Space:
                        ToolbarSpace();
                        break;
                    case Middle::FlexibleSpace:
                        ToolbarFlexibleSpace();
                        break;
                    case Middle::Separator:
                        ToolbarSeparator();
                        break;
                }
                m_Clicks[1] += ToolbarItem("Delete", {.Icon = Icons::Trash, .Disabled = m_IsDeleteDisabled}) ? 1 : 0;
                Record(1, "Delete");
                if (m_HasMore)
                {
                    m_Clicks[2] += ToolbarItem("Share", {.Icon = Icons::Export}) ? 1 : 0;
                    Record(2, "Share");
                    m_Clicks[3] += ToolbarItem("Info", {.Icon = Icons::Info}) ? 1 : 0;
                    Record(3, "Info");
                }
                m_IsControlBuilt = false;
                if (m_HasControl && BeginToolbarControl("Search", {.Icon = Icons::MagnifyingGlass}))
                {
                    m_IsControlBuilt = true;
                    Button("Inside", {.Width = 100.0f});
                    m_InsideID = GetItemID();
                    EndToolbarControl();
                }
                EndToolbar();
                m_Bar = GetLastItemRect();
            };
        }

        void Record(int index, std::string_view label)
        {
            m_Visible[index] = GetItemID() == GetID(label);
            if (m_Visible[index])
                m_Rects[index] = GetItemRect();
        }

        ID GetOverflowMenu() { return HashID("##overflowmenu", GetID("bar")); }

        ToolbarOptions m_Options = {.DisplayMode = ToolbarDisplayMode::IconOnly};
        Middle m_Middle = Middle::Nothing;
        bool m_HasMore = false;
        bool m_HasControl = false;
        bool m_IsDeleteDisabled = false;
        bool m_IsControlBuilt = false;
        int m_Clicks[4] = {};
        bool m_Visible[4] = {};
        Rect m_Rects[4];
        Rect m_Bar;
        ID m_InsideID;
    };

    TEST_F(ToolbarTests, ItemsActivateByClickAndKeyboard)
    {
        Settle(Interface());
        EXPECT_EQ(m_Bar, Rect(0.0f, 0.0f, 800.0f, 38.0f));
        EXPECT_EQ(m_Rects[0], Rect(8.0f, 5.0f, 36.0f, 28.0f));
        Click(m_Rects[0].GetCenter(), Interface());
        EXPECT_EQ(m_Clicks[0], 1);

        // Every item is a stop for Tab.
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Clicks[0], 2);
        TapKey(Key::Tab, Interface());
        TapKey(Key::Space, Interface());
        EXPECT_EQ(m_Clicks[1], 1);
    }

    TEST_F(ToolbarTests, DisabledItemsIgnoreInput)
    {
        m_IsDeleteDisabled = true;
        Settle(Interface());
        Click(m_Rects[1].GetCenter(), Interface());
        EXPECT_EQ(m_Clicks[1], 0);
        TapKey(Key::Tab, Interface());
        TapKey(Key::Tab, Interface());
        EXPECT_EQ(GetFocusedID(), GetID("New")) << "Tab skips the disabled item and wraps around";
    }

    TEST_F(ToolbarTests, EachDisplayModeHasItsLayout)
    {
        m_Options.DisplayMode = ToolbarDisplayMode::IconAndLabel;
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_Bar.Height, 52.0f);
        const float labelLine = GetTextSpec(TextStyle::Subheadline).LineHeight;
        EXPECT_FLOAT_EQ(m_Rects[0].Height, 28.0f + 2.0f + labelLine);
        EXPECT_GE(m_Rects[1].Width, 36.0f);

        m_Options.DisplayMode = ToolbarDisplayMode::IconOnly;
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_Bar.Height, 38.0f);
        EXPECT_EQ(m_Rects[1], Rect(48.0f, 5.0f, 36.0f, 28.0f));

        m_Options.DisplayMode = ToolbarDisplayMode::LabelOnly;
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_Bar.Height, 38.0f);
        EXPECT_FLOAT_EQ(m_Rects[0].Width, MeasureText("New", GetTextSpec(TextStyle::Body)).X + 20.0f);
        EXPECT_FLOAT_EQ(m_Rects[0].Height, 24.0f);

        m_Options.Height = 60.0f;
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_Bar.Height, 60.0f);
    }

    TEST_F(ToolbarTests, SpacesAndSeparatorsSitBetweenItems)
    {
        m_Middle = Middle::Space;
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_Rects[1].X, 8.0f + 36.0f + 4.0f + 8.0f + 4.0f);

        m_Middle = Middle::Separator;
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_Rects[1].X, 8.0f + 36.0f + 4.0f + 9.0f + 4.0f);

        m_Middle = Middle::FlexibleSpace;
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_Rects[1].GetRight(), 800.0f - 8.0f) << "a flexible space pushes Delete to the end";
    }

    TEST_F(ToolbarTests, ItemsThatDoNotFitMoveIntoTheOverflowMenu)
    {
        m_HasMore = true;
        m_Options.Width = 120.0f;
        Settle(Interface());
        // 104 points inside: New and the chevron fit, the rest goes into the menu.
        EXPECT_TRUE(m_Visible[0]);
        EXPECT_FALSE(m_Visible[1]);
        EXPECT_FALSE(m_Visible[3]);
        EXPECT_FALSE(IsAnimating());

        // The chevron sits at the trailing end and opens the menu; its first entry is Delete.
        Click(Vec2(120.0f - 8.0f - 12.0f, 19.0f), Interface());
        ASSERT_TRUE(IsOverlayOpen(GetOverflowMenu()));
        Settle(Interface());
        Click(Vec2(80.0f, 5.0f + 28.0f + 2.0f + 5.0f + 11.0f), Interface());
        EXPECT_FALSE(IsOverlayOpen(GetOverflowMenu()));
        Frame(Interface());
        EXPECT_EQ(m_Clicks[1], 1) << "the item reports the choice in the next frame";
        Frame(Interface());
        EXPECT_EQ(m_Clicks[1], 1) << "and only once";

        // Wider again: everything is back in the bar.
        m_Options.Width = Size::Fill();
        Settle(Interface());
        EXPECT_TRUE(m_Visible[1]);
        EXPECT_TRUE(m_Visible[3]);
    }

    TEST_F(ToolbarTests, AControlInTheOverflowMenuOpensInAPopover)
    {
        m_HasControl = true;
        m_Options.Width = 120.0f;
        Settle(Interface());
        EXPECT_FALSE(m_IsControlBuilt);

        Click(Vec2(120.0f - 8.0f - 12.0f, 19.0f), Interface());
        Settle(Interface());
        // Delete, then Search.
        Click(Vec2(80.0f, 5.0f + 28.0f + 2.0f + 5.0f + 22.0f + 11.0f), Interface());
        Settle(Interface(), 4);
        EXPECT_TRUE(m_IsControlBuilt);
        EXPECT_EQ(GetFocusedID(), m_InsideID) << "the control gets the keyboard";

        TapKey(Key::Escape, Interface());
        Settle(Interface(), 3);
        EXPECT_FALSE(m_IsControlBuilt);
    }

    TEST_F(ToolbarTests, ControlsAreBuiltInTheBarWhenTheyFit)
    {
        m_HasControl = true;
        Settle(Interface());
        EXPECT_TRUE(m_IsControlBuilt);
        EXPECT_TRUE(m_AssertMessages.empty());
    }
} // namespace Carbon
