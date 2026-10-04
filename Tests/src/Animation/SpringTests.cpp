#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "Carbon/Animation/Easing.h"
#include "Carbon/Animation/Spring.h"

namespace Carbon
{
    namespace
    {
        // Runs a spring from rest at 0 towards 1 with a fixed step and returns the trajectory.
        std::vector<float> Simulate(float response, float damping, float step, float duration)
        {
            std::vector<float> values;
            SpringState state;
            for (float time = 0.0f; time < duration; time += step)
            {
                state = AdvanceSpring(state, 1.0f, response, damping, step);
                values.push_back(state.Value);
            }
            return values;
        }
    } // namespace

    TEST(SpringTests, ConvergesForEveryDamping)
    {
        for (const float damping : {0.3f, 0.6f, 0.85f, 1.0f, 1.5f, 3.0f})
        {
            SpringState state;
            for (int i = 0; i < 600; i++)
                state = AdvanceSpring(state, 1.0f, 0.3f, damping, 1.0f / 60.0f);
            EXPECT_NEAR(state.Value, 1.0f, 1e-3f) << "damping " << damping;
            EXPECT_NEAR(state.Velocity, 0.0f, 1e-2f) << "damping " << damping;
            EXPECT_TRUE(IsSpringAtRest(state, 1.0f, 10.0f)) << "damping " << damping;
        }
    }

    TEST(SpringTests, CriticallyDampedAndOverdampedNeverOvershoot)
    {
        for (const float damping : {1.0f, 1.5f, 3.0f})
        {
            float previous = 0.0f;
            for (const float value : Simulate(0.4f, damping, 1.0f / 120.0f, 3.0f))
            {
                EXPECT_LE(value, 1.0f + 1e-5f) << "damping " << damping;
                EXPECT_GE(value, previous - 1e-5f) << "damping " << damping; // monotonic
                previous = value;
            }
        }
    }

    TEST(SpringTests, UnderdampedOvershootsThenSettles)
    {
        float peak = 0.0f;
        for (const float value : Simulate(0.4f, 0.4f, 1.0f / 120.0f, 3.0f))
            peak = std::max(peak, value);
        EXPECT_GT(peak, 1.1f);
        EXPECT_LT(peak, 1.5f);
    }

    TEST(SpringTests, ResponseSetsTheSpeed)
    {
        // After one response time a critically damped spring has covered almost the whole distance.
        const SpringState fast = AdvanceSpring(SpringState(), 1.0f, 0.2f, 1.0f, 0.2f);
        const SpringState slow = AdvanceSpring(SpringState(), 1.0f, 0.6f, 1.0f, 0.2f);
        EXPECT_GT(fast.Value, 0.98f);
        EXPECT_LT(slow.Value, 0.7f);
        EXPECT_GT(slow.Value, 0.5f);
    }

    TEST(SpringTests, MatchesTheAnalyticSolution)
    {
        // Critically damped from rest: x(t) = 1 - e^(-wt) (1 + wt), with w = 2 pi / response.
        const float response = 0.5f;
        const float omega = 6.2831853f / response;
        for (const float time : {0.05f, 0.1f, 0.25f, 0.5f})
        {
            const SpringState state = AdvanceSpring(SpringState(), 1.0f, response, 1.0f, time);
            const float expected = 1.0f - std::exp(-omega * time) * (1.0f + omega * time);
            EXPECT_NEAR(state.Value, expected, 1e-5f);
        }
    }

    TEST(SpringTests, ResultIsIndependentOfHowTimeIsSliced)
    {
        // The same total time in different frame patterns must give the same state.
        const std::vector<std::vector<float>> patterns = {
            {0.48f},
            {0.016f, 0.016f, 0.016f, 0.016f, 0.016f, 0.4f},
            {0.007f, 0.033f, 0.1f, 0.02f, 0.2f, 0.12f},
            {0.24f, 0.24f},
        };
        for (const float damping : {0.5f, 1.0f, 2.0f})
        {
            const SpringState reference = AdvanceSpring(SpringState{0.0f, 3.0f}, 10.0f, 0.4f, damping, 0.48f);
            for (const std::vector<float>& pattern : patterns)
            {
                SpringState state{0.0f, 3.0f};
                for (const float step : pattern)
                    state = AdvanceSpring(state, 10.0f, 0.4f, damping, step);
                EXPECT_NEAR(state.Value, reference.Value, 1e-3f) << "damping " << damping;
                EXPECT_NEAR(state.Velocity, reference.Velocity, 2e-2f) << "damping " << damping;
            }
        }

        // 240 small steps equal one large one.
        SpringState fine;
        for (int i = 0; i < 240; i++)
            fine = AdvanceSpring(fine, 1.0f, 0.3f, 0.8f, 0.001f);
        const SpringState coarse = AdvanceSpring(SpringState(), 1.0f, 0.3f, 0.8f, 0.24f);
        EXPECT_NEAR(fine.Value, coarse.Value, 1e-3f);
    }

