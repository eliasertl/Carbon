#include <Carbon/Extensions/Extensions.h>

#include <format>
#include <string>
#include <vector>

#include "Support/WidgetTest.h"

namespace Carbon
{
    // ---- MenuBar ------------------------------------------------------------------------------------------------

    class MenuBarTests : public WidgetTest
    {
    protected:
        // File (New, Open), Edit (Copy), Help (disabled).
        Builder Interface()
        {
            return [this]
            {
                m_OpenMenu.clear();
                BeginMenuBar();
                const bool isFileOpen = BeginMenuBarMenu("File");
                m_Titles[0] = GetItemRect();
                if (isFileOpen)
                {
                    m_OpenMenu = "File";
                    if (MenuItem("New"))
                        m_Chosen = "New";
                    m_FirstItems[0] = GetItemID();
                    if (MenuItem("Open"))
                        m_Chosen = "Open";
                    m_ItemRect = GetItemRect();
                    EndMenuBarMenu();
                }
                const bool isEditOpen = BeginMenuBarMenu("Edit");
                m_Titles[1] = GetItemRect();
                if (isEditOpen)
                {
                    m_OpenMenu = "Edit";
                    if (MenuItem("Copy"))
                        m_Chosen = "Copy";
                    m_FirstItems[1] = GetItemID();
                    EndMenuBarMenu();
                }
                const bool isHelpOpen = BeginMenuBarMenu("Help", {.Disabled = m_IsHelpDisabled});
                m_Titles[2] = GetItemRect();
                if (isHelpOpen)
                {
                    m_OpenMenu = "Help";
                    MenuItem("Help Topics");
                    EndMenuBarMenu();
                }
                EndMenuBar();
                m_Bar = GetLastItemRect();
            };
        }

        Rect m_Titles[3];
        Rect m_Bar;
        Rect m_ItemRect;
        ID m_FirstItems[2];
        std::string m_OpenMenu;
        std::string m_Chosen;
        bool m_IsHelpDisabled = false;
    };

    TEST_F(MenuBarTests, SpansTheWindowAtTheTop)
    {
        Settle(Interface());
        EXPECT_EQ(m_Bar, Rect(0.0f, 0.0f, 800.0f, 28.0f));
        EXPECT_FLOAT_EQ(m_Titles[0].X, 6.0f);
        EXPECT_NEAR(m_Titles[1].X, m_Titles[0].GetRight(), 0.01f);
    }

    TEST_F(MenuBarTests, PressOpensAMenuAndChoosingClosesIt)
    {
        Settle(Interface());
        MoveMouse(m_Titles[0].GetCenter(), Interface());
        PressMouse(Interface());
        ReleaseMouse(Interface());
        EXPECT_EQ(m_OpenMenu, "File");
        Settle(Interface());
        EXPECT_GT(m_ItemRect.Y, m_Titles[0].Y) << "the menu hangs below its title";

        Click(m_ItemRect.GetCenter(), Interface());
        EXPECT_EQ(m_Chosen, "Open");
        Frame(Interface());
        EXPECT_TRUE(m_OpenMenu.empty());
    }

    TEST_F(MenuBarTests, PressingTheOpenTitleClosesItsMenu)
    {
        Settle(Interface());
        Click(m_Titles[0].GetCenter(), Interface());
        Settle(Interface());
        ASSERT_EQ(m_OpenMenu, "File");
        Click(m_Titles[0].GetCenter(), Interface());
        Frame(Interface());
        EXPECT_TRUE(m_OpenMenu.empty());
    }

