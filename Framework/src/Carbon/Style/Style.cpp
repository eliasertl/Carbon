#include "Carbon/Style/Style.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Animation/Animation.h"
#include "Carbon/Animation/Easing.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Style/StyleInternal.h"

namespace Carbon
{
    namespace Internal
    {
        void StyleState::SetTheme(const Theme& theme, bool animated)
        {
            if (animated)
            {
                Source = Current;
                Elapsed = 0.0f;
            }
            else
            {
                Current = theme;
                Elapsed = FinishedElapsed;
            }
            Target = theme;
        }

        bool StyleState::Advance(float deltaTime, bool reduceMotion)
        {
            if (Elapsed >= FinishedElapsed)
                return false;

            const float duration = reduceMotion ? ReducedMotionFadeDuration : ThemeTransitionDuration;
            Elapsed += deltaTime;
            if (Elapsed >= duration)
            {
                Current = Target;
                Elapsed = FinishedElapsed;
                return false;
            }
            Current = Lerp(Source, Target, Ease(Easing::EaseInOut, Elapsed / duration));
            return true;
        }

        void StyleState::ResetWorkingValues()
        {
            Colors = Current.Colors;
            Vars = Current.Vars;
            // Touch mode: controls take the height of macOS's large size and stand further apart, so that 44-point
            // hit areas barely overlap (30 + 14 points from one control to the next).
            if (IsTouchMode)
            {
                float& height = Vars[static_cast<size_t>(StyleVar::ControlHeight)];
                float& spacing = Vars[static_cast<size_t>(StyleVar::Spacing)];
                height = std::round(height * TouchControlHeightFactor);
                spacing = std::round(spacing * TouchSpacingFactor);
            }
            // Larger text needs taller controls; smaller text keeps them, so that they stay easy to hit.
            if (TextScale > 1.0f)
            {
                float& height = Vars[static_cast<size_t>(StyleVar::ControlHeight)];
                height = std::round(height * TextScale);
            }
            Font = Current.Font;
            ColorStack.clear();
            VarStack.clear();
            FontStack.clear();
        }

        void StyleState::PushColor(StyleColor color, Color value)
        {
            const size_t index = static_cast<size_t>(color);
            ColorStack.push_back(ColorEntry{color, Colors[index]});
            Colors[index] = value;
        }

        bool StyleState::PopColors(int count)
        {
            bool isBalanced = true;
            for (int i = 0; i < count; i++)
            {
                if (ColorStack.empty())
                {
                    isBalanced = false;
                    break;
                }
                Colors[static_cast<size_t>(ColorStack.back().Index)] = ColorStack.back().Previous;
                ColorStack.pop_back();
            }
            return isBalanced;
        }

        void StyleState::PushVar(StyleVar var, float value)
        {
            const size_t index = static_cast<size_t>(var);
            VarStack.push_back(VarEntry{var, Vars[index]});
            Vars[index] = value;
        }

        bool StyleState::PopVars(int count)
        {
            bool isBalanced = true;
            for (int i = 0; i < count; i++)
            {
                if (VarStack.empty())
                {
                    isBalanced = false;
                    break;
                }
                Vars[static_cast<size_t>(VarStack.back().Index)] = VarStack.back().Previous;
                VarStack.pop_back();
            }
            return isBalanced;
        }

        void StyleState::PushFont(Carbon::Font* font)
        {
            FontStack.push_back(Font);
            Font = font != nullptr ? font : Current.Font;
        }

        bool StyleState::PopFonts(int count)
        {
            bool isBalanced = true;
            for (int i = 0; i < count; i++)
            {
                if (FontStack.empty())
                {
                    isBalanced = false;
                    break;
                }
                Font = FontStack.back();
                FontStack.pop_back();
            }
            return isBalanced;
        }
    } // namespace Internal

