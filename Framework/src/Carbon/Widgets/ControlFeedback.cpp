#include "Carbon/Widgets/ControlFeedback.h"

#include "Carbon/Animation/Animation.h"
#include "Carbon/Draw/DrawList.h"
#include "Carbon/Style/Style.h"

namespace Carbon
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
} // namespace Carbon
