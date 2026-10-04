#pragma once

#include <string_view>

#include "Carbon/Core/Color.h"
#include "Carbon/Core/ID.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Text/TextSpec.h"

namespace Carbon
{
    class DrawList;

    /// Animated 0..1 amounts of a control's hover and pressed states. Carbon's controls all react to the pointer
    /// through these helpers, so a custom component that uses them feels the same.
    struct ControlFeedback
    {
        float Hover = 0.0f;
        float Press = 0.0f;
    };

    /// Animates the hover and pressed feedback of a control from its interaction this frame.
    ControlFeedback AnimateFeedback(ID id, bool isHovered, bool isPressed);

    /// Applies hover and pressed tints to a fill color by shifting it towards `over` (usually the label color),
    /// by the theme's HoverAmount and PressedAmount.
    Color ApplyFeedback(const Color& fill, const Color& over, const ControlFeedback& feedback);

    /// Draws one line of text centered vertically in `rect`, starting at `x`.
    void DrawLabel(DrawList& drawList, const Rect& rect, float x, std::string_view text, const TextSpec& spec,
                   Color color);
} // namespace Carbon
