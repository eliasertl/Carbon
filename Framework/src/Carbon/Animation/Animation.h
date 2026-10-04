#pragma once

#include <cstdint>

#include "Carbon/Animation/Easing.h"
#include "Carbon/Core/Color.h"
#include "Carbon/Core/ID.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Core/Vec2.h"

namespace Carbon
{
    /// How an animated value moves towards its target.
    enum class AnimationKind : uint8_t
    {
        /// No animation: the value is always its target.
        None,
        /// A damped spring. Interruptible: a new target keeps the current velocity.
        Spring,
        /// A timing curve over a fixed duration. A new target restarts the curve from the current value.
        Ease
    };

    /// What an animation shows, which decides how it behaves when the user asks for reduced motion.
    enum class AnimationTrait : uint8_t
    {
        /// Something moves or changes size. With reduced motion the value jumps to its target.
        Motion,
        /// A color or opacity changes. With reduced motion this becomes a short cross-fade.
        Appearance
    };

    /// Describes an animation. Build one with Spring(), Ease(), Fade() or None().
    struct AnimationSpec
    {
        AnimationKind Kind = AnimationKind::Spring;
        /// Spring: roughly the time in seconds to reach the target.
        float Response = 0.3f;
        /// Spring: 1 is critically damped (no overshoot); below 1 bounces.
        float DampingFraction = 1.0f;
        /// Ease: the timing curve and its duration in seconds.
        Easing Curve = Easing::EaseInOut;
        float Duration = 0.2f;
        AnimationTrait Trait = AnimationTrait::Motion;

        /// A spring, parameterized like SwiftUI's: response in seconds and damping fraction.
        static constexpr AnimationSpec Spring(float response = 0.3f, float dampingFraction = 1.0f)
        {
            AnimationSpec spec;
            spec.Kind = AnimationKind::Spring;
            spec.Response = response;
            spec.DampingFraction = dampingFraction;
            return spec;
        }

        /// A timing curve over a duration in seconds.
        static constexpr AnimationSpec Ease(Easing curve, float duration)
        {
            AnimationSpec spec;
            spec.Kind = AnimationKind::Ease;
            spec.Curve = curve;
            spec.Duration = duration;
            return spec;
        }

        /// A short ease for colors and opacity; stays a fade under reduced motion.
        static constexpr AnimationSpec Fade(float duration = 0.15f)
        {
            AnimationSpec spec = Ease(Easing::EaseOut, duration);
            spec.Trait = AnimationTrait::Appearance;
            return spec;
        }

        /// No animation.
        static constexpr AnimationSpec None()
        {
            AnimationSpec spec;
            spec.Kind = AnimationKind::None;
            return spec;
        }

        /// The same animation, marked as a change of appearance (color, opacity) rather than motion.
        constexpr AnimationSpec AsAppearance() const
        {
            AnimationSpec spec = *this;
            spec.Trait = AnimationTrait::Appearance;
            return spec;
        }
    };

    /// Duration of the cross-fade that replaces animations when reduced motion is on.
    inline constexpr float ReducedMotionFadeDuration = 0.15f;

    /// Animates a value that belongs to `id` towards `target` and returns its value for this frame. Call it every
    /// frame with the current target; when the target changes, the value follows according to `spec`.
    ///
    /// The first call for an ID returns the target (nothing animates when a widget appears). State is kept per ID
    /// and dropped when a frame passes without a call, so use a distinct ID per animated value, for example
    /// GetID("hover") inside the widget's ID scope.
    float Animate(ID id, float target, const AnimationSpec& spec = AnimationSpec::Spring());
    Vec2 Animate(ID id, Vec2 target, const AnimationSpec& spec = AnimationSpec::Spring());
    Rect Animate(ID id, const Rect& target, const AnimationSpec& spec = AnimationSpec::Spring());
    /// Colors are a change of appearance: they keep fading under reduced motion whatever the spec's trait.
    Color Animate(ID id, const Color& target, const AnimationSpec& spec = AnimationSpec::Spring());

    /// Makes the animated value of `id` jump to `value` (and rest there) without animating.
    void SetAnimationValue(ID id, float value);
    void SetAnimationValue(ID id, Vec2 value);
    void SetAnimationValue(ID id, const Rect& value);
    void SetAnimationValue(ID id, const Color& value);

    /// Turns motion into short cross-fades or instant changes, for users who are sensitive to movement.
    /// The host decides; Carbon cannot read the OS setting.
    void SetReduceMotion(bool enabled);
    bool GetReduceMotion();

    /// True when something was still moving in the last finished frame (an animation, a theme transition or
    /// layout that has not settled). Hosts that render on demand keep rendering frames while this is true.
    bool IsAnimating();
} // namespace Carbon
