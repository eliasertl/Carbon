#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "Carbon/Style/Theme.h"

namespace Carbon::Internal
{
    /// The styling state of one context: the theme, its running transition, and the style stack.
    ///
    /// Lookups read the working values: a copy of the current theme's colors and metrics, taken at the start of
    /// the frame, that PushStyleColor and PushStyleVar overwrite and the matching pops restore.
    struct StyleState
    {
        /// Elapsed value that marks "no transition running".
        static constexpr float FinishedElapsed = 1.0e9f;

        /// Switches to a theme, optionally starting a transition from the current blend.
        void SetTheme(const Theme& theme, bool animated);

        /// Advances a running transition. Returns true while it is still running.
        bool Advance(float deltaTime, bool reduceMotion);

        /// Resets the working values to the current theme and empties the style stack.
        void ResetWorkingValues();

        Color GetColor(StyleColor color) const { return Colors[static_cast<size_t>(color)]; }
        float GetVar(StyleVar var) const { return Vars[static_cast<size_t>(var)]; }

        void PushColor(StyleColor color, Color value);
        /// Returns false when the stack holds fewer than `count` entries; pops what is there.
        bool PopColors(int count);
        void PushVar(StyleVar var, float value);
        bool PopVars(int count);
        void PushFont(Carbon::Font* font);
        bool PopFonts(int count);

        Theme Current = Theme::Light();
        Theme Source = Theme::Light();
        Theme Target = Theme::Light();
        float Elapsed = FinishedElapsed;

        std::array<Color, static_cast<size_t>(StyleColor::Count)> Colors = Current.Colors;
        std::array<float, static_cast<size_t>(StyleVar::Count)> Vars = Current.Vars;
        /// The font of text: the innermost pushed font, else the theme's. Null means the embedded default.
        Carbon::Font* Font = nullptr;

        struct ColorEntry
        {
            StyleColor Index;
            Color Previous;
        };
        struct VarEntry
        {
            StyleVar Index;
            float Previous;
        };
        std::vector<ColorEntry> ColorStack;
        std::vector<VarEntry> VarStack;
        /// The font each PushFont replaced.
        std::vector<Carbon::Font*> FontStack;
    };
} // namespace Carbon::Internal