    void SetTheme(const Theme& theme, bool animated)
    {
        Context& context = Internal::GetContext();
        // Before the first frame there is nothing on screen to animate from.
        const bool isAnimated = animated && context.FrameCount > 0;
        context.Style.SetTheme(theme, isAnimated);
        // An instant switch between frames is visible to lookups right away.
        if (!isAnimated && !context.IsInFrame)
            context.Style.ResetWorkingValues();
    }

    const Theme& GetTheme()
    {
        return Internal::GetContext().Style.Current;
    }

    const Theme& GetTargetTheme()
    {
        return Internal::GetContext().Style.Target;
    }

    void PushStyleColor(StyleColor color, Color value)
    {
        CB_VERIFY(color < StyleColor::Count, "Invalid style color {}", static_cast<int>(color));
        if (color < StyleColor::Count)
            Internal::GetContext().Style.PushColor(color, value);
    }

    void PopStyleColor(int count)
    {
        const bool isBalanced = Internal::GetContext().Style.PopColors(count);
        CB_VERIFY(isBalanced, "PopStyleColor called without a matching PushStyleColor");
    }

    void PushStyleVar(StyleVar var, float value)
    {
        CB_VERIFY(var < StyleVar::Count, "Invalid style variable {}", static_cast<int>(var));
        if (var < StyleVar::Count)
            Internal::GetContext().Style.PushVar(var, value);
    }

    void PopStyleVar(int count)
    {
        const bool isBalanced = Internal::GetContext().Style.PopVars(count);
        CB_VERIFY(isBalanced, "PopStyleVar called without a matching PushStyleVar");
    }

    void PushFont(Font* font)
    {
        Internal::GetContext().Style.PushFont(font);
    }

    void PopFont(int count)
    {
        const bool isBalanced = Internal::GetContext().Style.PopFonts(count);
        CB_VERIFY(isBalanced, "PopFont called without a matching PushFont");
    }

    Color GetStyleColor(StyleColor color)
    {
        return Internal::GetContext().Style.GetColor(color);
    }

    float GetStyleVar(StyleVar var)
    {
        return Internal::GetContext().Style.GetVar(var);
    }

    Color Resolve(const std::optional<Color>& perCall, StyleColor fallback)
    {
        return perCall.has_value() ? *perCall : GetStyleColor(fallback);
    }

    float Resolve(const std::optional<float>& perCall, StyleVar fallback)
    {
        return perCall.has_value() ? *perCall : GetStyleVar(fallback);
    }

    Color Blend(const Color& base, const Color& over, float amount)
    {
        // `over` at an opacity of `amount`, composited on top of `base`. For an opaque base this is a plain mix
        // of the two colors. For a translucent one (the control fill) the result also becomes more opaque, which
        // is what makes the tint visible: mixing only the color of a 16 % fill changes almost nothing.
        const float overAlpha = std::clamp(amount, 0.0f, 1.0f) * over.A;
        const float baseAlpha = base.A * (1.0f - overAlpha);
        const float alpha = overAlpha + baseAlpha;
        if (alpha <= 0.0f)
            return Color(base.R, base.G, base.B, 0.0f);
        return Color((over.R * overAlpha + base.R * baseAlpha) / alpha,
                     (over.G * overAlpha + base.G * baseAlpha) / alpha,
                     (over.B * overAlpha + base.B * baseAlpha) / alpha, alpha);
    }

    // Declared in Text/TextStyle.h; implemented here because the type ramp belongs to the theme.
    TextSpec GetTextSpec(TextStyle style, bool emphasized)
    {
        const Internal::StyleState& state = Internal::GetContext().Style;
        const TextStyleSpec& entry = state.Current.GetTextStyle(style < TextStyle::Count ? style : TextStyle::Body);
        TextSpec spec;
        // The pushed font, else the theme's; a widget's own font option is applied by the widget.
        spec.Font = state.Font;
        // The host's text size (IO::SetTextScale) applies to every style of the type ramp.
        spec.Size = entry.Size * state.TextScale;
        spec.LineHeight = entry.LineHeight * state.TextScale;
        spec.Weight = emphasized ? entry.EmphasizedWeight : entry.Weight;
        return spec;
    }
} // namespace Carbon
