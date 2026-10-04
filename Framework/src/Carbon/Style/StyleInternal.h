#pragma once

#include "Carbon/Style/Theme.h"

namespace Carbon::Internal
{
    /// The styling state of one context: the theme and its running transition.
    struct StyleState
    {
        /// Elapsed value that marks "no transition running".
        static constexpr float FinishedElapsed = 1.0e9f;

        /// Switches to a theme, optionally starting a transition from the current blend.
        void SetTheme(const Theme& theme, bool animated);

        /// Advances a running transition. Returns true while it is still running.
        bool Advance(float deltaTime, bool reduceMotion);

        Theme Current = Theme::Light();
        Theme Source = Theme::Light();
        Theme Target = Theme::Light();
        float Elapsed = FinishedElapsed;
    };
} // namespace Carbon::Internal
