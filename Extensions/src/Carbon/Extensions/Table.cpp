#include "Carbon/Extensions/Table.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Carbon/Extensions/Internal/BuildState.h"
#include "Carbon/Extensions/Internal/ColumnLayout.h"
#include "Carbon/Extensions/Internal/SelectionList.h"

namespace Carbon
{
    namespace
    {
        // Space around the rows, so that the selection highlight is inset from the table's edge.
        constexpr float RowInset = 5.0f;
        constexpr float RowInsetY = 3.0f;
        constexpr float TableCornerRadius = 7.0f;
        // The width around a column divider that the pointer can grab.
        constexpr float DividerGrabWidth = 8.0f;
        // Points scrolled sideways per wheel notch, as in scroll views.
        constexpr float WheelStep = 48.0f;
        constexpr AnimationSpec ScrollSpring = AnimationSpec::Spring(0.25f, 1.0f);
        // The horizontal scroll indicator, which looks and behaves like a scroll view's.
        constexpr float IndicatorHoldTime = 1.0f;
        constexpr float IndicatorMargin = 2.0f;
        constexpr float IndicatorMinLength = 24.0f;
        constexpr float IndicatorHoverGrowth = 4.0f;

        using Internal::CellPadding;
        using Internal::ColumnLayout;

        // Remembered per column of a table, also while the table is hidden.
        struct ColumnState
        {
            /// The width the user gave the column, or 0 while it keeps its declared width.
            float Width;
            /// Where the column is shown, plus one; 0 before the table has been arranged.
            int Position;
        };

        // Remembered per table.
        struct TableState
        {
            /// The sideways scroll offset the table is heading for; the displayed one follows with a spring.
            float ScrollX;
            /// Seconds since the table last scrolled sideways; the indicator fades after IndicatorHoldTime.
            float ScrollIdleTime;
            /// The offset when a drag of the indicator started.
            float ScrollDragStart;
            /// Where the table was last drawn: the wheel scrolls it while the pointer is over it.
            Rect Bounds;
            /// The width of a column when the drag of its divider started.
            float ResizeStartWidth;
            /// A column to fit to its content, plus one, and the width it needs. The cells are measured while the
            /// rows are added; the width is applied when the next frame begins.
            int FitColumn;
            float FitWidth;
        };

        // The table whose rows are being added.
        struct TableBuild
        {
            /// Indexed like the columns; points into s_Columns. X is relative to the rows' leading edge and
            /// includes the sideways scroll offset.
            ColumnLayout* Columns;
            int ColumnCount;
            int NextColumn;
            int RowIndex;
            Rect Row;
            float RowHeight;
            ID StateID;
            TableState* State;
            /// The visible area of the rows, including their inset.
            Rect Viewport;
            /// The width of all columns together, the width they are shown in, and the displayed and largest
            /// sideways scroll offsets.
            float ContentWidth;
            float VisibleWidth;
            float ScrollX;
            float MaxScrollX;
            /// The column being fitted to its content, or -1, and the widest of its text cells so far.
            int MeasuredColumn;
            float MeasuredWidth;
            bool IsRowEmphasized;
            /// The current row is inside the visible area: its text cells are drawn.
            bool IsRowVisible;
            bool ShowsAlternatingRows;
            /// A row is open: its ID is on the ID stack, so that the widgets in its cells are its own.
            bool HasRow;
        };

        Internal::BuildState<TableBuild> s_Build("Carbon.Table.Build");
        // Tables do not nest, so one table is built at a time. These grow with the most columns any table had and
        // are reused: a frame allocates only when a table has more columns than any before it.
        std::vector<ColumnLayout> s_Columns;
        std::vector<float> s_Widths;
        std::vector<int> s_Order;
        std::vector<uint8_t> s_Seen;

        TableBuild& GetBuild()
        {
            return s_Build.Get();
        }

        ColumnState& GetColumnState(ID table, size_t column)
        {
            return *GetState<ColumnState>(HashID(static_cast<int64_t>(column), table), StateLifetime::Persistent);
        }

        // True when `order` holds every index below its size exactly once.
        bool IsPermutation(std::span<const int> order)
        {
            const std::span<uint8_t> seen = Internal::GrowStorage(s_Seen, order.size());
            std::fill(seen.begin(), seen.end(), static_cast<uint8_t>(0));
            for (const int index : order)
            {
                if (index < 0 || static_cast<size_t>(index) >= order.size() || seen[static_cast<size_t>(index)] != 0)
                    return false;
                seen[static_cast<size_t>(index)] = 1;
            }
            return true;
        }

