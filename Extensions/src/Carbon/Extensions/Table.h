#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// One column of a table.
    struct TableColumn
    {
        std::string_view Title = {};
        /// Fixed is a width in points; Fill shares what the fixed columns leave over, by weight.
        Size Width = Size::Fill();
        /// How the column's text cells and its title are aligned.
        TextAlignment Alignment = TextAlignment::Leading;
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
    };

    /// Per-call options of TableCell. All fields are optional.
    struct TableCellOptions
    {
        /// Draws the text in the secondary label color.
        bool Secondary = false;
        /// An icon before the text.
        std::string_view Icon = {};
    };

    /// Rows of data in columns, with a header and a selection. At most 16 columns.
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
    /// The table is one stop for Tab; with focus, the up and down arrow keys, Home and End move the selection.
    void BeginTable(std::string_view id, std::span<const TableColumn> columns, const TableOptions& options = {});
    void EndTable();

    /// Starts the next row; its cells follow. Pass whether it is selected. Returns true when the user picks it,
    /// by click or keyboard. `id` must be unique within the table; it is pushed on the ID stack until the next
    /// row, so widgets in the cells of different rows may share their labels.
    bool TableRow(int64_t id, bool isSelected = false);
    bool TableRow(std::string_view id, bool isSelected = false);

    /// The next cell of the current row, as text. Text that does not fit is cut off with an ellipsis.
    void TableCell(std::string_view text, const TableCellOptions& options = {});

    /// The next cell of the current row with content of your own, laid out like in an HStack:
    ///     Carbon::BeginTableCell();
    ///     Carbon::Toggle("##done", &task.Done, { .Kind = Carbon::ToggleKind::Checkbox });
    ///     Carbon::EndTableCell();
    void BeginTableCell();
    void EndTableCell();
} // namespace Carbon
