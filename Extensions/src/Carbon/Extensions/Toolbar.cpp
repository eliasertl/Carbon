#include "Carbon/Extensions/Toolbar.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Carbon/Extensions/Menu.h"

namespace Carbon
{
    namespace
    {
        constexpr int MaxEntries = 64;
        constexpr size_t MaxLabel = 48;
        constexpr size_t MaxIcon = 8;

        constexpr float EdgePadding = 8.0f;
        constexpr float ItemSpacing = 4.0f;
        constexpr float FixedSpace = 8.0f;
        constexpr float SeparatorMargin = 4.0f;
        // The button that opens the overflow menu.
        constexpr float OverflowButtonWidth = 24.0f;
        constexpr float OverflowButtonHeight = 28.0f;

        // Items with icons: the icon sits in a box that carries the hover highlight, the label (if shown) below.
        constexpr float IconBoxWidth = 36.0f;
        constexpr float IconBoxHeight = 28.0f;
        constexpr float IconOnlySize = 17.0f;
        constexpr float IconWithLabelSize = 16.0f;
        constexpr float LabelGap = 2.0f;
        constexpr float LabelPadding = 4.0f;
        constexpr float LabelOnlyPadding = 10.0f;
        constexpr float LabelOnlyHeight = 24.0f;
        constexpr float HighlightRadius = 6.0f;

        enum class EntryKind : uint8_t
        {
            Item,
            Control,
            FlexibleSpace,
            Space,
            Separator
        };

        // One entry of the toolbar as it was submitted. Labels and icons are copied, so the overflow menu can be
        // built after the calls that named them, and the state stays trivially copyable.
        struct Entry
        {
            EntryKind Kind;
            bool Disabled;
            bool IsSelected;
            float Width;
            char Label[MaxLabel];
            char Icon[MaxIcon];
        };

        // Remembered per toolbar.
        struct ToolbarState
        {
            Entry Entries[MaxEntries];
            int Count;
            /// Entries from this index on do not fit and are in the overflow menu.
            int VisibleCount;
            /// An entry chosen in the overflow menu, plus one; 0 when none. An item reports it in the next frame.
            int PendingActivation;
            /// A control chosen in the overflow menu, plus one: its popover opens in the next frame.
            int PendingControl;
            /// The control whose popover is open, plus one.
            int OpenControl;
            /// The overflow menu was opened with the keyboard: its first item gets the highlight.
            bool FocusFirst;
            /// A control's popover has just opened: the control gets the keyboard, so that one can type at once.
            bool FocusControl;
            Rect OverflowButton;
        };

        // The toolbar being built.
        struct ToolbarBuild
        {
            ID Id;
            ToolbarState* State;
            ToolbarDisplayMode Mode;
            float Height;
            bool HasSeparator;
            bool IsOpen;
            /// The index the next entry gets.
            int Index;
            /// How EndToolbarControl closes what BeginToolbarControl opened.
            bool IsControlInStack;
            bool IsControlLabeled;
            bool IsControlInPopover;
            int ControlIndex;
        };

        ToolbarBuild& GetBuild()
        {
            return *GetState<ToolbarBuild>(HashID("Carbon.Toolbar.Build"), StateLifetime::Persistent);
        }

        template <size_t Size>
        void CopyText(char (&buffer)[Size], std::string_view text)
        {
            size_t length = std::min(text.size(), Size - 1);
            // Never cut a UTF-8 sequence in half.
            if (length < text.size())
            {
                while (length > 0 && (static_cast<unsigned char>(text[length]) & 0xC0) == 0x80)
                    length--;
            }
            std::memcpy(buffer, text.data(), length);
            buffer[length] = '\0';
        }