        // Reads the widths and the order of the columns into `widths` and `order`: from the application's storage
        // when it has some, from the table's memory otherwise.
        void LoadArrangement(ID table, const TableOptions& options, std::span<float> widths, std::span<int> order)
        {
            const size_t count = widths.size();
            const bool hasWidths = !options.ColumnWidths.empty();
            const bool hasOrder = !options.ColumnOrder.empty();
            CB_VERIFY(!hasWidths || options.ColumnWidths.size() == count,
                      "TableOptions::ColumnWidths has {} entries for {} columns", options.ColumnWidths.size(), count);
            CB_VERIFY(!hasOrder || options.ColumnOrder.size() == count,
                      "TableOptions::ColumnOrder has {} entries for {} columns", options.ColumnOrder.size(), count);

            std::fill(order.begin(), order.end(), -1);
            for (size_t i = 0; i < count; i++)
            {
                ColumnState& column = GetColumnState(table, i);
                if (hasWidths && options.ColumnWidths.size() == count)
                    column.Width = std::max(options.ColumnWidths[i], 0.0f);
                widths[i] = column.Width;
                if (column.Position > 0 && static_cast<size_t>(column.Position) <= count)
                    order[static_cast<size_t>(column.Position - 1)] = static_cast<int>(i);
            }

            if (hasOrder && options.ColumnOrder.size() == count)
                std::copy(options.ColumnOrder.begin(), options.ColumnOrder.end(), order.begin());
            if (IsPermutation(order))
                return;
            // A new table, columns that were added or removed, or an order that is no permutation: the columns are
            // shown as declared.
            for (size_t i = 0; i < count; i++)
                order[i] = static_cast<int>(i);
            for (size_t i = 0; i < count; i++)
                GetColumnState(table, i).Position = static_cast<int>(i) + 1;
            if (hasOrder && options.ColumnOrder.size() == count)
                std::copy(order.begin(), order.end(), options.ColumnOrder.begin());
        }

        // Gives a column the width the user chose, in the table's memory and in the application's storage.
        bool StoreWidth(ID table, const TableOptions& options, std::span<float> widths, size_t column, float width)
        {
            if (widths[column] == width)
                return false;
            widths[column] = width;
            GetColumnState(table, column).Width = width;
            if (options.ColumnWidths.size() == widths.size())
                options.ColumnWidths[column] = width;
            return true;
        }

        // The width a column needs for its title.
        float MeasureTitle(const TableColumn& column)
        {
            TextSpec spec = GetTextSpec(TextStyle::Subheadline, true);
            spec.Wraps = false;
            return MeasureText(column.Title, spec).X + CellPadding * 2.0f;
        }

        // The column dividers: dragging one resizes the column before it, double-clicking it fits that column to
        // its content.
        bool UpdateDividers(TableBuild& build, std::span<const TableColumn> columns, const TableOptions& options,
                            const Rect& header, std::span<float> widths, std::span<const int> order)
        {
            TableState& state = *build.State;
            bool changed = false;
            DragBehaviorOptions behavior;
            behavior.Focusable = false;
            GetDrawList().PushClipRect(header);
            for (const int index : order)
            {
                const size_t column = static_cast<size_t>(index);
                if (!columns[column].IsResizable)
                    continue;
                const ColumnLayout& layout = build.Columns[column];
                const float edge = header.X + RowInset + layout.X - build.ScrollX + layout.Width;
                const Rect grab(edge - DividerGrabWidth * 0.5f, header.Y, DividerGrabWidth, header.Height);
                const DragInteraction drag =
                    DragBehavior(HashID("##divider", HashID(index, build.StateID)), grab, behavior);
                if (drag.Hovered || drag.Active)
                    SetCursor(Cursor::ResizeHorizontal);
                if (drag.Started)
                {
                    state.ResizeStartWidth = layout.Width;
                    if (GetMouseClickCount() == 2)
                    {
                        // Fitted once the rows have been measured; the header's title counts too.
                        state.FitColumn = index + 1;
                        build.MeasuredColumn = index;
                        build.MeasuredWidth = MeasureTitle(columns[column]);
                    }
                }
                else if (drag.Active)
                {
                    const float width =
                        std::max(state.ResizeStartWidth + drag.Total.X, std::max(columns[column].MinWidth, 0.0f));
                    changed = StoreWidth(build.StateID, options, widths, column, width) || changed;
                }
            }
            GetDrawList().PopClipRect();
            return changed;
        }