    TEST_F(MenuBarTests, MenusFollowThePointerWhileOneIsOpen)
    {
        Settle(Interface());
        Click(m_Titles[0].GetCenter(), Interface());
        Settle(Interface());
        MoveMouse(m_Titles[1].GetCenter(), Interface());
        Frame(Interface());
        EXPECT_EQ(m_OpenMenu, "Edit");

        // Disabled menus are passed over.
        m_IsHelpDisabled = true;
        MoveMouse(m_Titles[2].GetCenter(), Interface());
        Frame(Interface());
        EXPECT_EQ(m_OpenMenu, "Edit");

        // With no menu open, the pointer alone opens nothing.
        TapKey(Key::Escape, Interface());
        Frame(Interface());
        EXPECT_TRUE(m_OpenMenu.empty());
        MoveMouse(m_Titles[0].GetCenter(), Interface());
        EXPECT_TRUE(m_OpenMenu.empty());
    }

    TEST_F(MenuBarTests, F10OpensTheFirstMenuAndArrowsMoveAlongTheBar)
    {
        Settle(Interface());
        TapKey(Key::F10, Interface());
        Settle(Interface(), 3);
        EXPECT_EQ(m_OpenMenu, "File");
        EXPECT_EQ(GetFocusedID(), m_FirstItems[0]) << "the first item is highlighted";

        TapKey(Key::RightArrow, Interface());
        Settle(Interface(), 3);
        EXPECT_EQ(m_OpenMenu, "Edit");
        EXPECT_EQ(GetFocusedID(), m_FirstItems[1]);

        TapKey(Key::LeftArrow, Interface());
        Settle(Interface(), 3);
        EXPECT_EQ(m_OpenMenu, "File");
        TapKey(Key::LeftArrow, Interface());
        Settle(Interface(), 3);
        EXPECT_EQ(m_OpenMenu, "Help") << "the bar wraps around";

        TapKey(Key::Escape, Interface());
        Frame(Interface());
        EXPECT_TRUE(m_OpenMenu.empty());
    }

    TEST_F(MenuBarTests, AltOnItsOwnOpensTheFirstMenu)
    {
        Settle(Interface());
        TapKey(Key::LeftAlt, Interface());
        Settle(Interface(), 3);
        EXPECT_EQ(m_OpenMenu, "File");
        TapKey(Key::Escape, Interface());
        Frame(Interface());

        // Alt as part of a shortcut does not.
        TapKey(Key::LeftAlt, Key::S, Interface());
        Settle(Interface(), 3);
        EXPECT_TRUE(m_OpenMenu.empty());
    }

    // ---- ComboBox -----------------------------------------------------------------------------------------------