        // Records an entry of this frame and returns its index, or -1 past the capacity.
        int AddEntry(ToolbarBuild& build, EntryKind kind, std::string_view label, std::string_view icon, float width)
        {
            CB_VERIFY(build.Index < MaxEntries, "A toolbar has at most {} items", MaxEntries);
            if (build.Index >= MaxEntries)
                return -1;
            const int index = build.Index++;
            Entry& entry = build.State->Entries[index];
            entry.Kind = kind;
            entry.Disabled = false;
            entry.IsSelected = false;
            // An entry that is not shown keeps the width it had when it was: controls are measured only then.
            if (index < build.State->VisibleCount || entry.Width <= 0.0f || kind != EntryKind::Control)
                entry.Width = width;
            CopyText(entry.Label, label);
            CopyText(entry.Icon, icon);
            return index;
        }

        bool IsVisible(const ToolbarBuild& build, int index)
        {
            return index >= 0 && index < build.State->VisibleCount;
        }

        // How many entries fit into `available` points; the rest go into the overflow menu, from the trailing end.
        int CountVisible(const ToolbarState& state, float available)
        {
            float total = 0.0f;
            for (int i = 0; i < state.Count; i++)
                total += state.Entries[i].Width + (i > 0 ? ItemSpacing : 0.0f);
            if (total <= available)
                return MaxEntries;

            // Flexible spaces have no width of their own, so they shrink to nothing first.
            float used = OverflowButtonWidth + ItemSpacing;
            int visible = state.Count;
            for (int i = 0; i < state.Count; i++)
            {
                used += state.Entries[i].Width + ItemSpacing;
                if (used > available)
                {
                    visible = i;
                    break;
                }
            }
            // A separator or space in front of the chevron would separate nothing.
            while (visible > 0 && state.Entries[visible - 1].Kind != EntryKind::Item &&
                   state.Entries[visible - 1].Kind != EntryKind::Control)
                visible--;
            return visible;
        }

        float GetHeight(const ToolbarOptions& options)
        {
            if (options.Height.has_value())
                return *options.Height;
            return options.DisplayMode == ToolbarDisplayMode::IconAndLabel ? 52.0f : 38.0f;
        }

        TextSpec GetLabelSpec(ToolbarDisplayMode mode)
        {
            return GetTextSpec(mode == ToolbarDisplayMode::IconAndLabel ? TextStyle::Subheadline : TextStyle::Body);
        }

        // The overflow menu: the entries that do not fit, in their order. Items and controls become menu items,
        // separators stay, spaces disappear.
        void OverflowMenu(ToolbarBuild& build)
        {
            ToolbarState& state = *build.State;
            const ID button = HashID("##overflow", build.Id);
            const ID menu = HashID("##overflowmenu", build.Id);

            const Rect rect = AllocateItem(Vec2(OverflowButtonWidth, OverflowButtonHeight));
            state.OverflowButton = rect;
            ButtonBehaviorOptions behavior;
            behavior.ActivateOnPress = true;
            const Interaction interaction = ButtonBehavior(button, rect, behavior);
            const bool isArrowPressed = interaction.Focused && IsKeyPressed(Key::DownArrow, false);
            if ((interaction.Clicked || isArrowPressed) && !IsOverlayOpen(menu))
            {
                OpenOverlay(menu);
                state.FocusFirst = !IsMousePressed() && !IsMouseReleased();
            }

            DrawList& drawList = GetDrawList();
            const Color labelColor = GetStyleColor(StyleColor::Label);
            const ControlFeedback feedback = AnimateFeedback(button, interaction.Hovered, interaction.Pressed);
            drawList.AddSquircle(rect, ApplyFeedback(labelColor.WithOpacity(0.0f), labelColor, feedback),
                                 HighlightRadius, GetStyleVar(StyleVar::CornerSmoothing));
            DrawIcon(drawList, rect.GetCenter(), Icons::CaretDoubleRight, 13.0f,
                     GetStyleColor(StyleColor::SecondaryLabel), IconVariant::Bold);
            DrawFocusRing(button, rect, HighlightRadius);
            SetLastItem(button, rect, interaction);
            Tooltip("More items");

            MenuOptions menuOptions;
            menuOptions.Anchor = rect;
            menuOptions.Alignment = Alignment::Trailing;
            if (!BeginMenu(menu, menuOptions))
                return;
            if (state.FocusFirst)
            {
                FocusNext();
                state.FocusFirst = false;
            }
            bool needsSeparator = false;
            bool hasItems = false;
            for (int i = state.VisibleCount; i < state.Count; i++)
            {
                const Entry& entry = state.Entries[i];
                if (entry.Kind == EntryKind::Separator)
                {
                    needsSeparator = hasItems;
                    continue;
                }
                if (entry.Kind != EntryKind::Item && entry.Kind != EntryKind::Control)
                    continue;
                if (needsSeparator)
                    MenuSeparator();
                needsSeparator = false;
                hasItems = true;

                PushID(i);
                MenuItemOptions itemOptions;
                itemOptions.Icon = entry.Icon;
                itemOptions.IsChecked = entry.IsSelected;
                itemOptions.Disabled = entry.Disabled;
                if (MenuItem(entry.Label, itemOptions))
                {
                    if (entry.Kind == EntryKind::Item)
                        state.PendingActivation = i + 1;
                    else
                        state.PendingControl = i + 1;
                    RequestAnimationFrame();
                }
                PopID();
            }
            EndMenu();
        }
    } // namespace

