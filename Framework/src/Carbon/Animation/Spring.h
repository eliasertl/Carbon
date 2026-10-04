#pragma once

namespace Carbon
{
    /// Position and velocity of a spring-driven value.
    struct SpringState
    {
        float Value = 0.0f;
        float Velocity = 0.0f;
    };

    /// Advances a damped spring towards `target` by `deltaTime` seconds and returns the new state.
    ///
    /// The spring is parameterized like Apple's: `response` is roughly how long, in seconds, it takes to get
    /// there, and `dampingFraction` is 1 for no overshoot (critically damped), below 1 for bounce and above 1 for
    /// a slower, overdamped approach.
    ///
    /// This is the exact solution of the damped harmonic oscillator, not a numerical integration, so the result
    /// does not depend on how time is sliced into frames: advancing by 16 ms twice equals advancing by 32 ms once.
    SpringState AdvanceSpring(SpringState state, float target, float response, float dampingFraction, float deltaTime);

    /// True when a spring is close enough to its target, and slow enough, to be considered at rest.
    /// `scale` is the size of one visible unit of the animated value (1 for points, about 1/255 for colors).
    bool IsSpringAtRest(SpringState state, float target, float scale = 1.0f);
} // namespace Carbon
