#include "Carbon/Extensions/Table.h"

#include <algorithm>

#include "Carbon/Extensions/Internal/SelectionList.h"

namespace Carbon
{
    namespace
    {
        constexpr int MaxColumns = 16;
        constexpr float HeaderHeight = 24.0f;
        // Space around the rows, so that the selection highlight is inset from the table's edge.
        constexpr float RowInset = 5.0f;
        constexpr float CellPadding = 8.0f;
        constexpr float IconSize = 15.0f;
        constexpr float IconGap = 6.0f;
        constexpr float TableCornerRadius = 7.0f;

        struct ColumnLayout
        {
            /// Relative to the leading edge of the rows.
            float X;
            float Width;
            TextAlignment Alignment;
        };

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
            bool ShowsAlternatingRows;
            /// A row is open: its ID is on the ID stack, so that the widgets in its cells are its own.
            bool HasRow;
        };

        TableBuild& GetBuild()
        {
            return *GetState<TableBuild>(HashID("Carbon.Table.Build"), StateLifetime::Persistent);
        }

        // Fixed columns get their width; the others share the rest by weight.
        void LayoutColumns(TableBuild& build, std::span<const TableColumn> columns, float width)
        {
            build.ColumnCount = std::min(static_cast<int>(columns.size()), MaxColumns);
            float fixed = 0.0f;
            float weight = 0.0f;
            for (int i = 0; i < build.ColumnCount; i++)
            {
                const Size size = columns[static_cast<size_t>(i)].Width;
                if (size.Mode == SizeMode::Fixed)
                    fixed += std::max(size.Value, 0.0f);
                else
                    weight += size.Mode == SizeMode::Fill ? std::max(size.Value, 0.0f) : 1.0f;
            }
            const float unit = weight > 0.0f ? std::max(width - fixed, 0.0f) / weight : 0.0f;

            float x = 0.0f;
            for (int i = 0; i < build.ColumnCount; i++)
            {
                const TableColumn& column = columns[static_cast<size_t>(i)];
                ColumnLayout& layout = build.Columns[i];
                layout.X = x;
                if (column.Width.Mode == SizeMode::Fixed)
                    layout.Width = std::max(column.Width.Value, 0.0f);
                else
                    layout.Width =
                        unit * (column.Width.Mode == SizeMode::Fill ? std::max(column.Width.Value, 0.0f) : 1.0f);
                layout.Alignment = column.Alignment;
                x += layout.Width;
            }
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

        DrawList& drawList = GetDrawList();
        const float pixel = GetContentScale().GetPixelSize();

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
            const Rect header = AllocateItem(Vec2(0.0f, HeaderHeight), item);
            LayoutColumns(build, columns, header.Width - RowInset * 2.0f);

            TextSpec spec = GetTextSpec(TextStyle::Subheadline, true);
            spec.Wraps = false;
            const Color titleColor = GetStyleColor(StyleColor::SecondaryLabel);
            const Color separator = GetStyleColor(StyleColor::Separator);
            for (int i = 0; i < build.ColumnCount; i++)
            {
                const ColumnLayout& column = build.Columns[i];
                const float x = header.X + RowInset + column.X;
                spec.MaxWidth = std::max(column.Width - CellPadding * 2.0f, 1.0f);
                spec.Alignment = column.Alignment;
                DrawLabel(drawList, header, x + CellPadding, columns[static_cast<size_t>(i)].Title, spec, titleColor);
                if (i > 0)
                    drawList.AddRect(Rect(x, header.Y + 5.0f, pixel, header.Height - 10.0f), separator);
            }
            drawList.AddRect(Rect(header.X, header.GetBottom() - pixel, header.Width, pixel), separator);
        }

        Internal::SelectionListDescription description;
        description.Scroll.Spacing = 0.0f;
        description.Scroll.Padding = EdgeInsets(RowInset, 3.0f);
        Internal::BeginSelectionList("##rows", description);
        if (!options.ShowsHeader)
            LayoutColumns(build, columns, Internal::GetSelectionListContentRect().Width);
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

        // The selected row has its highlight; every other one of the others is tinted.
        if (build.ShowsAlternatingRows && !isSelected && build.RowIndex % 2 == 1)
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
        Rect cell = TakeCell(build);
        if (build.NextColumn >= build.ColumnCount)
            return;
        const TextAlignment alignment = build.Columns[build.NextColumn].Alignment;
        build.NextColumn++;
        if (cell.Width <= 0.0f)
            return;

        DrawList& drawList = GetDrawList();
        const Color onAccent = GetStyleColor(StyleColor::OnAccent);
        if (!options.Icon.empty())
        {
            DrawIcon(drawList, Vec2(cell.X + IconSize * 0.5f, cell.GetCenter().Y), options.Icon, IconSize,
                     build.IsRowEmphasized ? onAccent : GetStyleColor(StyleColor::Accent));
            cell.X += IconSize + IconGap;
            cell.Width = std::max(cell.Width - IconSize - IconGap, 1.0f);
        }

        TextSpec spec = GetTextSpec(TextStyle::Body);
        spec.MaxWidth = cell.Width;
        spec.Wraps = false;
        spec.Alignment = alignment;
        const StyleColor role = options.Secondary ? StyleColor::SecondaryLabel : StyleColor::Label;
        DrawLabel(drawList, cell, cell.X, text, spec, build.IsRowEmphasized ? onAccent : GetStyleColor(role));
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
