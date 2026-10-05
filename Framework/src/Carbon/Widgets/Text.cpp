#include "Carbon/Widgets/Text.h"

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Interaction/Interaction.h"
#include "Carbon/Layout/Layout.h"
#include "Carbon/Style/Style.h"

namespace Carbon
{
    namespace
    {
        // The embedded icon fonts are drawn at 1.2 times the text size (see TextSystem), so an icon that should
        // fill a square of N points is set at N / 1.2.
        constexpr float IconFontScale = 1.2f;

        // Text and icons are not interactive, but Tooltip and IsItemHovered should work after them.
        void SetStaticLastItem(const Rect& rect)
        {
            Interaction interaction;
            interaction.Hovered = IsRectHovered(rect);
            SetLastItem(ID(), rect, interaction);
        }
    } // namespace

    void Text(std::string_view text, const TextOptions& options)
    {
        Context& context = Internal::GetFrameContext();

        TextSpec spec = GetTextSpec(options.Style, options.Emphasized);
        if (options.Weight.has_value())
            spec.Weight = *options.Weight;
        spec.Italic = options.Italic;
        spec.Icons = options.Icons;
        if (options.Font != nullptr)
            spec.Font = options.Font;

        ItemOptions item;
        item.Width = options.Width;
        if (options.Width.Mode != SizeMode::Fit)
        {
            // The width is decided by the layout; the text wraps or truncates inside it.
            spec.MaxWidth = ResolveItemSize(Vec2(), item).X;
            spec.Wraps = options.Wraps;
            spec.Alignment = options.Alignment;
        }

        const Vec2 size = MeasureText(text, spec);
        const Rect rect =
            AllocateItem(Vec2(options.Width.Mode == SizeMode::Fit ? size.X : spec.MaxWidth, size.Y), item);

        const StyleColor role = options.Secondary ? StyleColor::SecondaryLabel : StyleColor::Label;
        context.Draw.AddText(rect.GetMin(), text, spec, Resolve(options.Color, role));
        SetStaticLastItem(rect);
    }

    void Icon(std::string_view icon, const IconOptions& options)
    {
        Context& context = Internal::GetFrameContext();

        TextSpec spec;
        spec.Font = context.Style.Font;
        spec.Size = options.Size / IconFontScale;
        spec.LineHeight = options.Size;
        spec.Icons = options.Variant;

        const Rect rect = AllocateItem(Vec2(options.Size, options.Size));
        context.Draw.AddText(rect.GetMin(), icon, spec, Resolve(options.Color, StyleColor::Label));
        SetStaticLastItem(rect);
    }

    void DrawIcon(DrawList& drawList, Vec2 center, std::string_view icon, float size, Color color, IconVariant variant)
    {
        TextSpec spec;
        spec.Font = Internal::GetContext().Style.Font;
        spec.Size = size / IconFontScale;
        spec.LineHeight = size;
        spec.Icons = variant;
        drawList.AddText(center - Vec2(size * 0.5f), icon, spec, color);
    }
} // namespace Carbon