    class ComboBoxTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                static const std::string_view Items[] = {"Berlin", "Bern", "London", "Paris"};
                if (ComboBox("City", &m_Text, Items, {.Width = 200.0f}))
                    m_Changes++;
                m_Rect = GetItemRect();
                m_Submitted = m_Submitted || IsItemSubmitted();
                m_IsOpen = IsOverlayOpen(HashID("##list", GetID("City")));
            };
        }

        // The button sits inside the field's trailing edge.
        Vec2 GetButton() const { return Vec2(m_Rect.GetRight() - 12.0f, m_Rect.GetCenter().Y); }
        // Rows of the list, which hangs 2 points below the field with 5 points of padding.
        Vec2 GetRow(int row) const
        {
            return Vec2(m_Rect.GetCenter().X,
                        m_Rect.GetBottom() + 2.0f + 5.0f + 22.0f * static_cast<float>(row) + 11.0f);
        }

        std::string m_Text;
        int m_Changes = 0;
        Rect m_Rect;
        bool m_IsOpen = false;
        bool m_Submitted = false;
    };

    TEST_F(ComboBoxTests, ButtonOpensTheListAndPickingSetsTheText)
    {
        Settle(Interface());
        EXPECT_EQ(m_Rect.GetSize(), Vec2(200.0f, 24.0f));
        Click(GetButton(), Interface());
        EXPECT_TRUE(m_IsOpen);
        EXPECT_TRUE(IsFocused(GetID("City"))) << "the field takes focus, so typing goes on there";
        Settle(Interface());

        Click(GetRow(2), Interface());
        EXPECT_EQ(m_Text, "London");
        EXPECT_EQ(m_Changes, 1);
        EXPECT_FALSE(m_IsOpen);

        // The button closes an open list too.
        Click(GetButton(), Interface());
        EXPECT_TRUE(m_IsOpen);
        Click(GetButton(), Interface());
        EXPECT_FALSE(m_IsOpen);
    }

    TEST_F(ComboBoxTests, TypingFiltersAndEnterPicksTheHighlightedItem)
    {
        Settle(Interface());
        Click(m_Rect.GetCenter(), Interface());
        Type("be", Interface());
        EXPECT_EQ(m_Text, "be");
        EXPECT_TRUE(m_IsOpen) << "the list opens with the matching items";

        TapKey(Key::DownArrow, Interface());
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Text, "Bern") << "Berlin was highlighted first, the arrow moved to Bern";
        EXPECT_FALSE(m_IsOpen);
        EXPECT_TRUE(m_Submitted);

        // Typing goes on after the picked text: the caret is at its end.
        Type("e", Interface());
        EXPECT_EQ(m_Text, "Berne");
    }

    TEST_F(ComboBoxTests, LongListsBuildOnlyTheRowsInView)
    {
        // 2,000 items, of which the list shows eight at a time: the rows out of view are not built, and the keyboard
        // and the filter still reach every item.
        std::vector<std::string> names;
        for (int i = 0; i < 2000; i++)
            names.push_back(std::format("Item {:04}", i));
        const std::vector<std::string_view> items(names.begin(), names.end());
        std::string text;
        const Builder build = [&] { ComboBox("Item", &text, items, {.Width = 200.0f}); };

        Settle(build);
        Click(Vec2(100.0f, 12.0f), build);
        TapKey(Key::DownArrow, build);
        Settle(build);
        // The field, its button and eight rows of the list: a few dozen quads, not thousands.
        EXPECT_LT(GetDrawData().Vertices.size(), 4u * 200u);

        // Twenty steps down scroll the highlight along, far past the rows that were in view.
        for (int i = 0; i < 20; i++)
            TapKey(Key::DownArrow, build);
        TapKey(Key::Enter, build);
        EXPECT_EQ(text, "Item 0020");

        // Typing filters: of the items that contain "199" (0199, 1199 and 1990 to 1999), the third is picked.
        text.clear();
        Frame(
            [&]
            {
                ReloadTextField("Item");
                build();
            });
        Type("199", build);
        TapKey(Key::DownArrow, build);
        TapKey(Key::DownArrow, build);
        TapKey(Key::Enter, build);
        EXPECT_EQ(text, "Item 1990");
    }

    TEST_F(ComboBoxTests, AValueThatIsNotAnItemIsKept)
    {
        Settle(Interface());
        Click(m_Rect.GetCenter(), Interface());
        Type("Zurich", Interface());
        EXPECT_FALSE(m_IsOpen) << "nothing matches";
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Text, "Zurich");
    }

    TEST_F(ComboBoxTests, EscapeClosesTheListAndKeepsEditing)
    {
        Settle(Interface());
        Click(m_Rect.GetCenter(), Interface());
        TapKey(Key::DownArrow, Interface());
        EXPECT_TRUE(m_IsOpen);
        TapKey(Key::Escape, Interface());
        EXPECT_FALSE(m_IsOpen);
        EXPECT_TRUE(IsFocused(GetID("City")));
        Type("x", Interface());
        EXPECT_EQ(m_Text, "x");
    }

    TEST_F(ComboBoxTests, LosingFocusClosesTheList)
    {
        Settle(Interface());
        Click(GetButton(), Interface());
        EXPECT_TRUE(m_IsOpen);
        Click(Vec2(600.0f, 500.0f), Interface());
        Frame(Interface());
        EXPECT_FALSE(m_IsOpen);
        EXPECT_TRUE(m_Text.empty());
    }
} // namespace Carbon
