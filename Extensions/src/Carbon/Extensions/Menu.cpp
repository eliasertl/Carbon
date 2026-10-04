#include "Carbon/Extensions/Menu.h"

#include <algorithm>

#include "Carbon/Extensions/Internal/MenuInternal.h"

namespace Carbon
{
    namespace
    {
        // Space between the menu's edge and its rows.
        constexpr float MenuPadding = 5.0f;
        constexpr float MenuCornerRadius = 8.0f;
        constexpr float RowHeight = 22.0f;
        constexpr float RowCornerRadius = 4.0f;
        // Space between a row's edge and its content.
        constexpr float RowPadding = 8.0f;
        // Column for checkmarks and icons; it exists only in menus that have any.
        constexpr float LeadingWidth = 18.0f;
        constexpr float ShortcutGap = 24.0f;
        constexpr float ChevronWidth = 14.0f;
        constexpr float SeparatorHeight = 11.0f;
        constexpr float HeaderHeight = 20.0f;
        // Seconds the pointer rests on a submenu's item before it opens, and on the parent before it closes.
        constexpr float SubmenuDelay = 0.2f;
        constexpr int MaxMenuDepth = 8;

        // A menu whose items are being added.
        struct MenuLevel
        {
            ID Id;
            Rect Bounds;
            /// The item of the parent menu that opened this submenu.
            Rect OpenerRow;
            float MinWidth;
            bool HasLeadingColumn;
            /// A submenu of this menu is open: that one handles the keyboard.
            bool BuiltSubmenu;
        };

        // The chain of menus being built: a menu, its open submenu, that one's open submenu.
        struct MenuStack
        {
            uint64_t Frame;
            int Depth;
            /// A submenu was built during this frame, or the arrow keys opened or closed one: the left and right
            /// arrow keys belong to the menus, not to a menu bar.
            bool UsedHorizontalArrows;
            MenuLevel Levels[MaxMenuDepth];
        };

        // Remembered per menu while it is open.
        struct MenuState
        {
            bool HasLeadingColumn;
            bool SawLeadingColumn;
            /// The item the pointer highlighted last. The pointer takes the highlight again only when it moves or
            /// reaches another item, so it does not fight the arrow keys while it rests.
            ID PointerItem;
            bool SawPointerItem;
        };

        struct SubmenuState
        {
            float HoverTime;
            float LingerTime;
            /// Opened by keyboard: its first item gets the highlight.
            bool FocusFirst;
        };

        struct ContextMenuState
        {
            Vec2 Position;
        };

        struct RowResult
        {
            Carbon::Interaction Interaction;
            Rect Row;
        };

        MenuStack& GetStack()
        {
            MenuStack& stack = *GetState<MenuStack>(HashID("Carbon.Menu.Stack"), StateLifetime::Persistent);
            if (stack.Frame != GetFrameCount())
            {
                stack.Frame = GetFrameCount();
                stack.Depth = 0;
                stack.UsedHorizontalArrows = false;
            }
            return stack;
        }

        MenuState& GetMenuState(ID menu)
        {
            return *GetState<MenuState>(HashID("##menu", menu));
        }

        OverlayOptions GetMenuOverlay()
        {
            OverlayOptions overlay;
            overlay.Padding = EdgeInsets(MenuPadding);
            overlay.Spacing = 0.0f;
            overlay.CornerRadius = MenuCornerRadius;
            return overlay;
        }

        bool BeginMenuLevel(ID id, const OverlayOptions& overlay, float minWidth, const Rect& openerRow)
        {
            MenuStack& stack = GetStack();
            const bool hasRoom = stack.Depth < MaxMenuDepth;
            CB_VERIFY(hasRoom, "Menus can be nested at most {} deep", MaxMenuDepth);
            if (!hasRoom || !BeginOverlay(id, overlay))
                return false;

            MenuState& state = GetMenuState(id);
            state.HasLeadingColumn = state.SawLeadingColumn;
            state.SawLeadingColumn = false;
            if (!state.SawPointerItem)
                state.PointerItem = ID();
            state.SawPointerItem = false;

            MenuLevel& level = stack.Levels[stack.Depth++];
            level.Id = id;
            level.Bounds = GetContentRect().Expand(MenuPadding);
            level.OpenerRow = openerRow;
            level.MinWidth = minWidth;
            level.HasLeadingColumn = state.HasLeadingColumn;
            level.BuiltSubmenu = false;
            return true;
        }

