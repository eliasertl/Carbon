#include "Carbon/Style/Style.h"

#include "Carbon/Animation/Animation.h"
#include "Carbon/Animation/Easing.h"
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
    } // namespace Internal

    void SetTheme(const Theme& theme, bool animated)
    {
        Context& context = Internal::GetContext();
        // Before the first frame there is nothing on screen to animate from.
        context.Style.SetTheme(theme, animated && context.FrameCount > 0);
    }

    const Theme& GetTheme()
    {
        return Internal::GetContext().Style.Current;
    }

    const Theme& GetTargetTheme()
    {
        return Internal::GetContext().Style.Target;
    }

    Color GetStyleColor(StyleColor color)
    {
        return Internal::GetContext().Style.Current.GetColor(color);
    }

    float GetStyleVar(StyleVar var)
    {
        return Internal::GetContext().Style.Current.GetVar(var);
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
