#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "Carbon/Extension.h"
#include "Carbon/Extensions/Table.h"

namespace Carbon::Internal
{
    /// What Table and OutlineView have in common: columns sharing a width, a header with their titles, and text
    /// cells.
    inline constexpr float ColumnHeaderHeight = 24.0f;
    inline constexpr float CellPadding = 8.0f;

    /// Where one column sits, relative to the leading edge of the rows.
    struct ColumnLayout
    {
        float X;
        float Width;
        TextAlignment Alignment;
    };

    /// Fixed columns get their width; the others share the rest by weight, but get at least their MinWidth.
    /// `widths` overrides the width of a column with a value above zero, which makes it a fixed one (the width
    /// the user gave it); it may be empty. `order` lists the columns in the order they are shown and may be
    /// empty for the declared order. Writes one entry per column to `layouts`, indexed like `columns`, and
    /// returns the width of all columns together.
    /// `fillMinimum` is the least width of a column that shares the rest, beyond its MinWidth: in compact width a
    /// table scrolls sideways rather than squeezing its columns.
    float LayoutColumns(std::span<const TableColumn> columns, float width, std::span<ColumnLayout> layouts,
                        std::span<const float> widths = {}, std::span<const int> order = {}, float fillMinimum = 0.0f);

    /// Grows `storage` to hold at least `count` entries and returns the first `count` of them. Storage only ever
    /// grows, so a frame allocates only when a component has more columns than any before it.
    template <typename T>
    std::span<T> GrowStorage(std::vector<T>& storage, size_t count)
    {
        if (storage.size() < count)
            storage.resize(count);
        return std::span<T>(storage.data(), count);
    }

    /// Draws the column titles into `header`, with separators between them and a hairline below. `inset` is the
    /// distance from the header's leading edge to the rows' leading edge.
    void DrawColumnHeader(const Rect& header, float inset, std::span<const TableColumn> columns,
                          std::span<const ColumnLayout> layouts);

    /// Draws the text of a cell (`cell` is the area inside its padding), optionally after an icon. Emphasized
    /// cells belong to the selected row of a focused list and are drawn in the on-accent color.
    void DrawCellText(Rect cell, std::string_view text, const TableCellOptions& options, TextAlignment alignment,
                      bool isEmphasized);
    /// The width DrawCellText needs for the text and icon of a cell, without the cell's padding.
    float MeasureCellText(std::string_view text, const TableCellOptions& options);
} // namespace Carbon::Internal
