#include "Carbon/Widgets/Separator.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Layout/Layout.h"
#include "Carbon/Style/Style.h"

namespace Carbon
{
    void Separator(const SeparatorOptions& options)
    {
        Context& context = Internal::GetFrameContext();

        // A whole number of pixels, at least one, so the line is never blurred.
        const float requested = Resolve(options.Thickness, StyleVar::BorderWidth);
        const float pixels = std::max(1.0f, std::round(context.Scale.ToPixels(requested)));
        const float thickness = context.Scale.ToPoints(pixels);

        ItemOptions item;
        Vec2 size;
        if (GetLayoutAxis() == Axis::Vertical)
        {
            item.Width = Size::Fill();
            size = Vec2(0.0f, thickness);
        }
        else
        {
            item.Height = Size::Fill();
            size = Vec2(thickness, 0.0f);
        }
        const Rect rect = AllocateItem(size, item);
        context.Draw.AddRect(context.Scale.Snap(rect), Resolve(options.Color, StyleColor::Separator));
    }
} // namespace Carbon
