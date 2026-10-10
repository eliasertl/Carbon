#pragma once

#include <optional>

#include "Carbon/Core/Color.h"
#include "Carbon/Style/StyleColor.h"
#include "Carbon/Style/StyleVar.h"
#include "Carbon/Style/Theme.h"
#include "Carbon/Text/Font.h"

namespace Carbon
{
    /// Styling has three layers. From strongest to weakest:
    ///
    ///  1. the per-call option of a widget, e.g. `Button("Delete", { .CornerRadius = 12.0f })`;
    ///  2. the style stack: PushStyleColor / PushStyleVar / PushFont, in effect until the matching pop;
    ///  3. the theme.
    ///
    /// GetStyleColor and GetStyleVar return layers 2 and 3 combined; Resolve adds layer 1.

    /// Duration of an animated theme switch, in seconds.
    inline constexpr float ThemeTransitionDuration = 0.35f;

    /// In touch mode (Carbon/Input/Adaptive.h) the theme's ControlHeight and Spacing are multiplied by these:
    /// 24-point controls become 30 points tall, as macOS's large size, and 8 points of spacing become 14.
    inline constexpr float TouchControlHeightFactor = 1.25f;
    inline constexpr float TouchSpacingFactor = 1.75f;

    /// Sets the theme of the current context. Carbon cannot detect the OS appearance; the host decides.
    /// When `animated`, every color and metric glides from its current value to the new theme's over
    /// ThemeTransitionDuration (a short cross-fade when reduced motion is on). The very first theme, set before
    /// any frame, applies immediately.
    void SetTheme(const Theme& theme, bool animated = true);

    /// The theme as it is this frame, without the style stack. During an animated switch this is the blend of
    /// the old and the new theme.
    const Theme& GetTheme();

    /// The theme most recently passed to SetTheme: where a running transition is heading.
    const Theme& GetTargetTheme();

    /// Overrides a semantic color until the matching PopStyleColor. Affects everything drawn in between.
    void PushStyleColor(StyleColor color, Color value);
    /// Undoes the last `count` PushStyleColor calls.
    void PopStyleColor(int count = 1);

    /// Overrides a metric until the matching PopStyleVar.
    void PushStyleVar(StyleVar var, float value);
    /// Undoes the last `count` PushStyleVar calls.
    void PopStyleVar(int count = 1);

    /// Makes `font` the font of all text drawn by any widget until the matching PopFont. Pushes nest. A font given
    /// in a widget's options (TextOptions::Font) still wins; null pushes the theme's font back.
    void PushFont(Font* font);
    /// Undoes the last `count` PushFont calls.
    void PopFont(int count = 1);

    /// The value of a semantic color: the innermost pushed override, or the theme's color.
    Color GetStyleColor(StyleColor color);
    /// The value of a metric: the innermost pushed override, or the theme's value.
    float GetStyleVar(StyleVar var);

    /// Applies the precedence rules: the per-call value if the caller gave one, else the style stack, else the
    /// theme. Components use this for every option that can be overridden per call.
    Color Resolve(const std::optional<Color>& perCall, StyleColor fallback);
    float Resolve(const std::optional<float>& perCall, StyleVar fallback);

    /// Lays `over` on top of `base` at an opacity of `amount` (0..1). Used for hover and pressed tints: with an
    /// opaque base it mixes the two colors, with a translucent base the result also gets more opaque.
    Color Blend(const Color& base, const Color& over, float amount);
} // namespace Carbon