    void BeginToolbar(std::string_view id, const ToolbarOptions& options)
    {
        ToolbarBuild& build = GetBuild();
        CB_VERIFY(!build.IsOpen, "Toolbars cannot be nested");
        build = ToolbarBuild();
        build.Id = GetID(id);
        build.State = GetState<ToolbarState>(HashID("##state", build.Id));
        build.Mode = options.DisplayMode;
        build.Height = GetHeight(options);
        build.HasSeparator = options.HasSeparator;
        build.IsOpen = true;

        BeginHStack({.Spacing = ItemSpacing,
                     .Padding = EdgeInsets(EdgePadding, 0.0f),
                     .Width = options.Width,
                     .Height = build.Height,
                     .Background = options.Background.value_or(GetStyleColor(StyleColor::SecondaryBackground)),
                     .CornerRadius = 0.0f,
                     .ID = id});

        // Which entries fit is decided from last frame's widths: the entries of this frame are not known yet.
        ToolbarState& state = *build.State;
        state.VisibleCount = CountVisible(state, GetContentRect().Width);

        // A control chosen in the overflow menu opens in a popover now that the menu has closed.
        if (state.PendingControl > 0)
        {
            state.OpenControl = state.PendingControl;
            state.PendingControl = 0;
            state.FocusControl = true;
            OpenOverlay(HashID("##controlpopover", build.Id));
        }
    }

    void EndToolbar()
    {
        ToolbarBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen, "EndToolbar called without BeginToolbar");
        if (!build.IsOpen)
            return;
        ToolbarState& state = *build.State;
        if (state.VisibleCount < build.Index)
        {
            // The chevron goes to the trailing edge, where the items it stands for would be.
            Spacer();
            OverflowMenu(build);
        }
        EndHStack();
        const Rect bar = GetLastItemRect();
        if (build.HasSeparator)
        {
            const float pixel = GetContentScale().GetPixelSize();
            GetDrawList().AddRect(Rect(bar.X, bar.GetBottom() - pixel, bar.Width, pixel),
                                  GetStyleColor(StyleColor::Separator));
        }

