#include "Carbon/Extensions/Internal/ColumnLayout.h"

#include <algorithm>

namespace Carbon::Internal
{
    namespace
    {
        constexpr float IconSize = 15.0f;
        constexpr float IconGap = 6.0f;
    } // namespace

    float LayoutColumns(std::span<const TableColumn> columns, float width, std::span<ColumnLayout> layouts,
                        std::span<const float> widths, std::span<const int> order)
    {
        const size_t count = std::min(columns.size(), layouts.size());
        const bool hasWidths = widths.size() >= count;
        // A column the user resized, or a Fixed one, has its width; the others share what is left by weight.
        float fixed = 0.0f;
        float weight = 0.0f;
        for (size_t i = 0; i < count; i++)
        {
            const TableColumn& column = columns[i];
            const float minimum = std::max(column.MinWidth, 0.0f);
            if (hasWidths && widths[i] > 0.0f)
                fixed += std::max(widths[i], minimum);
            else if (column.Width.Mode == SizeMode::Fixed)
                fixed += std::max(column.Width.Value, 0.0f);
            else
                weight += column.Width.Mode == SizeMode::Fill ? std::max(column.Width.Value, 0.0f) : 1.0f;
        }
        const float unit = weight > 0.0f ? std::max(width - fixed, 0.0f) / weight : 0.0f;

        for (size_t i = 0; i < count; i++)
        {
            const TableColumn& column = columns[i];
            ColumnLayout& layout = layouts[i];
            const float minimum = std::max(column.MinWidth, 0.0f);
            if (hasWidths && widths[i] > 0.0f)
                layout.Width = std::max(widths[i], minimum);
            else if (column.Width.Mode == SizeMode::Fixed)
                layout.Width = std::max(column.Width.Value, 0.0f);
            else
                layout.Width = std::max(
                    unit * (column.Width.Mode == SizeMode::Fill ? std::max(column.Width.Value, 0.0f) : 1.0f), minimum);
            layout.Alignment = column.Alignment;
        }

        float x = 0.0f;
        const bool hasOrder = order.size() >= count;
        for (size_t position = 0; position < count; position++)
        {
            const size_t index = hasOrder ? static_cast<size_t>(order[position]) : position;
            if (index >= count)
                continue;
            layouts[index].X = x;
            x += layouts[index].Width;
        }
        return x;
    }

    void DrawColumnHeader(const Rect& header, float inset, std::span<const TableColumn> columns,
                          std::span<const ColumnLayout> layouts)
    {
        DrawList& drawList = GetDrawList();
        const float pixel = GetContentScale().GetPixelSize();
        TextSpec spec = GetTextSpec(TextStyle::Subheadline, true);
        spec.Wraps = false;
        const Color titleColor = GetStyleColor(StyleColor::SecondaryLabel);
        const Color separator = GetStyleColor(StyleColor::Separator);
        const size_t count = std::min(columns.size(), layouts.size());
        for (size_t i = 0; i < count; i++)
        {
            const ColumnLayout& column = layouts[i];
            const float x = header.X + inset + column.X;
            spec.MaxWidth = std::max(column.Width - CellPadding * 2.0f, 1.0f);
            spec.Alignment = column.Alignment;
            DrawLabel(drawList, header, x + CellPadding, columns[i].Title, spec, titleColor);
            if (i > 0)
                drawList.AddRect(Rect(x, header.Y + 5.0f, pixel, header.Height - 10.0f), separator);
        }
        drawList.AddRect(Rect(header.X, header.GetBottom() - pixel, header.Width, pixel), separator);
    }

    void DrawCellText(Rect cell, std::string_view text, const TableCellOptions& options, TextAlignment alignment,
                      bool isEmphasized)
    {
        if (cell.Width <= 0.0f)
            return;
        DrawList& drawList = GetDrawList();
        const Color onAccent = GetStyleColor(StyleColor::OnAccent);
        if (!options.Icon.empty())
        {
            DrawIcon(drawList, Vec2(cell.X + IconSize * 0.5f, cell.GetCenter().Y), options.Icon, IconSize,
                     isEmphasized ? onAccent : GetStyleColor(StyleColor::Accent));
            cell.X += IconSize + IconGap;
            cell.Width = std::max(cell.Width - IconSize - IconGap, 1.0f);
        }

        TextSpec spec = GetTextSpec(TextStyle::Body);
        spec.MaxWidth = cell.Width;
        spec.Wraps = false;
        spec.Alignment = alignment;
        const StyleColor role = options.Secondary ? StyleColor::SecondaryLabel : StyleColor::Label;
        DrawLabel(drawList, cell, cell.X, text, spec, isEmphasized ? onAccent : GetStyleColor(role));
    }

    float MeasureCellText(std::string_view text, const TableCellOptions& options)
    {
        TextSpec spec = GetTextSpec(TextStyle::Body);
        spec.Wraps = false;
        const float icon = options.Icon.empty() ? 0.0f : IconSize + IconGap;
        return icon + MeasureText(text, spec).X;
    }
} // namespace Carbon::Internal