        void EndMenuLevel()
        {
            MenuStack& stack = GetStack();
            if (stack.Depth > 0)
            {
                // The innermost open menu has the keyboard.
                if (!stack.Levels[stack.Depth - 1].BuiltSubmenu)
                {
                    if (IsKeyPressed(Key::DownArrow))
                        FocusNext();
                    if (IsKeyPressed(Key::UpArrow))
                        FocusPrevious();
                }
                stack.Depth--;
            }
            EndOverlay();
        }

        // The pointer rules of a chain of menus, applied by the innermost one: a click outside it closes the
        // submenus above the menu that was clicked, or everything; resting on a parent menu closes its submenu.
        void UpdateMenuChain(MenuStack& stack)
        {
            const int innermost = stack.Depth - 1;
            SubmenuState& state = *GetState<SubmenuState>(stack.Levels[innermost].Id);
            const Vec2 mouse = GetMousePos();
            if (!IsMousePosValid() || stack.Levels[innermost].Bounds.Contains(mouse))
            {
                state.LingerTime = 0.0f;
                return;
            }

            int over = -1;
            for (int i = innermost - 1; i >= 0 && over < 0; i--)
            {
                if (stack.Levels[i].Bounds.Contains(mouse))
                    over = i;
            }

            if (IsMousePressed(MouseButton::Left) || IsMousePressed(MouseButton::Right))
            {
                CloseOverlay(stack.Levels[over + 1].Id);
                return;
            }

            if (over >= 0 && !stack.Levels[over + 1].OpenerRow.Contains(mouse))
            {
                state.LingerTime += GetDeltaTime();
                RequestAnimationFrame();
                if (state.LingerTime >= SubmenuDelay)
                    CloseOverlay(stack.Levels[over + 1].Id);
            }
            else
            {
                state.LingerTime = 0.0f;
            }
        }