    TEST(SpringTests, RetargetingKeepsValueAndVelocity)
    {
        // Half-way to 100 the target changes to 0. The spring carries on upwards for a moment, because the
        // velocity is preserved, and only then turns around. That continuity is what makes interruptions smooth.
        SpringState state;
        for (int i = 0; i < 6; i++)
            state = AdvanceSpring(state, 100.0f, 0.3f, 1.0f, 1.0f / 60.0f);
        const SpringState atInterruption = state;
        EXPECT_GT(atInterruption.Velocity, 100.0f);

        // An infinitesimal step towards the new target changes neither value nor velocity noticeably.
        const SpringState justAfter = AdvanceSpring(atInterruption, 0.0f, 0.3f, 1.0f, 1e-5f);
        EXPECT_NEAR(justAfter.Value, atInterruption.Value, 1e-2f);
        EXPECT_NEAR(justAfter.Velocity, atInterruption.Velocity, 1.0f);

        const SpringState nextFrame = AdvanceSpring(atInterruption, 0.0f, 0.3f, 1.0f, 1.0f / 240.0f);
        EXPECT_GT(nextFrame.Value, atInterruption.Value);

        for (int i = 0; i < 300; i++)
            state = AdvanceSpring(state, 0.0f, 0.3f, 1.0f, 1.0f / 60.0f);
        EXPECT_NEAR(state.Value, 0.0f, 1e-2f);
    }

    TEST(SpringTests, DegenerateInputs)
    {
        const SpringState start{2.0f, 5.0f};
        // No time passes: nothing changes.
        const SpringState unchanged = AdvanceSpring(start, 10.0f, 0.3f, 1.0f, 0.0f);
        EXPECT_FLOAT_EQ(unchanged.Value, 2.0f);
        EXPECT_FLOAT_EQ(unchanged.Velocity, 5.0f);
        // A response of zero is an instant jump.
        const SpringState jumped = AdvanceSpring(start, 10.0f, 0.0f, 1.0f, 0.016f);
        EXPECT_FLOAT_EQ(jumped.Value, 10.0f);
        EXPECT_FLOAT_EQ(jumped.Velocity, 0.0f);
        // A very long frame does not blow up.
        const SpringState late = AdvanceSpring(start, 10.0f, 0.3f, 0.5f, 100.0f);
        EXPECT_NEAR(late.Value, 10.0f, 1e-4f);
    }

    TEST(EasingTests, EveryCurveStartsAtZeroAndEndsAtOne)
    {
        for (const Easing easing : {Easing::Linear, Easing::EaseIn, Easing::EaseOut, Easing::EaseInOut})
        {
            EXPECT_FLOAT_EQ(Ease(easing, 0.0f), 0.0f);
            EXPECT_FLOAT_EQ(Ease(easing, 1.0f), 1.0f);
            // Out-of-range progress clamps.
            EXPECT_FLOAT_EQ(Ease(easing, -0.5f), 0.0f);
            EXPECT_FLOAT_EQ(Ease(easing, 1.5f), 1.0f);

            float previous = 0.0f;
            for (int i = 1; i <= 100; i++)
            {
                const float value = Ease(easing, float(i) / 100.0f);
                EXPECT_GE(value, previous - 1e-5f); // monotonic
                previous = value;
            }
        }
    }

    TEST(EasingTests, CurvesHaveTheirCharacteristicShape)
    {
        EXPECT_FLOAT_EQ(Ease(Easing::Linear, 0.3f), 0.3f);
        EXPECT_LT(Ease(Easing::EaseIn, 0.25f), 0.25f);           // slow start
        EXPECT_GT(Ease(Easing::EaseOut, 0.25f), 0.25f);          // fast start
        EXPECT_NEAR(Ease(Easing::EaseInOut, 0.5f), 0.5f, 1e-4f); // symmetric
        EXPECT_LT(Ease(Easing::EaseInOut, 0.2f), 0.2f);
        EXPECT_GT(Ease(Easing::EaseInOut, 0.8f), 0.8f);
    }

    TEST(EasingTests, CubicBezierMatchesKnownValues)
    {
        // Control points on the diagonal make the curve linear.
        for (const float t : {0.1f, 0.37f, 0.9f})
            EXPECT_NEAR(CubicBezier(0.25f, 0.25f, 0.75f, 0.75f, t), t, 1e-4f);
        // CSS "ease" (0.25, 0.1, 0.25, 1.0) is about 0.8 at the half-way point.
        EXPECT_NEAR(CubicBezier(0.25f, 0.1f, 0.25f, 1.0f, 0.5f), 0.8024f, 2e-3f);
    }
} // namespace Carbon
