#pragma once

#include <optional>
#include <source_location>
#include <span>
#include <string_view>

#include "Carbon/Core/Color.h"
#include "Carbon/Core/EdgeInsets.h"
#include "Carbon/Layout/Size.h"

namespace Carbon
{
    /// Options of a grid. All fields are optional.
    struct GridOptions
    {
        /// Distance between columns; the theme's Spacing when not set.
        std::optional<float> HorizontalSpacing = {};
        /// Distance between rows; the theme's Spacing when not set.
        std::optional<float> VerticalSpacing = {};
        /// Where cells sit horizontally inside their column.
        Carbon::Alignment Alignment = Carbon::Alignment::Leading;
        /// Where cells sit vertically inside their row.
        Carbon::VerticalAlignment VerticalAlignment = Carbon::VerticalAlignment::Center;
        /// Alignment per column, from the first column on; overrides `Alignment` for the columns it covers.
        std::span<const Carbon::Alignment> ColumnAlignments = {};
        /// Space between the grid's edges and its rows.
        EdgeInsets Padding = {};
        Size Width = Size::Fit();
        Size Height = Size::Fit();
        /// Draws a squircle behind the grid, e.g. for a grouped box.
        std::optional<Color> Background = {};
        /// Corner radius of the background; the theme's GroupCornerRadius when not set.
        std::optional<float> CornerRadius = {};
        /// A stable identity, as for stacks. By default a grid is identified by its call site.
        std::string_view ID = {};
    };

    /// Options of one grid row.
    struct GridRowOptions
    {
        /// Where the cells of this row sit vertically; the grid's VerticalAlignment when not set.
        std::optional<Carbon::VerticalAlignment> Alignment = {};
        /// A stable identity, as for stacks. By default a row is identified by its call site.
        std::string_view ID = {};
    };

    /// Options of the next cell of a grid row.
    struct GridCellOptions
    {
        /// How many columns the cell covers.
        int ColumnSpan = 1;
        /// Where the cell's content sits horizontally; the column's alignment when not set.
        std::optional<Carbon::Alignment> Alignment = {};
        /// Where the cell's content sits vertically; the row's alignment when not set.
        std::optional<Carbon::VerticalAlignment> VerticalAlignment = {};
    };

    /// Starts a grid: rows of cells whose columns line up across the rows. Put rows inside it with BeginGridRow;
    /// anything placed directly in the grid spans its full width, like a divider. Every Begin needs a matching End.
    /// Leave `location` alone: it identifies the grid by its call site.
    void BeginGrid(const GridOptions& options = {},
                   const std::source_location& location = std::source_location::current());
    void EndGrid();

    /// Starts a row of the current grid. Each item placed in the row, a widget or a stack, is one cell, and each
    /// cell goes into the next column. A column is as wide as its widest cell.
    void BeginGridRow(const GridRowOptions& options = {},
                      const std::source_location& location = std::source_location::current());
    void EndGridRow();

    /// Sets the span and alignment of the next cell of the current grid row. Call it directly before the cell.
    void SetNextGridCell(const GridCellOptions& options);
} // namespace Carbon