        // Lays out, handles and draws one row. `staysHighlighted` keeps a submenu's item lit while the submenu
        // is open.
        RowResult MenuRow(std::string_view label, const MenuItemOptions& options, bool hasSubmenu,
                          bool staysHighlighted)
        {
            RowResult result;
            MenuStack& stack = GetStack();
            const bool isInMenu = stack.Depth > 0;
            CB_VERIFY(isInMenu, "Menu items must be added between BeginMenu and EndMenu");
            if (!isInMenu)
                return result;
            const MenuLevel& level = stack.Levels[stack.Depth - 1];

            const ID id = GetID(label);
            const std::string_view title = GetDisplayLabel(label);
            const TextSpec spec = GetTextSpec(TextStyle::Body);
            if (options.IsChecked || !options.Icon.empty())
                GetMenuState(level.Id).SawLeadingColumn = true;
            const float leading = level.HasLeadingColumn ? LeadingWidth : 0.0f;

            float width = RowPadding * 2.0f + leading + MeasureText(title, spec).X;
            const float shortcutWidth = options.Shortcut.empty() ? 0.0f : MeasureText(options.Shortcut, spec).X;
            if (!options.Shortcut.empty())
                width += ShortcutGap + shortcutWidth;
            if (hasSubmenu)
                width += ChevronWidth;

            // Every row spans the menu, whose width is that of its widest row.
            const Rect item = AllocateItem(Vec2(std::max(width, level.MinWidth - MenuPadding * 2.0f), RowHeight));
            const Rect content = GetContentRect();
            result.Row = Rect(content.X, item.Y, std::max(content.Width, item.Width), item.Height);

            PushDisabled(options.Disabled);
            result.Interaction = ButtonBehavior(id, result.Row);
            // The highlight follows the pointer and the arrow keys alike: it is the keyboard focus.
            if (result.Interaction.Hovered)
            {
                MenuState& state = GetMenuState(level.Id);
                const Vec2 delta = GetMouseDelta();
                const bool hasMoved = delta.X != 0.0f || delta.Y != 0.0f;
                if ((hasMoved || state.PointerItem != id) && !IsFocused(id))
                    SetFocus(id);
                state.PointerItem = id;
                state.SawPointerItem = true;
            }
            const bool isHighlighted = (IsFocused(id) || staysHighlighted) && !IsDisabled();

            DrawList& drawList = GetDrawList();
            Color text = GetStyleColor(options.IsDestructive ? StyleColor::Destructive : StyleColor::Label);
            Color secondary = GetStyleColor(StyleColor::SecondaryLabel);
            if (isHighlighted)
            {
                drawList.AddSquircle(result.Row, GetStyleColor(StyleColor::Selection), RowCornerRadius,
                                     GetStyleVar(StyleVar::CornerSmoothing));
                text = GetStyleColor(StyleColor::OnAccent);
                secondary = text;
            }

            const float centerY = result.Row.GetCenter().Y;
            float x = result.Row.X + RowPadding;
            if (leading > 0.0f)
            {
                if (options.IsChecked)
                    DrawIcon(drawList, Vec2(x + 6.0f, centerY), Icons::Check, 12.0f, text, IconVariant::Bold);
                else if (!options.Icon.empty())
                    DrawIcon(drawList, Vec2(x + 7.0f, centerY), options.Icon, 14.0f, text);
                x += leading;
            }
            DrawLabel(drawList, result.Row, x, title, spec, text);

            float right = result.Row.GetRight() - RowPadding;
            if (hasSubmenu)
            {
                DrawIcon(drawList, Vec2(right - 4.0f, centerY), Icons::CaretRight, 10.0f, secondary, IconVariant::Bold);
                right -= ChevronWidth;
            }
            if (!options.Shortcut.empty())
                DrawLabel(drawList, result.Row, right - shortcutWidth, options.Shortcut, spec, secondary);

            PopDisabled();
            return result;
        }
    } // namespace

    void OpenMenu(std::string_view id)
    {
        OpenOverlay(GetID(id));
    }

    bool IsMenuOpen(std::string_view id)
    {
        return IsOverlayOpen(GetID(id));
    }

    bool BeginMenu(std::string_view id, const MenuOptions& options)
    {
        return BeginMenu(GetID(id), options);
    }

    bool BeginMenu(ID id, const MenuOptions& options)
    {
        OverlayOptions overlay = GetMenuOverlay();
        overlay.Anchor = options.Anchor.value_or(GetItemRect());
        overlay.Placement = options.Placement;
        overlay.Alignment = options.Alignment;
        overlay.Gap = options.Gap;
        return BeginMenuLevel(id, overlay, options.MinWidth, Rect());
    }

    void EndMenu()
    {
        EndMenuLevel();
    }

    bool MenuItem(std::string_view label, const MenuItemOptions& options)
    {
        const RowResult row = MenuRow(label, options, false, false);
        if (!row.Interaction.Clicked)
            return false;
        // Choosing a command closes the whole chain of menus.
        CloseOverlay(GetStack().Levels[0].Id);
        return true;
    }

    void MenuSeparator()
    {
        const Rect item = AllocateItem(Vec2(0.0f, SeparatorHeight));
        const Rect content = GetContentRect();
        const ContentScale scale = GetContentScale();
        const float y = scale.Snap(item.Y + SeparatorHeight * 0.5f);
        GetDrawList().AddRect(Rect(content.X + RowPadding, y, content.Width - RowPadding * 2.0f, scale.GetPixelSize()),
                              GetStyleColor(StyleColor::Separator));
    }

