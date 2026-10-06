#include "Carbon/Extensions/Table.h"

#include <algorithm>

#include "Carbon/Extensions/Internal/BuildState.h"
#include "Carbon/Extensions/Internal/ColumnLayout.h"
#include "Carbon/Extensions/Internal/SelectionList.h"

namespace Carbon
{
    namespace
    {
        // Space around the rows, so that the selection highlight is inset from the table's edge.
        constexpr float RowInset = 5.0f;
        constexpr float TableCornerRadius = 7.0f;

        using Internal::CellPadding;
        using Internal::ColumnLayout;
        using Internal::MaxColumns;

        // The table whose rows are being added.
        struct TableBuild
        {
            ColumnLayout Columns[MaxColumns];
            int ColumnCount;
            int NextColumn;
            int RowIndex;
            Rect Row;
            float RowHeight;
            bool IsRowEmphasized;
            /// The current row is inside the visible area: its text cells are drawn.
            bool IsRowVisible;
            bool ShowsAlternatingRows;
            /// A row is open: its ID is on the ID stack, so that the widgets in its cells are its own.
            bool HasRow;
        };

        Internal::BuildState<TableBuild> s_Build("Carbon.Table.Build");

        TableBuild& GetBuild()
        {
            return s_Build.Get();
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

    void BeginTable(std::string_view id, std::span<const TableColumn> columns, const TableOptions& options)
    {
        CB_VERIFY(columns.size() <= static_cast<size_t>(MaxColumns), "A table has at most {} columns", MaxColumns);
        TableBuild& build = GetBuild();
        build = TableBuild();
        build.RowHeight = options.RowHeight;
        build.ShowsAlternatingRows = options.ShowsAlternatingRows;

        // The frame around header and rows is drawn first and sized when the table ends.
        BeginVStack({.Spacing = 0.0f,
                     .Width = options.Width,
                     .Height = options.Height,
                     .Background = GetStyleColor(StyleColor::ControlBackground),
                     .CornerRadius = TableCornerRadius,
                     .ID = id});
        PushID(id);

        if (options.ShowsHeader)
        {
            ItemOptions item;
            item.Width = Size::Fill();
            const Rect header = AllocateItem(Vec2(0.0f, Internal::ColumnHeaderHeight), item);
            build.ColumnCount = Internal::LayoutColumns(columns, header.Width - RowInset * 2.0f, build.Columns);
            Internal::DrawColumnHeader(header, RowInset, columns, build.Columns, build.ColumnCount);
        }

        Internal::SelectionListDescription description;
        description.Scroll.Spacing = 0.0f;
        description.Scroll.Padding = EdgeInsets(RowInset, 3.0f);
        Internal::BeginSelectionList("##rows", description);
        if (!options.ShowsHeader)
        {
            build.ColumnCount =
                Internal::LayoutColumns(columns, Internal::GetSelectionListContentRect().Width, build.Columns);
        }
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
        if (GetBuild().HasRow)
            PopID();
        GetBuild().HasRow = false;
        Internal::EndSelectionList();
        PopID();
        EndVStack();
        // The outline is drawn last, over the rows' edges.
        GetDrawList().AddSquircleStroke(GetLastItemRect(), GetStyleColor(StyleColor::ControlBorder), TableCornerRadius,
                                        GetContentScale().GetPixelSize(), GetStyleVar(StyleVar::CornerSmoothing));
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
        const TextAlignment alignment = build.Columns[build.NextColumn].Alignment;
        build.NextColumn++;
        if (build.IsRowVisible)
            Internal::DrawCellText(cell, text, options, alignment, build.IsRowEmphasized);
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