        void DrawHeader(const TableBuild& build, std::span<const TableColumn> columns, const Rect& header,
                        std::span<const int> order)
        {
            DrawList& drawList = GetDrawList();
            const float pixel = GetContentScale().GetPixelSize();
            TextSpec spec = GetTextSpec(TextStyle::Subheadline, true);
            spec.Wraps = false;
            const Color titleColor = GetStyleColor(StyleColor::SecondaryLabel);
            const Color separator = GetStyleColor(StyleColor::Separator);
            drawList.PushClipRect(header);
            for (size_t position = 0; position < order.size(); position++)
            {
                const size_t column = static_cast<size_t>(order[position]);
                const ColumnLayout& layout = build.Columns[column];
                const float x = header.X + RowInset + layout.X;
                if (x > header.GetRight() || x + layout.Width < header.X)
                    continue;
                spec.MaxWidth = std::max(layout.Width - CellPadding * 2.0f, 1.0f);
                spec.Alignment = layout.Alignment;
                DrawLabel(drawList, header, x + CellPadding, columns[column].Title, spec, titleColor);
                if (position > 0)
                    drawList.AddRect(Rect(x, header.Y + 5.0f, pixel, header.Height - 10.0f), separator);
            }
            drawList.PopClipRect();
            drawList.AddRect(Rect(header.X, header.GetBottom() - pixel, header.Width, pixel), separator);
        }

        // The sideways scroll indicator at the bottom of the rows: shown while the table scrolls sideways or the
        // pointer is over it, and draggable.
        void UpdateScrollIndicator(TableBuild& build)
        {
            TableState& state = *build.State;
            state.ScrollIdleTime = std::min(state.ScrollIdleTime + GetDeltaTime(), IndicatorHoldTime * 2.0f);
            const Rect& viewport = build.Viewport;
            const float trackLength = viewport.Width - IndicatorMargin * 2.0f;
            if (build.MaxScrollX <= 0.0f || trackLength <= 0.0f)
                return;

            const float thickness = GetStyleVar(StyleVar::ScrollIndicatorWidth);
            const float contentLength = build.ContentWidth + RowInset * 2.0f;
            const float thumbLength = std::clamp(trackLength * viewport.Width / contentLength,
                                                 std::min(IndicatorMinLength, trackLength), trackLength);
            const float thumbRange = trackLength - thumbLength;
            const float travel = thumbRange * std::clamp(build.ScrollX / build.MaxScrollX, 0.0f, 1.0f);
            const float laneHeight = thickness + IndicatorMargin * 2.0f + IndicatorHoverGrowth;
            const Rect lane(viewport.X, viewport.GetBottom() - laneHeight, viewport.Width, laneHeight);
            const Rect grip(viewport.X + IndicatorMargin + travel, lane.Y, thumbLength, laneHeight);

            DragBehaviorOptions behavior;
            behavior.Focusable = false;
            const DragInteraction drag = DragBehavior(HashID("##hscroll", build.StateID), grip, behavior);
            if (drag.Started)
                state.ScrollDragStart = state.ScrollX;
            if (drag.Active && thumbRange > 0.0f)
            {
                state.ScrollX = std::clamp(state.ScrollDragStart + drag.Total.X * build.MaxScrollX / thumbRange, 0.0f,
                                           build.MaxScrollX);
                // The rows follow the thumb directly.
                SetAnimationValue(HashID("##scrollx", build.StateID), state.ScrollX);
            }
            const bool isLaneHovered = drag.Active || (IsRectHovered(lane) && !GetActiveID().IsValid());
            if (isLaneHovered)
                state.ScrollIdleTime = 0.0f;

            const bool isShown = state.ScrollIdleTime < IndicatorHoldTime;
            if (isShown)
                RequestFrameAfter(IndicatorHoldTime - state.ScrollIdleTime);
            const float opacity =
                Animate(HashID("##hindicator", build.StateID), isShown ? 1.0f : 0.0f, AnimationSpec::Fade(0.25f));
            if (opacity <= 0.0f)
                return;
            const float growth = Animate(HashID("##hindicatorwidth", build.StateID),
                                         isLaneHovered ? IndicatorHoverGrowth : 0.0f, AnimationSpec::Spring(0.2f));
            const float width = thickness + growth;
            const Rect thumb(viewport.X + IndicatorMargin + travel, viewport.GetBottom() - IndicatorMargin - width,
                             thumbLength, width);
            GetDrawList().AddSquircle(thumb, GetStyleColor(StyleColor::ScrollIndicator).WithOpacity(opacity),
                                      width * 0.5f, 0.0f);
        }

