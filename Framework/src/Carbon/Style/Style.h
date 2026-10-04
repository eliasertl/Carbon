#pragma once

#include "Carbon/Core/Color.h"
#include "Carbon/Style/StyleColor.h"
#include "Carbon/Style/StyleVar.h"
#include "Carbon/Style/Theme.h"

namespace Carbon
{
    /// Duration of an animated theme switch, in seconds.
    inline constexpr float ThemeTransitionDuration = 0.35f;

    /// Sets the theme of the current context. Carbon cannot detect the OS appearance; the host decides.
    /// When `animated`, every color and metric glides from its current value to the new theme's over
    /// ThemeTransitionDuration (a short cross-fade when reduced motion is on). The very first theme, set before
    /// any frame, applies immediately.
    void SetTheme(const Theme& theme, bool animated = true);

    /// The theme as it is this frame. During an animated switch this is the blend of the old and the new theme.
    const Theme& GetTheme();

    /// The theme most recently passed to SetTheme: where a running transition is heading.
    const Theme& GetTargetTheme();

    /// The current value of a semantic color.
    Color GetStyleColor(StyleColor color);
    /// The current value of a metric.
    float GetStyleVar(StyleVar var);
} // namespace Carbon
