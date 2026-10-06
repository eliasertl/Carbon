#include "Carbon/Animation/Animation.h"

#include <algorithm>

#include "Carbon/Animation/Spring.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/State.h"

namespace Carbon
{
    namespace
    {
        constexpr int MaxComponents = 4;
        constexpr float FinishedElapsed = 1.0e9f;
        // The most an animation advances in the frame in which it starts: two frames at 60 Hz.
        constexpr float MaxStartStep = 1.0f / 30.0f;

        // The animated value of one ID: up to four components (a rectangle or a color).
        struct AnimationState
        {
            float Value[MaxComponents];
            float Velocity[MaxComponents];
            float Target[MaxComponents];
            // Ease: where the current run started and how long it has been running.
            float Start[MaxComponents];
            float Elapsed;
            uint64_t LastFrame;
        };

        void Jump(AnimationState& state, const float* target, int count)
        {
            for (int i = 0; i < count; i++)
            {
                state.Value[i] = target[i];
                state.Target[i] = target[i];
                state.Start[i] = target[i];
                state.Velocity[i] = 0.0f;
            }
            // An ease that is not running counts as finished.
            state.Elapsed = FinishedElapsed;
        }

        // Advances the state of `id` by this frame's delta time and writes the current value to `result`.
        // `scale` is the size of one visible unit, used to decide when a spring is at rest.
        void AnimateComponents(ID id, const float* target, int count, AnimationSpec spec, float scale, float* result)
        {
            Context& context = Internal::GetContext();
            bool created = false;
            AnimationState& state = *GetState<AnimationState>(id, StateLifetime::Transient, &created);

            if (context.ReduceMotion && spec.Kind != AnimationKind::None)
            {
                if (spec.Trait == AnimationTrait::Motion)
                    spec.Kind = AnimationKind::None;
                else
                    spec = AnimationSpec::Ease(Easing::EaseInOut, ReducedMotionFadeDuration);
            }

            if (created || spec.Kind == AnimationKind::None)
            {
                Jump(state, target, count);
            }
            else
            {
                bool retargeted = false;
                bool wasAtRest = true;
                for (int i = 0; i < count; i++)
                {
                    retargeted = retargeted || state.Target[i] != target[i];
                    wasAtRest = wasAtRest && state.Value[i] == state.Target[i] && state.Velocity[i] == 0.0f;
                }
                // An animation that starts in a frame after one in which nothing moved starts now. A host that
                // renders on demand may have slept in between, and then the frame's delta time is the length of
                // the sleep; taking that out of the animation would finish it before it was ever seen.
                const bool startsNow =
                    retargeted && !context.WasAnimatingLastFrame && (wasAtRest || spec.Kind != AnimationKind::Spring);
                const float deltaTime = startsNow ? std::min(context.DeltaTime, MaxStartStep) : context.DeltaTime;
                if (retargeted)
                {
                    // A spring keeps its value and velocity. An ease restarts from where the value is now.
                    for (int i = 0; i < count; i++)
                    {
                        state.Target[i] = target[i];
                        state.Start[i] = state.Value[i];
                    }
                    state.Elapsed = 0.0f;
                }

                // Advance once per frame, however often the value is requested.
                if (state.LastFrame != context.FrameCount)
                {
                    bool isMoving = false;
                    if (spec.Kind == AnimationKind::Spring)
                    {
                        for (int i = 0; i < count; i++)
                        {
                            SpringState spring{state.Value[i], state.Velocity[i]};
                            if (IsSpringAtRest(spring, target[i], scale))
                            {
                                spring = SpringState{target[i], 0.0f};
                            }
                            else
                            {
                                spring =
                                    AdvanceSpring(spring, target[i], spec.Response, spec.DampingFraction, deltaTime);
                                isMoving = true;
                            }
                            state.Value[i] = spring.Value;
                            state.Velocity[i] = spring.Velocity;
                        }
                    }
                    else
                    {
                        const bool isRunning = spec.Duration > 0.0f && state.Elapsed < spec.Duration;
                        if (isRunning)
                            state.Elapsed += deltaTime;
                        const float progress = spec.Duration > 0.0f ? state.Elapsed / spec.Duration : 1.0f;
                        const float eased = Ease(spec.Curve, progress);
                        for (int i = 0; i < count; i++)
                        {
                            state.Value[i] =
                                progress >= 1.0f ? target[i] : state.Start[i] + (target[i] - state.Start[i]) * eased;
                            state.Velocity[i] = 0.0f;
                        }
                        isMoving = progress < 1.0f;
                    }
                    context.IsAnimatingThisFrame = context.IsAnimatingThisFrame || isMoving;
                }
            }

            state.LastFrame = context.FrameCount;
            for (int i = 0; i < count; i++)
                result[i] = state.Value[i];
        }

