#include "Carbon/Extensions/Table.h"

#include <algorithm>
#include <cmath>
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
        // The chevron that marks the column the rows are sorted by.
        constexpr float SortIndicatorSize = 9.0f;
        constexpr float SortIndicatorGap = 3.0f;
        // A header has to be dragged this far before its column moves; less is a click.
        constexpr float ReorderThreshold = 4.0f;
        // Columns that make room for a moved one slide there on a spring, for about this long.
        constexpr AnimationSpec SlideSpring = AnimationSpec::Spring(0.3f, 0.86f);
        constexpr float SlideDuration = 0.8f;
        // Near the table's edges, a column being moved scrolls the table, up to this fast in points per second.
        constexpr float AutoScrollZone = 24.0f;
        constexpr float AutoScrollSpeed = 900.0f;

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
            /// The column the header's keyboard focus is on, as a position on screen.
            int FocusedPosition;
            /// The column being moved by its header, plus one, while the user drags it.
            int MovedColumn;
            /// Where the pointer grabbed the moved column, from the column's leading edge.
            float GrabOffset;
            /// Seconds since the columns were last rearranged; they slide into place meanwhile.
            float SlideTime;
            bool IsSliding;
        };

        // The table whose rows are being added.
        struct TableBuild
        {
            /// Indexed like the columns; points into s_Columns. X is relative to the rows' leading edge and
            /// includes the sideways scroll offset and the slide of a column that moves.
            ColumnLayout* Columns;
            int ColumnCount;
            int NextColumn;
            int RowIndex;
            Rect Row;
            float RowHeight;
            ID StateID;
            TableState* State;
            /// The header's stop for Tab, in a table that sorts.
            ID HeaderID;
            TableSort Sort;
            bool IsSorting;
            bool HasHeaderStop;
            /// The column whose header is held down, or -1.
            int PressedColumn;
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
            /// The column being moved, or -1. It floats above the others: their cells are cut where it is.
            int MovedColumn;
            /// The cell of BeginTableCell clips its content.
            bool IsCellClipped;
        };

        // The columns of the table being begun and the storage that arranges them, all indexed like the columns
        // except Order, which lists them as shown.
        struct Arrangement
        {
            std::span<const TableColumn> Columns;
            const TableOptions* Options = nullptr;
            std::span<ColumnLayout> Layouts;
            std::span<float> Widths;
            std::span<int> Order;
            /// How far each column is drawn from its place while columns slide.
            std::span<float> Offsets;
        };

        Internal::BuildState<TableBuild> s_Build("Carbon.Table.Build");
        // Tables do not nest, so one table is built at a time. These grow with the most columns any table had and
        // are reused: a frame allocates only when a table has more columns than any before it.
        std::vector<ColumnLayout> s_Columns;
        std::vector<float> s_Widths;
        std::vector<int> s_Order;
        std::vector<float> s_Offsets;
        std::vector<uint8_t> s_Seen;

        TableBuild& GetBuild()
        {
            return s_Build.Get();
        }

        ColumnState& GetColumnState(ID table, size_t column)
        {
            return *GetState<ColumnState>(HashID(static_cast<int64_t>(column), table), StateLifetime::Persistent);
        }

        ID GetSlideID(ID table, int column)
        {
            return HashID("##slide", HashID(column, table));
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

        // Reads the widths and the order of the columns: from the application's storage when it has some, from
        // the table's memory otherwise. While the user moves a column, the table's memory has the order.
        void LoadArrangement(const TableBuild& build, const Arrangement& arrangement)
        {
            const TableOptions& options = *arrangement.Options;
            const size_t count = arrangement.Columns.size();
            const bool hasWidths = !options.ColumnWidths.empty();
            const bool hasOrder = !options.ColumnOrder.empty();
            CB_VERIFY(!hasWidths || options.ColumnWidths.size() == count,
                      "TableOptions::ColumnWidths has {} entries for {} columns", options.ColumnWidths.size(), count);
            CB_VERIFY(!hasOrder || options.ColumnOrder.size() == count,
                      "TableOptions::ColumnOrder has {} entries for {} columns", options.ColumnOrder.size(), count);
            const bool usesWidths = hasWidths && options.ColumnWidths.size() == count;
            const bool usesOrder = hasOrder && options.ColumnOrder.size() == count && build.State->MovedColumn == 0;

            std::fill(arrangement.Order.begin(), arrangement.Order.end(), -1);
            for (size_t i = 0; i < count; i++)
            {
                ColumnState& column = GetColumnState(build.StateID, i);
                if (usesWidths)
                    column.Width = std::max(options.ColumnWidths[i], 0.0f);
                arrangement.Widths[i] = column.Width;
                if (column.Position > 0 && static_cast<size_t>(column.Position) <= count)
                    arrangement.Order[static_cast<size_t>(column.Position - 1)] = static_cast<int>(i);
            }
            if (usesOrder)
                std::copy(options.ColumnOrder.begin(), options.ColumnOrder.end(), arrangement.Order.begin());

            if (!IsPermutation(arrangement.Order))
            {
                // A new table, columns that were added or removed, or an order that is no permutation: the columns
                // are shown as declared.
                for (size_t i = 0; i < count; i++)
                    arrangement.Order[i] = static_cast<int>(i);
                if (usesOrder)
                    std::copy(arrangement.Order.begin(), arrangement.Order.end(), options.ColumnOrder.begin());
            }
            for (size_t position = 0; position < count; position++)
            {
                const size_t column = static_cast<size_t>(arrangement.Order[position]);
                GetColumnState(build.StateID, column).Position = static_cast<int>(position) + 1;
            }
        }

        // Lays the columns out at their places, without the sideways scroll offset.
        void Layout(TableBuild& build, const Arrangement& arrangement)
        {
            build.ContentWidth = Internal::LayoutColumns(arrangement.Columns, build.VisibleWidth, arrangement.Layouts,
                                                         arrangement.Widths, arrangement.Order);
            build.MaxScrollX = std::max(build.ContentWidth - build.VisibleWidth, 0.0f);
        }

        // Gives a column the width the user chose, in the table's memory and in the application's storage.
        bool StoreWidth(const TableBuild& build, const Arrangement& arrangement, size_t column, float width)
        {
            if (arrangement.Widths[column] == width)
                return false;
            arrangement.Widths[column] = width;
            GetColumnState(build.StateID, column).Width = width;
            const TableOptions& options = *arrangement.Options;
            if (options.ColumnWidths.size() == arrangement.Widths.size())
                options.ColumnWidths[column] = width;
            return true;
        }

        // Writes the order the user arranged to the application's storage.
        void StoreOrder(const Arrangement& arrangement)
        {
            const TableOptions& options = *arrangement.Options;
            if (options.ColumnOrder.size() == arrangement.Order.size())
                std::copy(arrangement.Order.begin(), arrangement.Order.end(), options.ColumnOrder.begin());
        }

        // Moves a column to another position on screen. The columns are laid out anew, and every column that
        // changed its place slides there from where it is drawn now.
        void MoveColumn(TableBuild& build, const Arrangement& arrangement, int column, int position)
        {
            const auto current = std::find(arrangement.Order.begin(), arrangement.Order.end(), column);
            const auto target = arrangement.Order.begin() + position;
            if (current == arrangement.Order.end() || current == target)
                return;
            if (current < target)
                std::rotate(current, current + 1, target + 1);
            else
                std::rotate(target, current, current + 1);

            const size_t count = arrangement.Columns.size();
            for (size_t i = 0; i < count; i++)
                arrangement.Offsets[i] += arrangement.Layouts[i].X;
            Layout(build, arrangement);
            for (size_t i = 0; i < count; i++)
            {
                arrangement.Offsets[i] -= arrangement.Layouts[i].X;
                SetAnimationValue(GetSlideID(build.StateID, static_cast<int>(i)), arrangement.Offsets[i]);
                GetColumnState(build.StateID, i).Position = 0;
            }
            for (size_t i = 0; i < count; i++)
            {
                const size_t moved = static_cast<size_t>(arrangement.Order[i]);
                GetColumnState(build.StateID, moved).Position = static_cast<int>(i) + 1;
            }
            build.State->IsSliding = true;
            build.State->SlideTime = 0.0f;
        }

        // The position among the others at which the moved column belongs: before the first column whose middle
        // its own middle has not passed.
        int FindDropPosition(const Arrangement& arrangement, int moved, float center)
        {
            int position = 0;
            float x = 0.0f;
            for (const int index : arrangement.Order)
            {
                if (index == moved)
                    continue;
                const float width = arrangement.Layouts[static_cast<size_t>(index)].Width;
                if (center > x + width * 0.5f)
                    position++;
                x += width;
            }
            return position;
        }

        // The width a column needs for its title, and for the sort indicator when the rows are sorted by it.
        float MeasureTitle(const TableBuild& build, const TableColumn& column, int index)
        {
            TextSpec spec = GetTextSpec(TextStyle::Subheadline, true);
            spec.Wraps = false;
            const bool isSorted = build.IsSorting && build.Sort.Column == index;
            return MeasureText(column.Title, spec).X + CellPadding * 2.0f +
                   (isSorted ? SortIndicatorSize + SortIndicatorGap : 0.0f);
        }

        // Sorts by a column, or flips the direction when the rows are sorted by it already.
        void SortBy(TableSort& sort, int column, const TableColumn& declaration)
        {
            if (sort.Column == column)
            {
                sort.Direction =
                    sort.Direction == SortDirection::Ascending ? SortDirection::Descending : SortDirection::Ascending;
            }
            else
            {
                sort.Column = column;
                sort.Direction = declaration.InitialSortDirection;
            }
        }

        // Scrolls sideways just far enough to show a column completely.
        void RevealColumn(const TableBuild& build, const Arrangement& arrangement, int column)
        {
            TableState& state = *build.State;
            const ColumnLayout& layout = arrangement.Layouts[static_cast<size_t>(column)];
            if (layout.X < state.ScrollX)
                state.ScrollX = layout.X;
            else if (layout.X + layout.Width > state.ScrollX + build.VisibleWidth)
                state.ScrollX = std::min(layout.X, layout.X + layout.Width - build.VisibleWidth);
        }

        // The header's stop for Tab: the arrow keys move between the columns, Space or Enter sorts by one, and
        // with the shortcut modifier the arrow keys move a column.
        void UpdateHeaderKeyboard(TableBuild& build, const Arrangement& arrangement, const Rect& header,
                                  TableChanges& changes)
        {
            TableState& state = *build.State;
            const int count = static_cast<int>(arrangement.Order.size());
            RegisterFocusable(build.HeaderID, header);
            state.FocusedPosition = std::clamp(state.FocusedPosition, 0, count - 1);
            if (!IsFocused(build.HeaderID) || IsDisabled() || state.MovedColumn != 0)
                return;

            const int column = arrangement.Order[static_cast<size_t>(state.FocusedPosition)];
            const TableColumn& declaration = arrangement.Columns[static_cast<size_t>(column)];
            const bool isMoving = IsShortcutPressed(Key::LeftArrow) || IsShortcutPressed(Key::RightArrow);
            int step = 0;
            if (IsKeyPressed(Key::LeftArrow))
                step = -1;
            if (IsKeyPressed(Key::RightArrow))
                step = 1;
            const int target = std::clamp(state.FocusedPosition + step, 0, count - 1);
            if (target != state.FocusedPosition)
            {
                if (isMoving && declaration.IsReorderable)
                {
                    MoveColumn(build, arrangement, column, target);
                    StoreOrder(arrangement);
                    changes.OrderChanged = true;
                }
                state.FocusedPosition = target;
                RevealColumn(build, arrangement, arrangement.Order[static_cast<size_t>(target)]);
            }

            const bool isActivated = IsKeyPressed(Key::Space, false) || IsKeyPressed(Key::Enter, false) ||
                                     IsKeyPressed(Key::KeypadEnter, false);
            if (isActivated && build.IsSorting && declaration.IsSortable)
            {
                SortBy(*arrangement.Options->Sort, column, declaration);
                changes.SortChanged = true;
            }
        }

        // The header's pointer interaction: clicking a column sorts by it, dragging one moves it, dragging a
        // divider resizes the column before it, and double-clicking a divider fits that column to its content.
        // Column positions are not scrolled yet.
        TableChanges UpdateHeader(TableBuild& build, const Arrangement& arrangement, const Rect& header)
        {
            TableChanges changes;
            TableState& state = *build.State;
            const size_t count = arrangement.Order.size();
            if (count == 0)
                return changes;
            if (build.HasHeaderStop)
                UpdateHeaderKeyboard(build, arrangement, header, changes);

            DragBehaviorOptions behavior;
            behavior.Focusable = false;
            GetDrawList().PushClipRect(header);
            // Where the pointer is, in the coordinates of the columns.
            const float pointer = GetMousePos().X - header.X - RowInset + build.ScrollX;

            // The columns themselves: a click sorts, a drag moves the column.
            for (size_t position = 0; position < count; position++)
            {
                const int index = arrangement.Order[position];
                const TableColumn& column = arrangement.Columns[static_cast<size_t>(index)];
                const bool sorts = build.IsSorting && column.IsSortable;
                if (!sorts && !column.IsReorderable)
                    continue;
                const ColumnLayout& layout = arrangement.Layouts[static_cast<size_t>(index)];
                const Rect cell(header.X + RowInset + layout.X - build.ScrollX, header.Y, layout.Width, header.Height);
                const DragInteraction drag =
                    DragBehavior(HashID("##column", HashID(index, build.StateID)), cell, behavior);
                if (drag.Started)
                    state.GrabOffset = pointer - layout.X;
                const bool isMoved = state.MovedColumn == index + 1;
                if (drag.Active && !isMoved && column.IsReorderable && std::abs(drag.Total.X) >= ReorderThreshold &&
                    state.MovedColumn == 0)
                {
                    state.MovedColumn = index + 1;
                }
                else if (drag.Active && !isMoved && sorts)
                {
                    build.PressedColumn = index;
                }
                if (drag.Ended && isMoved)
                {
                    // Dropped: the column slides from the pointer into its place, and the order is reported.
                    state.MovedColumn = 0;
                    state.IsSliding = true;
                    state.SlideTime = 0.0f;
                    StoreOrder(arrangement);
                    changes.OrderChanged = true;
                }
                else if (drag.Ended && sorts && IsRectHovered(cell))
                {
                    SortBy(*arrangement.Options->Sort, index, column);
                    changes.SortChanged = true;
                    state.FocusedPosition = static_cast<int>(position);
                }
            }

            // The column being moved follows the pointer; the others make room once its middle passes theirs.
            if (state.MovedColumn > 0 && static_cast<size_t>(state.MovedColumn) <= count)
            {
                const int moved = state.MovedColumn - 1;
                const ColumnLayout& layout = arrangement.Layouts[static_cast<size_t>(moved)];
                const float x =
                    std::clamp(pointer - state.GrabOffset, 0.0f, std::max(build.ContentWidth - layout.Width, 0.0f));
                MoveColumn(build, arrangement, moved, FindDropPosition(arrangement, moved, x + layout.Width * 0.5f));
                arrangement.Offsets[static_cast<size_t>(moved)] = x - layout.X;
                SetAnimationValue(GetSlideID(build.StateID, moved), x - layout.X);

                // Near the table's edges, the table scrolls towards the hidden columns.
                const float mouse = GetMousePos().X;
                const float leading = header.X + AutoScrollZone - mouse;
                const float trailing = mouse - (header.GetRight() - AutoScrollZone);
                const float depth = std::max(leading, trailing) / AutoScrollZone;
                if (depth > 0.0f)
                {
                    const float distance = AutoScrollSpeed * std::min(depth, 1.0f) * GetDeltaTime();
                    state.ScrollX += leading > 0.0f ? -distance : distance;
                    state.ScrollIdleTime = 0.0f;
                    RequestAnimationFrame();
                }
            }
            else
            {
                state.MovedColumn = 0;
            }

            // The dividers, after the columns so that they win the pointer where both are.
            for (const int index : arrangement.Order)
            {
                const size_t column = static_cast<size_t>(index);
                if (!arrangement.Columns[column].IsResizable || state.MovedColumn != 0)
                    continue;
                const ColumnLayout& layout = arrangement.Layouts[column];
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
                        build.MeasuredWidth = MeasureTitle(build, arrangement.Columns[column], index);
                    }
                }
                else if (drag.Active)
                {
                    const float minimum = std::max(arrangement.Columns[column].MinWidth, 0.0f);
                    const float width = std::max(state.ResizeStartWidth + drag.Total.X, minimum);
                    changes.WidthsChanged = StoreWidth(build, arrangement, column, width) || changes.WidthsChanged;
                }
            }
            GetDrawList().PopClipRect();
            if (changes.SortChanged)
                build.Sort = *arrangement.Options->Sort;
            return changes;
        }

        void DrawHeaderColumn(const TableBuild& build, const TableColumn& column, int index, const Rect& header,
                              size_t position, bool isMoved)
        {
            DrawList& drawList = GetDrawList();
            const ColumnLayout& layout = build.Columns[index];
            const Rect cell(header.X + RowInset + layout.X, header.Y, layout.Width, header.Height);
            if (cell.X > header.GetRight() || cell.GetRight() < header.X)
                return;
            const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
            if (isMoved)
            {
                // The column being moved floats above the others.
                drawList.AddShadow(cell, GetStyleColor(StyleColor::Shadow), 4.0f, 8.0f, Vec2(0.0f, 1.0f));
                drawList.AddSquircle(cell, GetStyleColor(StyleColor::ControlBackground), 4.0f, smoothing);
                drawList.AddSquircle(cell, GetStyleColor(StyleColor::ControlFill), 4.0f, smoothing);
            }
            else if (index == build.PressedColumn)
            {
                drawList.AddSquircle(cell.Inset(EdgeInsets(1.0f, 2.0f)), GetStyleColor(StyleColor::ControlFill), 4.0f,
                                     smoothing);
            }

            TextSpec spec = GetTextSpec(TextStyle::Subheadline, true);
            spec.Wraps = false;
            const Color titleColor = GetStyleColor(StyleColor::SecondaryLabel);
            // The sorted column shows a chevron at its trailing edge: up for ascending, down for descending.
            float titleWidth = layout.Width - CellPadding * 2.0f;
            if (build.IsSorting && build.Sort.Column == index)
            {
                const std::string_view icon =
                    build.Sort.Direction == SortDirection::Ascending ? Icons::CaretUp : Icons::CaretDown;
                const Vec2 center(cell.GetRight() - CellPadding - SortIndicatorSize * 0.5f, cell.GetCenter().Y);
                DrawIcon(drawList, center, icon, SortIndicatorSize, titleColor, IconVariant::Bold);
                titleWidth -= SortIndicatorSize + SortIndicatorGap;
            }
            spec.MaxWidth = std::max(titleWidth, 1.0f);
            spec.Alignment = layout.Alignment;
            DrawLabel(drawList, header, cell.X + CellPadding, column.Title, spec, titleColor);
            if (position > 0 && !isMoved)
            {
                drawList.AddRect(Rect(cell.X, header.Y + 5.0f, GetContentScale().GetPixelSize(), header.Height - 10.0f),
                                 GetStyleColor(StyleColor::Separator));
            }
            if (build.HasHeaderStop && static_cast<int>(position) == build.State->FocusedPosition &&
                IsFocusVisible(build.HeaderID))
            {
                DrawFocusRing(build.HeaderID, cell.Inset(EdgeInsets(2.0f, 3.0f)), 4.0f);
            }
        }

        void DrawHeader(const TableBuild& build, std::span<const TableColumn> columns, const Rect& header,
                        std::span<const int> order)
        {
            DrawList& drawList = GetDrawList();
            const int moved = build.State->MovedColumn - 1;
            drawList.PushClipRect(header);
            for (size_t position = 0; position < order.size(); position++)
            {
                if (order[position] != moved)
                    DrawHeaderColumn(build, columns[static_cast<size_t>(order[position])], order[position], header,
                                     position, false);
            }
            // The column being moved is drawn last, above the ones it passes.
            const auto movedPosition = std::find(order.begin(), order.end(), moved);
            if (movedPosition != order.end())
            {
                DrawHeaderColumn(build, columns[static_cast<size_t>(moved)], moved, header,
                                 static_cast<size_t>(movedPosition - order.begin()), true);
            }
            drawList.PopClipRect();
            const float pixel = GetContentScale().GetPixelSize();
            drawList.AddRect(Rect(header.X, header.GetBottom() - pixel, header.Width, pixel),
                             GetStyleColor(StyleColor::Separator));
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

        // The part of the current row's cell in `column` that the column being moved leaves visible, or the whole
        // cell when no column is being moved. When the moved column lies inside the cell, the wider side remains.
        Rect GetUncoveredCell(const TableBuild& build, int column)
        {
            const ColumnLayout& layout = build.Columns[column];
            Rect cell(build.Row.X + layout.X, build.Row.Y, layout.Width, build.Row.Height);
            if (build.MovedColumn < 0 || build.MovedColumn == column)
                return cell;
            const ColumnLayout& moved = build.Columns[build.MovedColumn];
            const float coverStart = build.Row.X + moved.X;
            const float coverEnd = coverStart + moved.Width;
            const float before = std::clamp(coverStart, cell.X, cell.GetRight()) - cell.X;
            const float after = cell.GetRight() - std::clamp(coverEnd, cell.X, cell.GetRight());
            if (coverEnd <= cell.X || coverStart >= cell.GetRight())
                return cell;
            if (before >= after)
                return Rect(cell.X, cell.Y, before, cell.Height);
            return Rect(cell.GetRight() - after, cell.Y, after, cell.Height);
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
        build.RowHeight = GetAdaptiveRowHeight(options.RowHeight);
        build.ShowsAlternatingRows = options.ShowsAlternatingRows;
        build.MeasuredColumn = -1;
        build.PressedColumn = -1;
        build.MovedColumn = -1;
        build.StateID = HashID("##table", GetID(id));
        build.HeaderID = HashID("##header", build.StateID);
        build.IsSorting = options.Sort != nullptr;
        if (build.IsSorting)
            build.Sort = *options.Sort;
        build.HasHeaderStop = options.ShowsHeader && build.IsSorting;
        TableState& state = *GetState<TableState>(build.StateID, StateLifetime::Persistent);
        build.State = &state;

        const size_t count = columns.size();
        Arrangement arrangement;
        arrangement.Columns = columns;
        arrangement.Options = &options;
        arrangement.Layouts = Internal::GrowStorage(s_Columns, count);
        arrangement.Widths = Internal::GrowStorage(s_Widths, count);
        arrangement.Order = Internal::GrowStorage(s_Order, count);
        arrangement.Offsets = Internal::GrowStorage(s_Offsets, count);
        LoadArrangement(build, arrangement);
        build.Columns = arrangement.Layouts.data();
        build.ColumnCount = static_cast<int>(count);

        // A column fitted to its content during the last frame.
        if (state.FitColumn > 0 && static_cast<size_t>(state.FitColumn) <= count)
        {
            const size_t column = static_cast<size_t>(state.FitColumn - 1);
            const float width = std::max(state.FitWidth, std::max(columns[column].MinWidth, 0.0f));
            changes.WidthsChanged = StoreWidth(build, arrangement, column, width);
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
        Layout(build, arrangement);
        state.ScrollX = std::clamp(state.ScrollX, 0.0f, build.MaxScrollX);
        build.ScrollX =
            GetContentScale().Snap(Animate(HashID("##scrollx", build.StateID), state.ScrollX, ScrollSpring));

        // Columns that were rearranged slide into their places.
        std::fill(arrangement.Offsets.begin(), arrangement.Offsets.end(), 0.0f);
        if (state.IsSliding)
        {
            for (size_t i = 0; i < count; i++)
                arrangement.Offsets[i] = Animate(GetSlideID(build.StateID, static_cast<int>(i)), 0.0f, SlideSpring);
            state.SlideTime += GetDeltaTime();
            state.IsSliding = state.MovedColumn != 0 || state.SlideTime < SlideDuration;
        }

        Rect header;
        if (options.ShowsHeader)
        {
            ItemOptions item;
            item.Width = Size::Fill();
            header = AllocateItem(Vec2(0.0f, Internal::ColumnHeaderHeight), item);
            const TableChanges clicked = UpdateHeader(build, arrangement, header);
            changes.SortChanged = clicked.SortChanged;
            changes.OrderChanged = clicked.OrderChanged;
            if (clicked.WidthsChanged)
            {
                changes.WidthsChanged = true;
                Layout(build, arrangement);
            }
            state.ScrollX = std::clamp(state.ScrollX, 0.0f, build.MaxScrollX);
        }
        build.ScrollX = std::min(build.ScrollX, build.MaxScrollX);
        for (size_t i = 0; i < count; i++)
            arrangement.Layouts[i].X += arrangement.Offsets[i] - build.ScrollX;
        if (options.ShowsHeader)
            DrawHeader(build, columns, header, arrangement.Order);

        Internal::SelectionListDescription description;
        description.Scroll.Spacing = 0.0f;
        description.Scroll.Padding = EdgeInsets(RowInset, RowInsetY);
        Internal::BeginSelectionList("##rows", description);
        const Rect content = Internal::GetSelectionListContentRect();
        build.Viewport = Rect(content.X - RowInset, content.Y - RowInsetY, content.Width + RowInset * 2.0f,
                              content.Height + RowInsetY * 2.0f);

        // The column being moved floats above the others down the rows too.
        if (state.MovedColumn > 0 && static_cast<size_t>(state.MovedColumn) <= count)
        {
            build.MovedColumn = state.MovedColumn - 1;
            const ColumnLayout& moved = arrangement.Layouts[static_cast<size_t>(build.MovedColumn)];
            const Rect band(build.Viewport.X + RowInset + moved.X, build.Viewport.Y, moved.Width,
                            build.Viewport.Height);
            DrawList& drawList = GetDrawList();
            drawList.AddRect(band, GetStyleColor(StyleColor::ControlBackground));
            drawList.AddRect(band, GetStyleColor(StyleColor::ControlFill));
        }
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
        if (build.MovedColumn < 0)
        {
            Internal::DrawCellText(cell, text, options, build.Columns[column].Alignment, build.IsRowEmphasized);
            return;
        }
        // A column is being moved: what it covers of this cell is not drawn.
        DrawList& drawList = GetDrawList();
        drawList.PushClipRect(GetUncoveredCell(build, column));
        Internal::DrawCellText(cell, text, options, build.Columns[column].Alignment, build.IsRowEmphasized);
        drawList.PopClipRect();
    }

    void BeginTableCell()
    {
        TableBuild& build = GetBuild();
        const Rect cell = TakeCell(build);
        const int column = build.NextColumn;
        build.NextColumn = std::min(build.NextColumn + 1, build.ColumnCount);
        // A column is being moved: what it covers of this cell is not drawn.
        build.IsCellClipped = build.MovedColumn >= 0 && column < build.ColumnCount;
        if (build.IsCellClipped)
            GetDrawList().PushClipRect(GetUncoveredCell(build, column));
        // The cell is placed by hand; the row has already taken its space in the list.
        SetCursorPos(cell.GetMin());
        BeginHStack({.Width = Size::Fixed(cell.Width), .Height = Size::Fixed(cell.Height)});
    }

    void EndTableCell()
    {
        EndHStack();
        TableBuild& build = GetBuild();
        if (build.IsCellClipped)
            GetDrawList().PopClipRect();
        build.IsCellClipped = false;
    }
} // namespace Carbon
