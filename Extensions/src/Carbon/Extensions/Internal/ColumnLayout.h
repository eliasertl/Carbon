#pragma once

#include <span>
#include <string_view>

#include "Carbon/Extension.h"
#include "Carbon/Extensions/Table.h"

namespace Carbon::Internal
{
    /// What Table and OutlineView have in common: columns sharing a width, a header with their titles, and text
    /// cells.
    inline constexpr int MaxColumns = 16;
    inline constexpr float ColumnHeaderHeight = 24.0f;
    inline constexpr float CellPadding = 8.0f;

    /// Where one column sits, relative to the leading edge of the rows.
    struct ColumnLayout
    {
        float X;
        float Width;
        TextAlignment Alignment;
    };

    /// Fixed columns get their width; the others share the rest by weight. Writes at most MaxColumns entries to
    /// `layouts` and returns how many.
    int LayoutColumns(std::span<const TableColumn> columns, float width, ColumnLayout* layouts);

    /// Draws the column titles into `header`, with separators between them and a hairline below. `inset` is the
    /// distance from the header's leading edge to the rows' leading edge.
    void DrawColumnHeader(const Rect& header, float inset, std::span<const TableColumn> columns,
                          const ColumnLayout* layouts, int count);

    /// Draws the text of a cell (`cell` is the area inside its padding), optionally after an icon. Emphasized
    /// cells belong to the selected row of a focused list and are drawn in the on-accent color.
    void DrawCellText(Rect cell, std::string_view text, const TableCellOptions& options, TextAlignment alignment,
                      bool isEmphasized);
} // namespace Carbon::Internal