        void SetComponents(ID id, const float* value, int count)
        {
            Context& context = Internal::GetContext();
            AnimationState& state = *GetState<AnimationState>(id, StateLifetime::Transient);
            Jump(state, value, count);
            state.LastFrame = context.FrameCount;
        }
    } // namespace

    float Animate(ID id, float target, const AnimationSpec& spec)
    {
        float result = target;
        AnimateComponents(id, &target, 1, spec, 1.0f, &result);
        return result;
    }

    Vec2 Animate(ID id, Vec2 target, const AnimationSpec& spec)
    {
        const float components[2] = {target.X, target.Y};
        float result[2] = {target.X, target.Y};
        AnimateComponents(id, components, 2, spec, 1.0f, result);
        return Vec2(result[0], result[1]);
    }

    Rect Animate(ID id, const Rect& target, const AnimationSpec& spec)
    {
        const float components[4] = {target.X, target.Y, target.Width, target.Height};
        float result[4] = {target.X, target.Y, target.Width, target.Height};
        AnimateComponents(id, components, 4, spec, 1.0f, result);
        return Rect(result[0], result[1], result[2], result[3]);
    }

    Color Animate(ID id, const Color& target, const AnimationSpec& spec)
    {
        // Animated premultiplied, so a fade to or from transparent does not shift the hue.
        const float components[4] = {target.R * target.A, target.G * target.A, target.B * target.A, target.A};
        float result[4] = {components[0], components[1], components[2], components[3]};
        AnimateComponents(id, components, 4, spec.AsAppearance(), 1.0f / 255.0f, result);
        if (result[3] <= 0.0001f)
            return Color(target.R, target.G, target.B, 0.0f);
        return Color(result[0] / result[3], result[1] / result[3], result[2] / result[3], result[3]);
    }

    void SetAnimationValue(ID id, float value)
    {
        SetComponents(id, &value, 1);
    }

    void SetAnimationValue(ID id, Vec2 value)
    {
        const float components[2] = {value.X, value.Y};
        SetComponents(id, components, 2);
    }

    void SetAnimationValue(ID id, const Rect& value)
    {
        const float components[4] = {value.X, value.Y, value.Width, value.Height};
        SetComponents(id, components, 4);
    }

    void SetAnimationValue(ID id, const Color& value)
    {
        const float components[4] = {value.R * value.A, value.G * value.A, value.B * value.A, value.A};
        SetComponents(id, components, 4);
    }

    void SetReduceMotion(bool enabled)
    {
        Internal::GetContext().ReduceMotion = enabled;
    }

    bool GetReduceMotion()
    {
        return Internal::GetContext().ReduceMotion;
    }

    bool IsAnimating()
    {
        return Internal::GetContext().WasAnimatingLastFrame;
    }

    void RequestAnimationFrame()
    {
        Internal::GetContext().IsAnimatingThisFrame = true;
    }

    void RequestFrameAfter(float seconds)
    {
        Context& context = Internal::GetContext();
        context.NextFrameDelayThisFrame = std::min(context.NextFrameDelayThisFrame, std::max(seconds, 0.0f));
    }

    float GetNextFrameDelay()
    {
        const Context& context = Internal::GetContext();
        return context.WasAnimatingLastFrame ? 0.0f : context.NextFrameDelayLastFrame;
    }
} // namespace Carbon