        // The rectangle of the next cell of the current row, without its padding. Empty when the row is full.
        Rect TakeCell(TableBuild& build)
        {
            const bool hasColumn = build.NextColumn < build.ColumnCount;
            CB_VERIFY(hasColumn, "The table row has more cells than the table has columns");
            if (!hasColumn)
                return Rect();
            const ColumnLayout& column = build.Columns[build.NextColumn];
            return Rect(build.Row.X + column.X + CellPadding, build.Row.Y,
                        std::max(column.Width - CellPadding * 2.0f, 0.0f), build.Row.Height);
        }
    } // namespace

    TableChanges BeginTable(std::string_view id, std::span<const TableColumn> columns, const TableOptions& options)
    {
        TableChanges changes;
        TableBuild& build = s_Build.Begin();
        build = TableBuild();
        build.RowHeight = options.RowHeight;
        build.ShowsAlternatingRows = options.ShowsAlternatingRows;
        build.MeasuredColumn = -1;
        build.StateID = HashID("##table", GetID(id));
        TableState& state = *GetState<TableState>(build.StateID, StateLifetime::Persistent);
        build.State = &state;

        const size_t count = columns.size();
        const std::span<ColumnLayout> layouts = Internal::GrowStorage(s_Columns, count);
        const std::span<float> widths = Internal::GrowStorage(s_Widths, count);
        const std::span<int> order = Internal::GrowStorage(s_Order, count);
        LoadArrangement(build.StateID, options, widths, order);
        build.Columns = layouts.data();
        build.ColumnCount = static_cast<int>(count);

        // A column fitted to its content during the last frame.
        if (state.FitColumn > 0 && static_cast<size_t>(state.FitColumn) <= count)
        {
            const size_t column = static_cast<size_t>(state.FitColumn - 1);
            const float width = std::max(state.FitWidth, std::max(columns[column].MinWidth, 0.0f));
            changes.WidthsChanged = StoreWidth(build.StateID, options, widths, column, width);
        }
        state.FitColumn = 0;

        // Sideways scrolling: a horizontal wheel, a trackpad or Shift with the wheel, over the table.
        const float wheel = GetMouseWheel().X;
        if (wheel != 0.0f && IsRectHovered(state.Bounds))
        {
            state.ScrollX -= wheel * WheelStep;
            state.ScrollIdleTime = 0.0f;
        }

        // The frame around header and rows is drawn first and sized when the table ends.
        BeginVStack({.Spacing = 0.0f,
                     .Width = options.Width,
                     .Height = options.Height,
                     .Background = GetStyleColor(StyleColor::ControlBackground),
                     .CornerRadius = TableCornerRadius,
                     .ID = id});
        PushID(id);

        build.VisibleWidth = std::max(GetContentRect().Width - RowInset * 2.0f, 0.0f);
        build.ContentWidth = Internal::LayoutColumns(columns, build.VisibleWidth, layouts, widths, order);
        build.MaxScrollX = std::max(build.ContentWidth - build.VisibleWidth, 0.0f);
        state.ScrollX = std::clamp(state.ScrollX, 0.0f, build.MaxScrollX);
        build.ScrollX =
            GetContentScale().Snap(Animate(HashID("##scrollx", build.StateID), state.ScrollX, ScrollSpring));

        Rect header;
        if (options.ShowsHeader)
        {
            ItemOptions item;
            item.Width = Size::Fill();
            header = AllocateItem(Vec2(0.0f, Internal::ColumnHeaderHeight), item);
            if (UpdateDividers(build, columns, options, header, widths, order))
            {
                changes.WidthsChanged = true;
                build.ContentWidth = Internal::LayoutColumns(columns, build.VisibleWidth, layouts, widths, order);
                build.MaxScrollX = std::max(build.ContentWidth - build.VisibleWidth, 0.0f);
                state.ScrollX = std::clamp(state.ScrollX, 0.0f, build.MaxScrollX);
            }
        }
        build.ScrollX = std::min(build.ScrollX, build.MaxScrollX);
        for (ColumnLayout& layout : layouts)
            layout.X -= build.ScrollX;
        if (options.ShowsHeader)
            DrawHeader(build, columns, header, order);

        Internal::SelectionListDescription description;
        description.Scroll.Spacing = 0.0f;
        description.Scroll.Padding = EdgeInsets(RowInset, RowInsetY);
        Internal::BeginSelectionList("##rows", description);
        const Rect content = Internal::GetSelectionListContentRect();
        build.Viewport = Rect(content.X - RowInset, content.Y - RowInsetY, content.Width + RowInset * 2.0f,
                              content.Height + RowInsetY * 2.0f);
        return changes;
    }

