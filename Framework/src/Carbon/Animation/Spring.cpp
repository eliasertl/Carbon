#include "Carbon/Animation/Spring.h"

#include <algorithm>
#include <cmath>

namespace Carbon
{
    SpringState AdvanceSpring(SpringState state, float target, float response, float dampingFraction, float deltaTime)
    {
        if (deltaTime <= 0.0f)
            return state;
        if (response <= 0.0f)
            return SpringState{target, 0.0f};

        // x'' + 2 z w x' + w^2 x = 0, with x the displacement from the target.
        const float omega = 6.2831853f / response;
        const float zeta = std::max(dampingFraction, 0.0f);
        const float x0 = state.Value - target;
        const float v0 = state.Velocity;
        const float t = deltaTime;

        float x = 0.0f;
        float v = 0.0f;
        if (zeta < 0.9999f)
        {
            // Underdamped: a decaying oscillation.
            const float damped = omega * std::sqrt(1.0f - zeta * zeta);
            const float decay = std::exp(-zeta * omega * t);
            const float cosine = std::cos(damped * t);
            const float sine = std::sin(damped * t);
            x = decay * (x0 * cosine + (v0 + zeta * omega * x0) / damped * sine);
            v = decay * (v0 * cosine - (zeta * omega * v0 + omega * omega * x0) / damped * sine);
        }
        else if (zeta <= 1.0001f)
        {
            // Critically damped: the fastest approach without overshoot.
            const float decay = std::exp(-omega * t);
            const float slope = v0 + omega * x0;
            x = decay * (x0 + slope * t);
            v = decay * (v0 - omega * slope * t);
        }
        else
        {
            // Overdamped: two decaying exponentials.
            const float root = std::sqrt(zeta * zeta - 1.0f);
            const float r1 = -omega * (zeta - root);
            const float r2 = -omega * (zeta + root);
            const float c1 = (v0 - r2 * x0) / (r1 - r2);
            const float c2 = x0 - c1;
            const float e1 = std::exp(r1 * t);
            const float e2 = std::exp(r2 * t);
            x = c1 * e1 + c2 * e2;
            v = c1 * r1 * e1 + c2 * r2 * e2;
        }

        return SpringState{target + x, v};
    }

    bool IsSpringAtRest(SpringState state, float target, float scale)
    {
        // A thousandth of a visible unit, and less than a hundredth of a unit per second.
        return std::abs(state.Value - target) < 0.001f * scale && std::abs(state.Velocity) < 0.01f * scale;
    }
} // namespace Carbon
