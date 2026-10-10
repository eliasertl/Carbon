#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "Carbon/Extension.h"
#include "Carbon/Extensions/RowRange.h"

namespace Carbon
{
    /// The direction in which rows are sorted.
    enum class SortDirection : uint8_t
    {
        /// Smallest first: A to Z, oldest first.
        Ascending,
        Descending
    };

    /// The column the rows of a table are sorted by. The application owns it and sorts its rows; the table shows
    /// it in the header and changes it when the user clicks a column header.
    struct TableSort
    {
        /// An index into the table's columns, as declared (not as shown), or -1 when the rows are not sorted.
        int Column = -1;
        SortDirection Direction = SortDirection::Ascending;

        constexpr bool operator==(const TableSort& other) const = default;
    };

    /// One column of a table.
    struct TableColumn
    {
        std::string_view Title = {};
        /// Fixed is a width in points; Fill shares what the fixed columns leave over, by weight.
        Size Width = Size::Fill();
        /// How the column's text cells and its title are aligned.
        TextAlignment Alignment = TextAlignment::Leading;
        /// The narrowest the column gets, whether the user drags its divider or Fill columns share little space.
        float MinWidth = 40.0f;
        /// The user can drag the divider at the column's trailing edge to resize it, and double-click it to fit
        /// the column to its content.
        bool IsResizable = true;
        /// Clicking the header sorts the rows by this column, in a table that has a TableOptions::Sort.
        bool IsSortable = true;
        /// The direction of the first sort by this column; another click flips it. Descending suits dates and
        /// sizes, where the newest or largest usually matter most.
        SortDirection InitialSortDirection = SortDirection::Ascending;
    };

    /// Per-call options of BeginTable. All fields are optional.
    struct TableOptions
    {
        Size Width = Size::Fill();
        /// A table scrolls, so inside a stack that fits its content it needs a fixed height.
        Size Height = Size::Fill();
        float RowHeight = 24.0f;
        bool ShowsHeader = true;
        /// Tints every other row, which helps the eye follow a row across many columns.
        bool ShowsAlternatingRows = true;
        /// The column the rows are sorted by. Pass the application's sort to make the headers sortable: the table
        /// shows the sort indicator and updates it when the user clicks a header. Sort the rows by it before you
        /// add them. Null for a table whose headers do not sort.
        TableSort* Sort = nullptr;
        /// The width of each column in points, by column index, for an application that keeps the user's widths
        /// (to save them, or to set them): one entry per column, 0 for a column that keeps its declared Width.
        /// The table writes the width the user drags or fits. Leave it empty to let Carbon remember the widths.
        std::span<float> ColumnWidths = {};
        /// The order in which the columns are shown, as column indices, for an application that keeps the user's
        /// arrangement: one entry per column. An entry that is not a permutation of the indices is reset to
        /// 0, 1, 2, ... Leave it empty to let Carbon remember the order.
        std::span<int> ColumnOrder = {};
    };

    /// What the user changed in the header of a table this frame. The table has already written the change to
    /// the storage of TableOptions; this tells an application that saves it when to do so.
    struct TableChanges
    {
        /// The user chose another column to sort by, or flipped the direction: sort the rows again.
        bool SortChanged = false;
        /// A column was resized: dragged or fitted to its content.
        bool WidthsChanged = false;
        /// The columns were rearranged.
        bool OrderChanged = false;
    };

    /// Per-call options of TableCell. All fields are optional.
    struct TableCellOptions
    {
        /// Draws the text in the secondary label color.
        bool Secondary = false;
        /// An icon before the text.
        std::string_view Icon = {};
    };

    /// Rows of data in columns, with a header and a selection. Any number of columns; when they are wider than
    /// the table, it scrolls sideways.
    ///
    ///     const Carbon::TableColumn columns[] = { { .Title = "Name" }, { .Title = "Size", .Width = 80.0f } };
    ///     Carbon::BeginTable("files", columns, { .Height = 240.0f });
    ///     for (int i = 0; i < count; i++)
    ///     {
    ///         if (Carbon::TableRow(i, i == selected))
    ///             selected = i;
    ///         Carbon::TableCell(files[i].Name);
    ///         Carbon::TableCell(files[i].Size, { .Secondary = true });
    ///     }
    ///     Carbon::EndTable();
    ///
    /// Cells are submitted in the order of `columns`, however the user has arranged the columns on screen.
    ///
    /// The user resizes a column by dragging the divider at its trailing edge and fits it to its content with a
    /// double-click on the divider. A column whose width the user has set keeps that width; the Fill columns
    /// share what is left.
    ///
    /// Sorting is the application's: Carbon reports which column to sort by and in which direction, and the
    /// application submits its rows in that order.
    ///
    ///     Carbon::TableSort sort = { .Column = 0 };     // kept by the application
    ///     if (Carbon::BeginTable("files", columns, { .Sort = &sort }).SortChanged)
    ///         SortFiles(files, sort);
    ///
    /// The table is one stop for Tab; with focus, the up and down arrow keys, Home and End move the selection. A
    /// table that sorts has a second stop before it, its header: the left and right arrow keys move between the
    /// columns, and Space or Enter sorts by the column.
    TableChanges BeginTable(std::string_view id, std::span<const TableColumn> columns,
                            const TableOptions& options = {});
    void EndTable();

    /// Starts the next row; its cells follow. Pass whether it is selected. Returns true when the user picks it,
    /// by click or keyboard. `id` must be unique within the table; it is pushed on the ID stack until the next
    /// row, so widgets in the cells of different rows may share their labels.
    bool TableRow(int64_t id, bool isSelected = false);
    bool TableRow(std::string_view id, bool isSelected = false);

    /// For a table with many rows: declares that the table has `count` rows and returns the ones to submit in
    /// this frame, which are the visible rows and, after the arrow keys moved the selection, the row they moved
    /// to. The table reserves the space of all the others, so it scrolls as if every row were there, and a frame
    /// costs the same whether the table has a thousand rows or a million:
    ///
    ///     Carbon::BeginTable("files", columns, { .Height = 240.0f });
    ///     const Carbon::RowRange rows = Carbon::ClipTableRows(count, selected);
    ///     for (int i = rows.First; i < rows.End; i++)
    ///     {
    ///         if (Carbon::TableRow(i, i == selected))
    ///             selected = i;
    ///         Carbon::TableCell(files[i].Name);
    ///     }
    ///     Carbon::EndTable();
    ///
    /// Call it once, right after BeginTable, and submit exactly the rows of the range, in order. `selectedRow` is
    /// the index of the selected row, or -1: the keyboard moves the selection from there even while that row is
    /// not among the submitted ones.
    RowRange ClipTableRows(int count, int selectedRow = -1);
    /// The next cell of the current row, as text. Text that does not fit is cut off with an ellipsis.
    void TableCell(std::string_view text, const TableCellOptions& options = {});

    /// The next cell of the current row with content of your own, laid out like in an HStack:
    ///     Carbon::BeginTableCell();
    ///     Carbon::Toggle("##done", &task.Done, { .Kind = Carbon::ToggleKind::Checkbox });
    ///     Carbon::EndTableCell();
    void BeginTableCell();
    void EndTableCell();
} // namespace Carbon
