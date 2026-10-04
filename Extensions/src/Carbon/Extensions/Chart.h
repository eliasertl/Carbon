#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// One set of values in a chart.
    struct ChartSeries
    {
        /// Shown in the legend and next to the value under the pointer.
        std::string_view Label = {};
        std::span<const float> Values = {};
        /// Defaults to a color from the theme's palette, by the position of the series.
        std::optional<Carbon::Color> Color = {};
    };

    /// Per-call options of LineChart and BarChart. All fields are optional.
    struct ChartOptions
    {
        Size Width = Size::Fill();
        float Height = 180.0f;
        /// Labels along the horizontal axis, one per value. When they do not all fit, some are left out.
        std::span<const std::string_view> Labels = {};
        /// The range of the vertical axis. By default it fits the values, rounded outwards to round numbers; a
        /// bar chart always includes zero.
        std::optional<float> Min = {};
        std::optional<float> Max = {};
        bool ShowsGrid = true;
        /// Shows the series' labels above the chart, when they have any.
        bool ShowsLegend = true;
        /// Marks every value of a line chart with a dot.
        bool ShowsPoints = false;
    };

    /// A chart of one or more series as lines: for change over time. The value under the pointer is called out.
    /// The chart draws itself in when it appears.
    ///
    ///     const float sales[] = { 3.0f, 5.0f, 4.0f, 8.0f };
    ///     const Carbon::ChartSeries series[] = { { .Label = "Sales", .Values = sales } };
    ///     Carbon::LineChart("sales", series);
    void LineChart(std::string_view id, std::span<const ChartSeries> series, const ChartOptions& options = {});
    /// The same for a single unnamed series.
    void LineChart(std::string_view id, std::span<const float> values, const ChartOptions& options = {});

    /// A chart of one or more series as bars, grouped by position: for comparing categories.
    void BarChart(std::string_view id, std::span<const ChartSeries> series, const ChartOptions& options = {});
    void BarChart(std::string_view id, std::span<const float> values, const ChartOptions& options = {});
} // namespace Carbon