    void MenuHeader(std::string_view title)
    {
        const TextSpec spec = GetTextSpec(TextStyle::Subheadline, true);
        const Rect item = AllocateItem(Vec2(MeasureText(title, spec).X + RowPadding * 2.0f, HeaderHeight));
        DrawLabel(GetDrawList(), item, GetContentRect().X + RowPadding, title, spec,
                  GetStyleColor(StyleColor::SecondaryLabel));
    }

    bool BeginSubmenu(std::string_view label, const MenuItemOptions& options)
    {
        MenuStack& stack = GetStack();
        const bool isInMenu = stack.Depth > 0;
        CB_VERIFY(isInMenu, "BeginSubmenu must be called between BeginMenu and EndMenu");
        if (!isInMenu)
            return false;

        const ID submenu = HashID("##submenu", GetID(label));
        const bool wasOpen = IsOverlayOpen(submenu);
        const RowResult row = MenuRow(label, options, true, wasOpen);

        // It opens when its item is chosen, with the right arrow key, or when the pointer rests on the item.
        SubmenuState& state = *GetState<SubmenuState>(submenu);
        if (row.Interaction.Hovered && !wasOpen)
        {
            state.HoverTime += GetDeltaTime();
            RequestAnimationFrame();
        }
        else
        {
            state.HoverTime = 0.0f;
        }
        const bool byKeyboard = row.Interaction.Focused && (IsKeyPressed(Key::RightArrow, false) ||
                                                            (row.Interaction.Clicked && !IsMouseReleased()));
        if (!wasOpen && !options.Disabled && (row.Interaction.Clicked || byKeyboard || state.HoverTime >= SubmenuDelay))
        {
            OpenOverlay(submenu);
            state.FocusFirst = byKeyboard;
            stack.UsedHorizontalArrows = stack.UsedHorizontalArrows || byKeyboard;
            state.HoverTime = 0.0f;
            state.LingerTime = 0.0f;
        }

        // The submenu lines up its first row with the item, at the parent menu's edge. It holds the pointer
        // like a modal overlay; UpdateMenuChain decides what a click outside of it closes.
        OverlayOptions overlay = GetMenuOverlay();
        overlay.Anchor = row.Row.Expand(MenuPadding);
        overlay.Placement = OverlayPlacement::Trailing;
        overlay.Alignment = Alignment::Leading;
        overlay.Gap = 0.0f;
        overlay.IsModal = true;
        if (!BeginMenuLevel(submenu, overlay, 0.0f, row.Row))
            return false;

        stack.Levels[stack.Depth - 2].BuiltSubmenu = true;
        stack.UsedHorizontalArrows = true;
        if (state.FocusFirst)
        {
            FocusNext();
            state.FocusFirst = false;
        }
        return true;
    }

    void EndSubmenu()
    {
        MenuStack& stack = GetStack();
        if (stack.Depth >= 2 && !stack.Levels[stack.Depth - 1].BuiltSubmenu)
        {
            UpdateMenuChain(stack);
            if (IsKeyPressed(Key::LeftArrow, false))
            {
                CloseOverlay(stack.Levels[stack.Depth - 1].Id);
                stack.UsedHorizontalArrows = true;
            }
        }
        EndMenuLevel();
    }

    bool BeginContextMenu(std::string_view id)
    {
        const ID menu = GetID(id);
        ContextMenuState& state = *GetState<ContextMenuState>(HashID("##context", menu));
        if (IsItemHovered() && IsMousePressed(MouseButton::Right))
        {
            state.Position = GetMousePos();
            OpenOverlay(menu);
        }

        MenuOptions options;
        options.Anchor = Rect(state.Position, Vec2());
        options.Gap = 0.0f;
        return BeginMenu(menu, options);
    }

    void EndContextMenu()
    {
        EndMenuLevel();
    }

    bool Internal::DidMenusUseHorizontalArrows()
    {
        return GetStack().UsedHorizontalArrows;
    }
} // namespace Carbon
