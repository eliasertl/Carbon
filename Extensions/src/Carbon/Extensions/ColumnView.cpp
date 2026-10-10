#include "Carbon/Extensions/ColumnView.h"

#include <algorithm>

#include "Carbon/Extensions/Internal/BuildState.h"
#include "Carbon/Extensions/Internal/SelectionList.h"

namespace Carbon
{
    namespace
    {
        constexpr int MaxColumns = 32;
        constexpr float CornerRadius = 7.0f;
        constexpr float ColumnPadding = 5.0f;
        constexpr float RowPadding = 8.0f;
        constexpr float IconSize = 15.0f;
        constexpr float IconGap = 6.0f;
        constexpr float ChevronSize = 10.0f;
        // The line between two columns can be grabbed this far to either side.
        constexpr float GrabMargin = 3.0f;

        // The column view being built.
        struct ColumnViewBuild
        {
            ColumnViewOptions Options;
            /// The column view's remembered state, identified at the view's ID scope.
            ID StateID;
            ID ColumnIDs[MaxColumns];
            /// The ordinal of the selected item of each column, or -1.
            int SelectedOrdinals[MaxColumns];
            int ColumnCount;
            bool SelectedHasChildren;
            /// The keyboard moved into the current column during this frame.
            bool IsArriving;
            bool IsInColumn;
            bool IsOpen;
        };

        // Remembered per column view.
        struct ColumnViewState
        {
            float Widths[MaxColumns];
            float DragStartWidth;
            int LastColumnCount;
            /// A keyboard move into another column: the column to focus, and the item to pick there. With -1, the
            /// first item is picked if the column has no selection.
            int FocusColumn;
            int PickOrdinal;
            bool IsFocusPending;
        };

        Internal::BuildState<ColumnViewBuild> s_Build("Carbon.ColumnView.Build");

        ColumnViewBuild& GetBuild()
        {
            return s_Build.Get();
        }

        ColumnViewState& GetViewState()
        {
            return *GetState<ColumnViewState>(GetBuild().StateID, StateLifetime::Persistent);
        }

        float& GetColumnWidth(ColumnViewState& state, const ColumnViewBuild& build, int index)
        {
            float& width = state.Widths[index];
            if (width <= 0.0f)
                width = build.Options.ColumnWidth;
            return width;
        }

        // The line after a column: a hairline that can be dragged to resize the column.
        void ColumnDivider(ColumnViewState& state, const ColumnViewBuild& build, int index)
        {
            const float pixel = GetContentScale().GetPixelSize();
            ItemOptions item;
            item.Height = Size::Fill();
            const Rect line = AllocateItem(Vec2(pixel, 0.0f), item);
            const Rect grab(line.X - GrabMargin, line.Y, line.Width + GrabMargin * 2.0f, line.Height);

            DragBehaviorOptions behavior;
            behavior.Focusable = false;
            const DragInteraction drag = DragBehavior(HashID(index, GetID("##divider")), grab, behavior);
            float& width = GetColumnWidth(state, build, index);
            if (drag.Started)
                state.DragStartWidth = width;
            if (drag.Active)
                width = std::max(state.DragStartWidth + drag.Total.X, build.Options.MinColumnWidth);
            if (drag.Hovered || drag.Active)
                SetCursor(Cursor::ResizeHorizontal);
            GetDrawList().AddRect(line, GetStyleColor(StyleColor::Separator));
        }
    } // namespace

    void BeginColumnView(std::string_view id, const ColumnViewOptions& options)
    {
        ColumnViewBuild& build = s_Build.Begin();
        build = ColumnViewBuild();
        build.Options = options;
        build.Options.RowHeight = GetAdaptiveRowHeight(options.RowHeight);
        build.IsOpen = true;
        for (int& ordinal : build.SelectedOrdinals)
            ordinal = -1;

        BeginVStack({.Spacing = 0.0f,
                     .Width = options.Width,
                     .Height = options.Height,
                     .Background = options.HasBorder
                                       ? std::optional<Color>(GetStyleColor(StyleColor::ControlBackground))
                                       : std::nullopt,
                     .CornerRadius = CornerRadius,
                     .ID = id});
        PushID(id);
        build.StateID = GetID("##state");
        BeginScrollView("##columns", {.Axis = Axis::Horizontal, .Spacing = 0.0f});
    }