    RowRange ClipTableRows(int count, int selectedRow)
    {
        TableBuild& build = GetBuild();
        const RowRange range = Internal::ClipSelectionListRows(count, build.RowHeight, selectedRow);
        // Every other row is tinted, counted from the first row of the table.
        build.RowIndex += range.First;
        return range;
    }

    void EndTable()
    {
        TableBuild& build = GetBuild();
        if (build.HasRow)
            PopID();
        build.HasRow = false;
        Internal::EndSelectionList();
        UpdateScrollIndicator(build);
        PopID();
        EndVStack();
        TableState& state = *build.State;
        state.Bounds = GetLastItemRect();
        // The outline is drawn last, over the rows' edges.
        GetDrawList().AddSquircleStroke(state.Bounds, GetStyleColor(StyleColor::ControlBorder), TableCornerRadius,
                                        GetContentScale().GetPixelSize(), GetStyleVar(StyleVar::CornerSmoothing));

        if (build.MeasuredColumn >= 0)
        {
            // Fitting a column: the width is applied in the next frame, which must come.
            state.FitWidth = build.MeasuredWidth;
            RequestAnimationFrame();
        }
    }

    bool TableRow(int64_t id, bool isSelected)
    {
        TableBuild& build = GetBuild();
        // A row ends where the next one starts.
        if (build.HasRow)
            PopID();
        const ID rowID = GetID(id);
        const Internal::SelectionRow row = Internal::SelectionListRow(rowID, build.RowHeight, isSelected, false);
        PushID(rowID);
        build.HasRow = true;
        build.Row = row.Bounds;
        build.NextColumn = 0;
        build.IsRowEmphasized = row.IsEmphasized;
        build.IsRowVisible = row.IsVisible;

        // The selected row has its highlight; every other one of the others is tinted.
        if (row.IsVisible && build.ShowsAlternatingRows && !isSelected && build.RowIndex % 2 == 1)
        {
            GetDrawList().AddSquircle(row.Bounds, GetStyleColor(StyleColor::ControlFill).WithOpacity(0.35f), 5.0f,
                                      GetStyleVar(StyleVar::CornerSmoothing));
        }
        build.RowIndex++;
        return row.Clicked;
    }

    bool TableRow(std::string_view id, bool isSelected)
    {
        return TableRow(static_cast<int64_t>(GetID(id).Value), isSelected);
    }

    void TableCell(std::string_view text, const TableCellOptions& options)
    {
        TableBuild& build = GetBuild();
        const Rect cell = TakeCell(build);
        if (build.NextColumn >= build.ColumnCount)
            return;
        const int column = build.NextColumn++;
        if (!build.IsRowVisible)
            return;
        if (column == build.MeasuredColumn)
        {
            build.MeasuredWidth =
                std::max(build.MeasuredWidth, Internal::MeasureCellText(text, options) + CellPadding * 2.0f);
        }
        // Columns scrolled out of view sideways are not drawn.
        if (cell.GetRight() <= build.Viewport.X || cell.X >= build.Viewport.GetRight())
            return;
        Internal::DrawCellText(cell, text, options, build.Columns[column].Alignment, build.IsRowEmphasized);
    }

    void BeginTableCell()
    {
        TableBuild& build = GetBuild();
        const Rect cell = TakeCell(build);
        build.NextColumn = std::min(build.NextColumn + 1, build.ColumnCount);
        // The cell is placed by hand; the row has already taken its space in the list.
        SetCursorPos(cell.GetMin());
        BeginHStack({.Width = Size::Fixed(cell.Width), .Height = Size::Fixed(cell.Height)});
    }

    void EndTableCell()
    {
        EndHStack();
    }
} // namespace Carbon
