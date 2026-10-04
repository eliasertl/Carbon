#include "Carbon/Extensions/MenuBar.h"

#include <algorithm>

#include "Carbon/Extensions/Internal/MenuInternal.h"
#include "Carbon/Extensions/Menu.h"

namespace Carbon
{
    namespace
    {
        constexpr int MaxMenus = 32;
        constexpr float TitlePadding = 10.0f;
        constexpr float LeadingPadding = 6.0f;
        constexpr float HighlightInset = 3.0f;
        constexpr float HighlightRadius = 5.0f;

        // The menu bar being built.
        struct MenuBarBuild
        {
            ID Id;
            float Height;
            int Count;
            /// The menu that is open during this frame.
            ID OpenMenu;
            /// The index of the menu whose items are being added, between BeginMenuBarMenu and EndMenuBarMenu.
            int CurrentIndex;
            /// An arrow key moved to another menu during this frame. The menu it moved to sees the same key press,
            /// and must not move on again.
            bool HasSwitched;
            bool IsOpen;
        };

        // Remembered per menu bar.
        struct MenuBarState
        {
            /// The menu that was open during the last frame, and how many menus the bar had.
            ID OpenMenu;
            int Count;
            /// A menu to open on behalf of the keyboard, with its first item highlighted; -1 when none.
            int PendingIndex;
            bool FocusFirst;
            /// Alt went down with nothing else: if it comes up the same way, the first menu opens.
            bool IsAltArmed;
        };

        MenuBarBuild& GetBuild()
        {
            return *GetState<MenuBarBuild>(HashID("Carbon.MenuBar.Build"), StateLifetime::Persistent);
        }

        MenuBarState& GetBarState(ID bar)
        {
            MenuBarState& state = *GetState<MenuBarState>(HashID("##state", bar), StateLifetime::Persistent);
            if (state.Count == 0)
                state.PendingIndex = -1;
            return state;
        }

        bool IsAltKey(Key key)
        {
            return key == Key::LeftAlt || key == Key::RightAlt;
        }

        // Alt pressed and released on its own, or F10: the keyboard way into the menu bar, as on Windows.
        bool IsMenuBarKeyPressed(MenuBarState& state)
        {
            bool isOtherKeyPressed = IsMousePressed(MouseButton::Left) || IsMousePressed(MouseButton::Right);
            bool isAltPressed = false;
            bool isAltReleased = false;
            for (int i = 0; i < static_cast<int>(Key::Count); i++)
            {
                const Key key = static_cast<Key>(i);
                if (IsKeyPressed(key, false))
                {
                    if (IsAltKey(key))
                        isAltPressed = true;
                    else
                        isOtherKeyPressed = true;
                }
                if (IsAltKey(key) && IsKeyReleased(key))
                    isAltReleased = true;
            }
            if (isAltPressed)
                state.IsAltArmed = true;
            if (isOtherKeyPressed)
                state.IsAltArmed = false;
            const bool isAltTapped = isAltReleased && state.IsAltArmed;
            if (isAltReleased)
                state.IsAltArmed = false;
            return isAltTapped || IsKeyPressed(Key::F10, false);
        }
    } // namespace

    void BeginMenuBar(const MenuBarOptions& options)
    {
        MenuBarBuild& build = GetBuild();
        build = MenuBarBuild();
        build.Id = GetID("##menubar");
        build.Height = options.Height;
        build.CurrentIndex = -1;
        build.IsOpen = true;

        MenuBarState& state = GetBarState(build.Id);
        const bool isBarMenuOpen = state.OpenMenu.IsValid() && IsOverlayOpen(state.OpenMenu);
        // The keyboard opens the first menu, unless something else (a sheet, a popover) has the keyboard.
        if (IsMenuBarKeyPressed(state) && !isBarMenuOpen && !IsAnyOverlayOpen())
        {
            state.PendingIndex = 0;
            state.FocusFirst = true;
        }

        BeginHStack({.Spacing = 0.0f,
                     .Width = options.Width,
                     .Height = options.Height,
                     .Background = options.Background.value_or(GetStyleColor(StyleColor::SecondaryBackground)),
                     .CornerRadius = 0.0f,
                     .ID = "##menubar"});
        Spacer({.Length = LeadingPadding});
        if (options.HasSeparator)
        {
            // Drawn at the bar's bottom edge once its size is known, in EndMenuBar.
            build.Height = -options.Height;
        }
    }

    void EndMenuBar()
    {
        MenuBarBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen, "EndMenuBar called without BeginMenuBar");
        if (!build.IsOpen)
            return;
        Spacer({.Length = LeadingPadding});
        EndHStack();
        if (build.Height < 0.0f)
        {
            const Rect bar = GetLastItemRect();
            const float pixel = GetContentScale().GetPixelSize();
            GetDrawList().AddRect(Rect(bar.X, bar.GetBottom() - pixel, bar.Width, pixel),
                                  GetStyleColor(StyleColor::Separator));
        }

