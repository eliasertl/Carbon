#include "Carbon/Widgets/ControlSize.h"

#include <cmath>

#include "Carbon/Style/Style.h"

namespace Carbon
{
    ControlMetrics GetControlMetrics(ControlSize size)
    {
        const float height = GetStyleVar(StyleVar::ControlHeight);
        const float padding = GetStyleVar(StyleVar::ControlPadding);
        const float radius = GetStyleVar(StyleVar::CornerRadius);

        ControlMetrics metrics;
        switch (size)
        {
            case ControlSize::Small:
                // 20 points with 11-point text at the default theme.
                metrics.Height = std::round(height * 0.84f);
                metrics.Padding = std::round(padding * 0.8f);
                metrics.CornerRadius = radius * 0.84f;
                metrics.Style = TextStyle::Subheadline;
                break;
            case ControlSize::Regular:
                metrics.Height = height;
                metrics.Padding = padding;
                metrics.CornerRadius = radius;
                metrics.Style = TextStyle::Body;
                break;
            case ControlSize::Large:
                // 30 points; the text stays at body size, as on macOS.
                metrics.Height = std::round(height * 1.25f);
                metrics.Padding = std::round(padding * 1.4f);
                metrics.CornerRadius = radius * 1.34f;
                metrics.Style = TextStyle::Body;
                break;
        }
        return metrics;
    }
} // namespace Carbon
