#include "Carbon/Extensions/Internal/ColumnLayout.h"

#include <algorithm>

namespace Carbon::Internal
{
    namespace
    {
        constexpr float IconSize = 15.0f;
        constexpr float IconGap = 6.0f;
    } // namespace

    int LayoutColumns(std::span<const TableColumn> columns, float width, ColumnLayout* layouts)
    {
        const int count = std::min(static_cast<int>(columns.size()), MaxColumns);
        float fixed = 0.0f;
        float weight = 0.0f;
        for (int i = 0; i < count; i++)
        {
            const Size size = columns[static_cast<size_t>(i)].Width;
            if (size.Mode == SizeMode::Fixed)
                fixed += std::max(size.Value, 0.0f);
            else
                weight += size.Mode == SizeMode::Fill ? std::max(size.Value, 0.0f) : 1.0f;
        }
        const float unit = weight > 0.0f ? std::max(width - fixed, 0.0f) / weight : 0.0f;

        float x = 0.0f;
        for (int i = 0; i < count; i++)
        {
            const TableColumn& column = columns[static_cast<size_t>(i)];
            ColumnLayout& layout = layouts[i];
            layout.X = x;
            if (column.Width.Mode == SizeMode::Fixed)
                layout.Width = std::max(column.Width.Value, 0.0f);
            else
                layout.Width = unit * (column.Width.Mode == SizeMode::Fill ? std::max(column.Width.Value, 0.0f) : 1.0f);
            layout.Alignment = column.Alignment;
            x += layout.Width;
        }
        return count;
    }

    void DrawColumnHeader(const Rect& header, float inset, std::span<const TableColumn> columns,
                          const ColumnLayout* layouts, int count)
    {
        DrawList& drawList = GetDrawList();
        const float pixel = GetContentScale().GetPixelSize();
        TextSpec spec = GetTextSpec(TextStyle::Subheadline, true);
        spec.Wraps = false;
        const Color titleColor = GetStyleColor(StyleColor::SecondaryLabel);
        const Color separator = GetStyleColor(StyleColor::Separator);
        for (int i = 0; i < count; i++)
        {
            const ColumnLayout& column = layouts[i];
            const float x = header.X + inset + column.X;
            spec.MaxWidth = std::max(column.Width - CellPadding * 2.0f, 1.0f);
            spec.Alignment = column.Alignment;
            DrawLabel(drawList, header, x + CellPadding, columns[static_cast<size_t>(i)].Title, spec, titleColor);
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
} // namespace Carbon::Internal