        // With this frame's entries and widths a different set may fit; then one more frame is needed.
        state.Count = build.Index;
        const int shown = std::min(state.VisibleCount, state.Count);
        if (std::min(CountVisible(state, bar.Width - EdgePadding * 2.0f), state.Count) != shown)
            RequestAnimationFrame();
        if (state.OpenControl > 0 && !IsOverlayOpen(HashID("##controlpopover", build.Id)))
            state.OpenControl = 0;
        build.IsOpen = false;
    }

    bool ToolbarItem(std::string_view label, const ToolbarItemOptions& options)
    {
        ToolbarBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen, "ToolbarItem must be called between BeginToolbar and EndToolbar");
        if (!build.IsOpen)
            return false;

        const ID id = GetID(label);
        const std::string_view title = GetDisplayLabel(label);
        const ToolbarDisplayMode mode = options.Icon.empty() ? ToolbarDisplayMode::LabelOnly : build.Mode;
        const TextSpec spec = GetLabelSpec(mode);
        const float titleWidth = title.empty() ? 0.0f : MeasureText(title, spec).X;

        Vec2 size;
        switch (mode)
        {
            case ToolbarDisplayMode::IconAndLabel:
                size = Vec2(std::max(IconBoxWidth, titleWidth + LabelPadding * 2.0f),
                            IconBoxHeight + LabelGap + spec.LineHeight);
                break;
            case ToolbarDisplayMode::IconOnly:
                size = Vec2(IconBoxWidth, IconBoxHeight);
                break;
            case ToolbarDisplayMode::LabelOnly:
                size = Vec2(titleWidth + LabelOnlyPadding * 2.0f, LabelOnlyHeight);
                break;
        }

        const int index = AddEntry(build, EntryKind::Item, title, options.Icon, size.X);
        if (index < 0)
            return false;
        Entry& entry = build.State->Entries[index];
        entry.Disabled = options.Disabled;
        entry.IsSelected = options.IsSelected;

        // An item in the overflow menu reports there that it was chosen.
        if (!IsVisible(build, index))
        {
            const bool isChosen = build.State->PendingActivation == index + 1;
            if (isChosen)
                build.State->PendingActivation = 0;
            return isChosen && !options.Disabled;
        }

        PushDisabled(options.Disabled);
        const Rect rect = AllocateItem(size);
        const Interaction interaction = ButtonBehavior(id, rect);
        const Rect box = mode == ToolbarDisplayMode::IconAndLabel
                             ? Rect(rect.GetCenter().X - IconBoxWidth * 0.5f, rect.Y, IconBoxWidth, IconBoxHeight)
                             : rect;

        // No bezel: a highlight appears under the pointer, and stays as a fill while the item is turned on.
        DrawList& drawList = GetDrawList();
        const Color labelColor = GetStyleColor(StyleColor::Label);
        const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
        const ControlFeedback feedback = AnimateFeedback(id, interaction.Hovered, interaction.Pressed);
        const Color base = options.IsSelected ? GetStyleColor(StyleColor::ControlFill) : labelColor.WithOpacity(0.0f);
        drawList.AddSquircle(box, ApplyFeedback(base, labelColor, feedback), HighlightRadius, smoothing);

        const Color content =
            options.IsSelected ? GetStyleColor(StyleColor::Accent) : GetStyleColor(StyleColor::SecondaryLabel);
        switch (mode)
        {
            case ToolbarDisplayMode::IconAndLabel:
                DrawIcon(drawList, box.GetCenter(), options.Icon, IconWithLabelSize, content);
                DrawLabel(drawList, Rect(rect.X, box.GetBottom() + LabelGap, rect.Width, spec.LineHeight),
                          rect.GetCenter().X - titleWidth * 0.5f, title, spec,
                          GetStyleColor(StyleColor::SecondaryLabel));
                break;
            case ToolbarDisplayMode::IconOnly:
                DrawIcon(drawList, box.GetCenter(), options.Icon, IconOnlySize, content);
                break;
            case ToolbarDisplayMode::LabelOnly:
                DrawLabel(drawList, rect, rect.GetCenter().X - titleWidth * 0.5f, title, spec,
                          options.IsSelected ? content : labelColor);
                break;
        }

        DrawFocusRing(id, box, HighlightRadius);
        SetLastItem(id, rect, interaction);
        if (mode == ToolbarDisplayMode::IconOnly && !title.empty())
            Tooltip(title);
        PopDisabled();
        return interaction.Clicked;
    }

    void ToolbarFlexibleSpace()
    {
        ToolbarBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen, "ToolbarFlexibleSpace must be called between BeginToolbar and EndToolbar");
        if (!build.IsOpen)
            return;
        const int index = AddEntry(build, EntryKind::FlexibleSpace, {}, {}, 0.0f);
        if (IsVisible(build, index))
            Spacer();
    }

    void ToolbarSpace()
    {
        ToolbarBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen, "ToolbarSpace must be called between BeginToolbar and EndToolbar");
        if (!build.IsOpen)
            return;
        const int index = AddEntry(build, EntryKind::Space, {}, {}, FixedSpace);
        if (IsVisible(build, index))
            Spacer({.Length = FixedSpace});
    }

    void ToolbarSeparator()
    {
        ToolbarBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen, "ToolbarSeparator must be called between BeginToolbar and EndToolbar");
        if (!build.IsOpen)
            return;
        const float pixel = GetContentScale().GetPixelSize();
        const float width = SeparatorMargin * 2.0f + pixel;
        const int index = AddEntry(build, EntryKind::Separator, {}, {}, width);
        if (!IsVisible(build, index))
            return;
        const Rect rect = AllocateItem(Vec2(width, IconBoxHeight - 6.0f));
        GetDrawList().AddRect(Rect(GetContentScale().Snap(rect.X + SeparatorMargin), rect.Y, pixel, rect.Height),
                              GetStyleColor(StyleColor::Separator));
    }

    bool BeginToolbarControl(std::string_view label, const ToolbarControlOptions& options)
    {
        ToolbarBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen, "BeginToolbarControl must be called between BeginToolbar and EndToolbar");
        if (!build.IsOpen)
            return false;
        CB_VERIFY(!build.IsControlInStack && !build.IsControlInPopover,
                  "BeginToolbarControl needs EndToolbarControl before the next one");

        const int index = AddEntry(build, EntryKind::Control, GetDisplayLabel(label), options.Icon, 0.0f);
        if (index < 0)
            return false;
        build.ControlIndex = index;
        if (IsVisible(build, index))
        {
            // Wrapped in a stack whose width is the entry's width for the overflow calculation. With labels shown,
            // the control sits on the row of the icons and has its label below, like the items.
            // Every control's stacks are begun from these lines, so each gets its index as its identity. The ID
            // is popped right away: the stacks keep it, and the caller's widgets inside keep their own IDs.
            build.IsControlLabeled = build.Mode == ToolbarDisplayMode::IconAndLabel;
            PushID(index);
            if (build.IsControlLabeled)
                BeginVStack({.Spacing = LabelGap, .Alignment = Alignment::Center});
            BeginHStack({.Spacing = ItemSpacing, .Height = build.IsControlLabeled ? IconBoxHeight : Size::Fit()});
            PopID();
            build.IsControlInStack = true;
            return true;
        }

        // In the overflow menu: built only while its popover, opened from there, is showing.
        ToolbarState& state = *build.State;
        if (state.OpenControl != index + 1)
            return false;
        OverlayOptions overlay;
        overlay.Anchor = state.OverflowButton;
        overlay.Alignment = Alignment::Trailing;
        overlay.ShowsArrow = true;
        if (!BeginOverlay(HashID("##controlpopover", build.Id), overlay))
        {
            state.OpenControl = 0;
            return false;
        }
        build.IsControlInPopover = true;
        if (state.FocusControl)
        {
            FocusNext();
            state.FocusControl = false;
        }
        return true;
    }

    void EndToolbarControl()
    {
        ToolbarBuild& build = GetBuild();
        CB_VERIFY(build.IsControlInStack || build.IsControlInPopover,
                  "EndToolbarControl called without a BeginToolbarControl that returned true");
        if (build.IsControlInStack)
        {
            EndHStack();
            if (build.IsControlLabeled)
            {
                Text(build.State->Entries[build.ControlIndex].Label,
                     {.Style = TextStyle::Subheadline, .Secondary = true});
                EndVStack();
            }
            build.State->Entries[build.ControlIndex].Width = GetLastItemRect().Width;
        }
        else if (build.IsControlInPopover)
        {
            EndOverlay();
        }
        build.IsControlInStack = false;
        build.IsControlLabeled = false;
        build.IsControlInPopover = false;
    }
} // namespace Carbon