    void EndColumnView()
    {
        ColumnViewBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen && !build.IsInColumn, "EndColumnView does not match BeginColumnView");
        if (build.IsInColumn)
            EndColumnViewColumn();
        EndScrollView();

        // A column that appears is scrolled into view; the offset is clamped to the content on the next frame.
        ColumnViewState& state = GetViewState();
        if (build.ColumnCount > state.LastColumnCount)
            SetScrollOffset("##columns", Vec2(1.0e6f, 0.0f), true);
        state.LastColumnCount = build.ColumnCount;

        PopID();
        EndVStack();
        if (build.Options.HasBorder)
        {
            GetDrawList().AddSquircleStroke(GetLastItemRect(), GetStyleColor(StyleColor::ControlBorder), CornerRadius,
                                            GetContentScale().GetPixelSize(), GetStyleVar(StyleVar::CornerSmoothing));
        }
        build.IsOpen = false;
    }

    void BeginColumnViewColumn()
    {
        ColumnViewBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen && !build.IsInColumn && build.ColumnCount < MaxColumns,
                  "BeginColumnViewColumn must be called between BeginColumnView and EndColumnView, at most {} times",
                  MaxColumns);
        if (!build.IsOpen || build.IsInColumn || build.ColumnCount >= MaxColumns)
            return;
        ColumnViewState& state = GetViewState();
        const int index = build.ColumnCount;

        Internal::SelectionListDescription description;
        description.Scroll.Width = Size::Fixed(GetColumnWidth(state, build, index));
        description.Scroll.Height = Size::Fill();
        description.Scroll.Spacing = 0.0f;
        description.Scroll.Padding = EdgeInsets(ColumnPadding);
        PushID(index);
        Internal::BeginSelectionList("##column", description);
        build.ColumnIDs[index] = Internal::GetSelectionListID();
        build.SelectedHasChildren = false;
        build.IsInColumn = true;

        // Arriving here with the arrow keys: focus this column. What to pick is decided once its items are known.
        build.IsArriving = state.IsFocusPending && state.FocusColumn == index;
        if (build.IsArriving)
        {
            SetFocus(build.ColumnIDs[index], true);
            state.IsFocusPending = false;
        }
    }

    void EndColumnViewColumn()
    {
        ColumnViewBuild& build = GetBuild();
        CB_VERIFY(build.IsInColumn, "EndColumnViewColumn called without BeginColumnViewColumn");
        if (!build.IsInColumn)
            return;
        ColumnViewState& state = GetViewState();
        const int index = build.ColumnCount;

        if (build.IsArriving)
        {
            const int ordinal =
                state.PickOrdinal >= 0 ? state.PickOrdinal : (build.SelectedOrdinals[index] < 0 ? 0 : -1);
            if (ordinal >= 0)
                Internal::RequestSelectionListPick(ordinal);
        }

        // The left and right arrow keys move between columns. Moving right picks the first item of the next column
        // when nothing is selected there; moving left picks the parent again, which closes the columns after it.
        // The key that brought the focus here does not move it on.
        if (Internal::IsSelectionListFocused() && !build.IsArriving)
        {
            if (IsKeyPressed(Key::RightArrow) && build.SelectedOrdinals[index] >= 0 && build.SelectedHasChildren)
            {
                state.FocusColumn = index + 1;
                state.PickOrdinal = -1;
                state.IsFocusPending = true;
                RequestAnimationFrame();
            }
            if (IsKeyPressed(Key::LeftArrow) && index > 0)
            {
                SetFocus(build.ColumnIDs[index - 1], true);
                state.FocusColumn = index - 1;
                state.PickOrdinal = build.SelectedOrdinals[index - 1];
                state.IsFocusPending = true;
                RequestAnimationFrame();
            }
        }
        Internal::EndSelectionList();
        PopID();
        ColumnDivider(state, build, index);
        build.IsArriving = false;
        build.IsInColumn = false;
        build.ColumnCount++;
    }

    RowRange ClipColumnViewItems(int count, int selectedItem, bool selectedHasChildren)
    {
        ColumnViewBuild& build = GetBuild();
        CB_VERIFY(build.IsInColumn,
                  "ClipColumnViewItems must be called between BeginColumnViewColumn and EndColumnViewColumn");
        if (!build.IsInColumn)
            return RowRange();
        const RowRange range = Internal::ClipSelectionListRows(count, build.Options.RowHeight, selectedItem);
        // A selected item that is not submitted is still where the arrow keys start from.
        if (selectedItem >= 0 && selectedItem < count && (selectedItem < range.First || selectedItem >= range.End))
        {
            build.SelectedOrdinals[build.ColumnCount] = selectedItem;
            build.SelectedHasChildren = selectedHasChildren;
        }
        return range;
    }

    bool ColumnViewItem(std::string_view label, bool isSelected, const ColumnViewItemOptions& options)
    {
        ColumnViewBuild& build = GetBuild();
        CB_VERIFY(build.IsInColumn,
                  "Column view items must be added between BeginColumnViewColumn and EndColumnViewColumn");
        if (!build.IsInColumn)
            return false;

        // An item that is scrolled out of view needs no ID, which would mean hashing its label.
        const bool isVisible = Internal::IsNextSelectionListRowVisible(build.Options.RowHeight);
        const ID id = isVisible ? GetID(label) : ID();
        PushDisabled(options.Disabled);
        const Internal::SelectionRow row =
            Internal::SelectionListRow(id, build.Options.RowHeight, isSelected, options.Disabled);
        if (isSelected)
        {
            build.SelectedOrdinals[build.ColumnCount] = row.Ordinal;
            build.SelectedHasChildren = options.HasChildren;
        }
        if (!row.IsVisible)
        {
            PopDisabled();
            return row.Clicked;
        }

        DrawList& drawList = GetDrawList();
        const Color onAccent = GetStyleColor(StyleColor::OnAccent);
        const float centerY = row.Bounds.GetCenter().Y;
        float x = row.Bounds.X + RowPadding;
        if (!options.Icon.empty())
        {
            const Color tint = row.IsEmphasized ? onAccent : Resolve(options.IconTint, StyleColor::Accent);
            DrawIcon(drawList, Vec2(x + IconSize * 0.5f, centerY), options.Icon, IconSize, tint);
            x += IconSize + IconGap;
        }
        float right = row.Bounds.GetRight() - RowPadding;
        if (options.HasChildren)
        {
            DrawIcon(drawList, Vec2(right - ChevronSize * 0.5f, centerY), Icons::CaretRight, ChevronSize,
                     row.IsEmphasized ? onAccent : GetStyleColor(StyleColor::SecondaryLabel), IconVariant::Bold);
            right -= ChevronSize + IconGap;
        }
        TextSpec spec = GetTextSpec(TextStyle::Body);
        spec.MaxWidth = std::max(right - x, 1.0f);
        spec.Wraps = false;
        DrawLabel(drawList, row.Bounds, x, GetDisplayLabel(label), spec,
                  row.IsEmphasized ? onAccent : GetStyleColor(StyleColor::Label));
        PopDisabled();
        SetLastItem(id, row.Bounds, row.Interaction);
        return row.Clicked;
    }

    void BeginColumnViewPreview()
    {
        const ColumnViewBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen && !build.IsInColumn, "BeginColumnViewPreview must follow the last column");
        BeginVStack({.Spacing = 8.0f,
                     .Padding = 16.0f,
                     .Alignment = Alignment::Center,
                     .Width = Size::Fixed(build.Options.PreviewWidth),
                     .Height = Size::Fill(),
                     .ID = "##preview"});
    }

    void EndColumnViewPreview()
    {
        EndVStack();
    }
} // namespace Carbon
