#include "Carbon/Style/Style.h"

#include <algorithm>

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
            ColorStack.clear();
            VarStack.clear();
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
        const float t = std::clamp(amount, 0.0f, 1.0f);
        return Color(base.R + (over.R - base.R) * t, base.G + (over.G - base.G) * t, base.B + (over.B - base.B) * t,
                     base.A);
    }

    // Declared in Text/TextStyle.h; implemented here because the type ramp belongs to the theme.
    TextSpec GetTextSpec(TextStyle style, bool emphasized)
    {
        const Theme& theme = Internal::GetContext().Style.Current;
        const TextStyleSpec& entry = theme.GetTextStyle(style < TextStyle::Count ? style : TextStyle::Body);
        TextSpec spec;
        spec.Font = theme.Font;
        spec.Size = entry.Size;
        spec.LineHeight = entry.LineHeight;
        spec.Weight = emphasized ? entry.EmphasizedWeight : entry.Weight;
        return spec;
    }
} // namespace Carbon
