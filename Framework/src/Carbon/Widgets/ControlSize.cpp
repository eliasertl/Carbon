#include "Carbon/Widgets/ControlSize.h"

#include <cmath>

#include "Carbon/Animation/Animation.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Draw/DrawList.h"
#include "Carbon/Style/Style.h"
#include "Carbon/Widgets/WidgetInternal.h"

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

    namespace Internal
    {
        ControlFeedback AnimateFeedback(ID id, bool isHovered, bool isPressed)
        {
            // Quick, critically damped, and a change of appearance: it stays a fade under reduced motion.
            static constexpr AnimationSpec HoverSpec = AnimationSpec::Spring(0.2f).AsAppearance();
            static constexpr AnimationSpec PressSpec = AnimationSpec::Spring(0.12f).AsAppearance();
            ControlFeedback feedback;
            feedback.Hover = Animate(HashID("##hover", id), isHovered ? 1.0f : 0.0f, HoverSpec);
            feedback.Press = Animate(HashID("##press", id), isPressed ? 1.0f : 0.0f, PressSpec);
            return feedback;
        }

        Color ApplyFeedback(const Color& fill, const Color& over, const ControlFeedback& feedback)
        {
            const float hover = GetStyleVar(StyleVar::HoverAmount) * feedback.Hover;
            const float press = GetStyleVar(StyleVar::PressedAmount);
            // Pressing replaces the hover tint rather than adding to it.
            return Blend(fill, over, hover + (press - hover) * feedback.Press);
        }

        void DrawLabel(DrawList& drawList, const Rect& rect, float x, std::string_view text, const TextSpec& spec,
                       Color color)
        {
            const FontMetrics metrics = GetFontMetrics(spec);
            drawList.AddText(Vec2(x, rect.Y + (rect.Height - metrics.LineHeight) * 0.5f), text, spec, color);
        }
    } // namespace Internal
} // namespace Carbon