        MenuBarState& state = GetBarState(build.Id);
        state.OpenMenu = build.OpenMenu;
        state.Count = build.Count;
        build.IsOpen = false;
    }

    bool BeginMenuBarMenu(std::string_view title, const MenuBarMenuOptions& options)
    {
        MenuBarBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen && build.CurrentIndex < 0 && build.Count < MaxMenus,
                  "BeginMenuBarMenu must be called between BeginMenuBar and EndMenuBar, at most {} times", MaxMenus);
        if (!build.IsOpen || build.CurrentIndex >= 0 || build.Count >= MaxMenus)
            return false;
        MenuBarState& state = GetBarState(build.Id);
        const int index = build.Count++;

        const ID titleID = GetID(title);
        const ID menu = HashID("##menu", titleID);
        const std::string_view text = GetDisplayLabel(title);
        const TextSpec spec = GetTextSpec(TextStyle::Body);
        const float barHeight = build.Height < 0.0f ? -build.Height : build.Height;

        PushDisabled(options.Disabled);
        const Rect rect = AllocateItem(Vec2(MeasureText(text, spec).X + TitlePadding * 2.0f, barHeight));
        ButtonBehaviorOptions behavior;
        behavior.Focusable = false;
        behavior.ActivateOnPress = true;
        const Interaction interaction = ButtonBehavior(titleID, rect, behavior);

        bool isOpen = IsOverlayOpen(menu);
        const bool isOtherOpen = !isOpen && state.OpenMenu.IsValid() && IsOverlayOpen(state.OpenMenu);
        const auto open = [&](bool focusFirst)
        {
            if (isOtherOpen)
                CloseOverlay(state.OpenMenu);
            OpenOverlay(menu);
            state.FocusFirst = focusFirst;
            isOpen = true;
        };
        if (!IsDisabled())
        {
            if (interaction.Clicked && !isOpen)
                open(false);
            // While a menu of the bar is open, the pointer moves between menus without clicking. The open menu
            // holds the pointer, so the title is tested against the pointer's position directly.
            else if (isOtherOpen && IsMousePosValid() && rect.Contains(GetMousePos()))
                open(false);
            if (state.PendingIndex == index && !isOpen)
                open(true);
        }
        if (state.PendingIndex == index)
            state.PendingIndex = -1;

        // The title: highlighted while its menu is open, tinted under the pointer.
        const ControlFeedback feedback = AnimateFeedback(titleID, interaction.Hovered, false);
        const float opacity =
            isOpen ? GetStyleVar(StyleVar::PressedAmount) : GetStyleVar(StyleVar::HoverAmount) * feedback.Hover;
        const Rect highlight(rect.X, rect.Y + HighlightInset, rect.Width, rect.Height - HighlightInset * 2.0f);
        DrawList& drawList = GetDrawList();
        if (opacity > 0.001f)
        {
            drawList.AddSquircle(highlight, GetStyleColor(StyleColor::Label).WithOpacity(opacity), HighlightRadius,
                                 GetStyleVar(StyleVar::CornerSmoothing));
        }
        DrawLabel(drawList, rect, rect.X + TitlePadding, text, spec, GetStyleColor(StyleColor::Label));
        PopDisabled();
        SetLastItem(titleID, rect, interaction);

        MenuOptions menuOptions;
        menuOptions.Anchor = highlight;
        menuOptions.Gap = HighlightInset;
        if (!BeginMenu(menu, menuOptions))
            return false;
        build.OpenMenu = menu;
        build.CurrentIndex = index;
        if (state.FocusFirst)
        {
            FocusNext();
            state.FocusFirst = false;
        }
        return true;
    }

    void EndMenuBarMenu()
    {
        MenuBarBuild& build = GetBuild();
        CB_VERIFY(build.CurrentIndex >= 0, "EndMenuBarMenu called without an open BeginMenuBarMenu");
        if (build.CurrentIndex < 0)
            return;

        // Left and right move to the neighbouring menu, unless a submenu uses them.
        MenuBarState& state = GetBarState(build.Id);
        const int count = std::max(state.Count, build.CurrentIndex + 1);
        if (!Internal::DidMenusUseHorizontalArrows() && !build.HasSwitched)
        {
            if (IsKeyPressed(Key::LeftArrow))
                state.PendingIndex = (build.CurrentIndex + count - 1) % count;
            if (IsKeyPressed(Key::RightArrow))
                state.PendingIndex = (build.CurrentIndex + 1) % count;
            if (state.PendingIndex >= 0)
            {
                build.HasSwitched = true;
                RequestAnimationFrame();
            }
        }
        EndMenu();
        build.CurrentIndex = -1;
    }
} // namespace Carbon
