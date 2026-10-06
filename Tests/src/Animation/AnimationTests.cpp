#include "Support/ContextTest.h"

#include <cmath>
#include <limits>
#include <string>

#include "Carbon/Core/ContextInternal.h"

namespace Carbon
{
    class AnimationTests : public ContextTest
    {
    protected:
        // Runs one frame and returns the animated value of `id` for `target`.
        float Step(ID id, float target, const AnimationSpec& spec = AnimationSpec::Spring(),
                   float deltaTime = FrameTime)
        {
            float value = 0.0f;
            Frame([&] { value = Animate(id, target, spec); }, deltaTime);
            return value;
        }
    };

    TEST_F(AnimationTests, FirstCallReturnsTheTarget)
    {
        const ID id = HashID("value");
        EXPECT_FLOAT_EQ(Step(id, 42.0f), 42.0f);
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(AnimationTests, FollowsANewTargetAndArrives)
    {
        const ID id = HashID("value");
        Step(id, 0.0f);

        const float first = Step(id, 100.0f);
        EXPECT_GT(first, 0.0f);
        EXPECT_LT(first, 100.0f);
        EXPECT_TRUE(IsAnimating());

        float previous = first;
        for (int i = 0; i < 120; i++)
        {
            const float value = Step(id, 100.0f);
            EXPECT_GE(value, previous - 1e-4f); // the default spring is critically damped: no overshoot
            previous = value;
        }
        EXPECT_FLOAT_EQ(previous, 100.0f);
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(AnimationTests, InterruptedSpringContinuesSmoothly)
    {
        const ID id = HashID("highlight");
        Step(id, 0.0f);
        float value = 0.0f;
        for (int i = 0; i < 5; i++)
            value = Step(id, 100.0f);
        const float beforeInterrupt = value;

        // Back to 0 while moving fast towards 100: the value keeps rising for a moment instead of snapping back.
        const float afterInterrupt = Step(id, 0.0f, AnimationSpec::Spring(), 1.0f / 240.0f);
        EXPECT_GT(afterInterrupt, beforeInterrupt);

        for (int i = 0; i < 200; i++)
            value = Step(id, 0.0f);
        EXPECT_FLOAT_EQ(value, 0.0f);
    }

    TEST_F(AnimationTests, IsIndependentOfTheFrameRate)
    {
        const ID smooth = HashID("smooth");
        const ID choppy = HashID("choppy");
        const AnimationSpec spec = AnimationSpec::Spring(0.5f, 0.7f);

        // 0.3 seconds as 30 frames of 10 ms...
        float smoothValue = 0.0f;
        Frame([&] { Animate(smooth, 0.0f, spec); });
        for (int i = 0; i < 30; i++)
            Frame([&] { smoothValue = Animate(smooth, 100.0f, spec); }, 0.01f);
        // ...and as three uneven frames. The first is short: the frame in which an animation starts after
        // stillness counts for 1/30 s at most.
        float choppyValue = 0.0f;
        Frame([&] { Animate(choppy, 0.0f, spec); });
        for (const float deltaTime : {0.02f, 0.23f, 0.05f})
            Frame([&] { choppyValue = Animate(choppy, 100.0f, spec); }, deltaTime);

        EXPECT_NEAR(smoothValue, choppyValue, 0.05f);
    }

    TEST_F(AnimationTests, AdvancesOncePerFrameHoweverOftenItIsRead)
    {
        const ID once = HashID("once");
        const ID twice = HashID("twice");
        Frame(
            [&]
            {
                Animate(once, 0.0f);
                Animate(twice, 0.0f);
            });
        float single = 0.0f;
        float repeated = 0.0f;
        Frame(
            [&]
            {
                single = Animate(once, 100.0f);
                Animate(twice, 100.0f);
                repeated = Animate(twice, 100.0f);
            });
        EXPECT_FLOAT_EQ(single, repeated);
    }

    TEST_F(AnimationTests, StateIsPerIDAndForgottenAfterAGap)
    {
        const ID a = HashID("a");
        const ID b = HashID("b");
        Frame(
            [&]
            {
                Animate(a, 0.0f);
                Animate(b, 50.0f);
            });
        float valueA = 0.0f;
        float valueB = 0.0f;
        Frame(
            [&]
            {
                valueA = Animate(a, 100.0f);
                valueB = Animate(b, 50.0f);
            });
        EXPECT_GT(valueA, 0.0f);
        EXPECT_LT(valueA, 100.0f);
        EXPECT_FLOAT_EQ(valueB, 50.0f);

        // A frame without the widget: its animation state is dropped, so it reappears at its target.
        RunFrame();
        EXPECT_FLOAT_EQ(Step(a, 100.0f), 100.0f);
    }

    TEST_F(AnimationTests, EaseRunsForItsDurationAndRestartsOnRetarget)
    {
        const ID id = HashID("fade");
        const AnimationSpec spec = AnimationSpec::Ease(Easing::Linear, 0.2f);
        Step(id, 0.0f, spec);

        EXPECT_NEAR(Step(id, 1.0f, spec, 0.025f), 0.125f, 1e-4f);
        EXPECT_NEAR(Step(id, 1.0f, spec, 0.075f), 0.5f, 1e-4f);

        // Retargeting restarts the curve from the current value: half of the way back in half the duration.
        EXPECT_NEAR(Step(id, 0.0f, spec, 0.1f), 0.25f, 1e-4f);
        EXPECT_NEAR(Step(id, 0.0f, spec, 0.1f), 0.0f, 1e-4f);
        Step(id, 0.0f, spec);
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(AnimationTests, AnAnimationThatStartsAfterAHostSleptIsNotOverAtOnce)
    {
        // A host that renders on demand slept for five seconds, then a click changes a target: the frame's delta
        // time is the five seconds, and the animation still starts at its beginning.
        const ID fade = HashID("fade");
        const ID spring = HashID("spring");
        const AnimationSpec ease = AnimationSpec::Ease(Easing::Linear, 0.2f);
        float fadeValue = 0.0f;
        float springValue = 0.0f;
        const auto step = [&](float target, float deltaTime)
        {
            Frame(
                [&]
                {
                    fadeValue = Animate(fade, target, ease);
                    springValue = Animate(spring, target * 100.0f);
                },
                deltaTime);
        };
        step(0.0f, FrameTime);
        step(0.0f, FrameTime);
        EXPECT_FALSE(IsAnimating());

        step(1.0f, 5.0f);
        EXPECT_LT(fadeValue, 0.2f);
        EXPECT_LT(springValue, 50.0f);
        EXPECT_TRUE(IsAnimating());

        // While things are moving, a long frame is a long frame.
        step(1.0f, 5.0f);
        EXPECT_FLOAT_EQ(fadeValue, 1.0f);
        EXPECT_NEAR(springValue, 100.0f, 0.01f);
    }

    TEST_F(AnimationTests, NoneJumps)
    {
        const ID id = HashID("instant");
        Step(id, 0.0f, AnimationSpec::None());
        EXPECT_FLOAT_EQ(Step(id, 7.0f, AnimationSpec::None()), 7.0f);
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(AnimationTests, AnimatesVectorsRectsAndColors)
    {
        const ID position = HashID("position");
        const ID frame = HashID("frame");
        const ID tint = HashID("tint");
        Frame(
            [&]
            {
                Animate(position, Vec2(0.0f, 0.0f));
                Animate(frame, Rect(0.0f, 0.0f, 10.0f, 10.0f));
                Animate(tint, Color::Transparent());
            });

        Vec2 point;
        Rect rect;
        Color color;
        for (int i = 0; i < 3; i++)
        {
            Frame(
                [&]
                {
                    point = Animate(position, Vec2(100.0f, 50.0f));
                    rect = Animate(frame, Rect(40.0f, 20.0f, 30.0f, 10.0f));
                    color = Animate(tint, Color(1.0f, 0.0f, 0.0f, 1.0f));
                });
        }
        // Components move together: the point stays on the straight line to its target.
        EXPECT_GT(point.X, 0.0f);
        EXPECT_LT(point.X, 100.0f);
        EXPECT_NEAR(point.Y, point.X * 0.5f, 1e-3f);
        EXPECT_GT(rect.X, 0.0f);
        EXPECT_GT(rect.Width, 10.0f);
        EXPECT_FLOAT_EQ(rect.Height, 10.0f);
        // Fading in from transparent keeps the hue: pure red at partial alpha, not a darkened red.
        EXPECT_GT(color.A, 0.0f);
        EXPECT_LT(color.A, 1.0f);
        EXPECT_NEAR(color.R, 1.0f, 1e-3f);
        EXPECT_NEAR(color.G, 0.0f, 1e-3f);

        for (int i = 0; i < 200; i++)
        {
            Frame(
                [&]
                {
                    point = Animate(position, Vec2(100.0f, 50.0f));
                    color = Animate(tint, Color(1.0f, 0.0f, 0.0f, 1.0f));
                });
        }
        EXPECT_EQ(point, Vec2(100.0f, 50.0f));
        EXPECT_FLOAT_EQ(color.A, 1.0f);
    }

    TEST_F(AnimationTests, SetAnimationValueJumps)
    {
        const ID id = HashID("offset");
        Step(id, 0.0f);
        Frame([&] { SetAnimationValue(id, 80.0f); });
        // From 80 towards 100: already close, and approaching from below.
        const float value = Step(id, 100.0f);
        EXPECT_GT(value, 80.0f);
        EXPECT_LT(value, 100.0f);
    }

    TEST_F(AnimationTests, NextFrameDelayIsInfiniteWhenNothingIsDue)
    {
        Settle([] { Button("Save"); });
        EXPECT_FALSE(IsAnimating());
        EXPECT_EQ(GetNextFrameDelay(), std::numeric_limits<float>::infinity());
    }

    TEST_F(AnimationTests, NextFrameDelayIsTheEarliestRequestOfTheFrame)
    {
        Frame(
            []
            {
                RequestFrameAfter(2.0f);
                RequestFrameAfter(0.25f);
                RequestFrameAfter(1.0f);
            });
        EXPECT_FALSE(IsAnimating());
        EXPECT_FLOAT_EQ(GetNextFrameDelay(), 0.25f);

        // A request lasts for its frame, and anything in motion means: now.
        RunFrame();
        EXPECT_EQ(GetNextFrameDelay(), std::numeric_limits<float>::infinity());
        Frame(
            []
            {
                RequestFrameAfter(3.0f);
                RequestAnimationFrame();
            });
        EXPECT_TRUE(IsAnimating());
        EXPECT_FLOAT_EQ(GetNextFrameDelay(), 0.0f);
    }

    TEST_F(AnimationTests, AFocusedTextFieldAsksForAFrameWhenItsCaretChanges)
    {
        std::string text = "Text";
        const auto build = [&] { TextField("Name", &text); };
        Frame(build);
        Frame(
            [&]
            {
                SetFocus(GetID("Name"));
                build();
            });
        Settle(build, 60);

        // Nothing moves, so a host that renders on demand may sleep: until the caret appears or disappears,
        // which it does twice a second.
        EXPECT_FALSE(IsAnimating());
        const float delay = GetNextFrameDelay();
        EXPECT_GT(delay, 0.0f);
        EXPECT_LE(delay, 0.5f);

        // The caret is one quad. Rendering exactly when asked shows it changing every time.
        const size_t before = GetDrawData().Vertices.size();
        Frame(build, delay + 0.001f);
        const size_t after = GetDrawData().Vertices.size();
        EXPECT_TRUE(after == before + 4 || after + 4 == before);
        EXPECT_NEAR(GetNextFrameDelay(), 0.5f, 0.01f);
        Frame(build, GetNextFrameDelay() + 0.001f);
        EXPECT_EQ(GetDrawData().Vertices.size(), before);
    }

    TEST_F(AnimationTests, AScrollIndicatorAsksForAFrameWhenItHides)
    {
        const auto build = []
        {
            BeginScrollView("content", {.Width = 300.0f, .Height = 200.0f});
            for (int i = 0; i < 100; i++)
                Text("A line of content");
            EndScrollView();
        };
        GetIO().AddMousePosEvent(50.0f, 100.0f);
        Settle(build, 200);
        EXPECT_EQ(GetNextFrameDelay(), std::numeric_limits<float>::infinity());

        // One notch of the wheel: the content glides, then nothing moves while the indicator stays, then it fades.
        GetIO().AddMouseWheelEvent(0.0f, -1.0f);
        Frame(build);
        EXPECT_TRUE(IsAnimating());
        int frames = 0;
        while (IsAnimating() && frames < 200)
        {
            Frame(build);
            frames++;
        }
        EXPECT_LT(frames, 60) << "the glide ends well before the indicator hides";
        const float delay = GetNextFrameDelay();
        EXPECT_GT(delay, 0.1f);
        EXPECT_LE(delay, 1.0f);

        // The host slept for most of a second. The fade starts with this frame and is seen: it is not over
        // because the frame's delta time was long.
        Frame(build, delay + 0.001f);
        EXPECT_TRUE(IsAnimating()) << "the indicator fades out";
        int fadeFrames = 0;
        while (IsAnimating() && fadeFrames < 200)
        {
            Frame(build);
            fadeFrames++;
        }
        EXPECT_GE(fadeFrames, 10);
        EXPECT_EQ(GetNextFrameDelay(), std::numeric_limits<float>::infinity());
    }

    TEST_F(AnimationTests, ReduceMotionMakesMotionInstant)
    {
        SetReduceMotion(true);
        EXPECT_TRUE(GetReduceMotion());
        const ID id = HashID("slide");
        Step(id, 0.0f);
        EXPECT_FLOAT_EQ(Step(id, 300.0f), 300.0f);
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(AnimationTests, ReduceMotionTurnsAppearanceChangesIntoShortFades)
    {
        SetReduceMotion(true);
        const ID opacity = HashID("opacity");
        const ID tint = HashID("tint");
        // A slow, bouncy spring marked as an appearance change.
        const AnimationSpec spec = AnimationSpec::Spring(2.0f, 0.3f).AsAppearance();
        Frame(
            [&]
            {
                Animate(opacity, 0.0f, spec);
                Animate(tint, Color::Black());
            });

        float value = 0.0f;
        Color color;
        for (const float deltaTime : {0.025f, ReducedMotionFadeDuration * 0.5f - 0.025f})
        {
            Frame(
                [&]
                {
                    value = Animate(opacity, 1.0f, spec);
                    color = Animate(tint, Color::White());
                },
                deltaTime);
        }
        // Half-way through a symmetric ease: half-way there, with no bounce.
        EXPECT_NEAR(value, 0.5f, 0.01f);
        EXPECT_NEAR(color.R, 0.5f, 0.01f);

        Frame(
            [&]
            {
                value = Animate(opacity, 1.0f, spec);
                color = Animate(tint, Color::White());
            },
            ReducedMotionFadeDuration);
        EXPECT_FLOAT_EQ(value, 1.0f);
        EXPECT_FLOAT_EQ(color.R, 1.0f);
    }

    TEST_F(AnimationTests, FadeSpecStaysAFadeUnderReduceMotion)
    {
        SetReduceMotion(true);
        const ID id = HashID("fade");
        Step(id, 0.0f, AnimationSpec::Fade());
        const float value = Step(id, 1.0f, AnimationSpec::Fade(), 0.05f);
        EXPECT_GT(value, 0.0f);
        EXPECT_LT(value, 1.0f);
    }
} // namespace Carbon
