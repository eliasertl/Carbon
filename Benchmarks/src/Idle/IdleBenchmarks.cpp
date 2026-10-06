// Idle: how many frames per second Carbon asks an event-driven host for while nothing happens. The host renders a
// frame while IsAnimating() is true and otherwise waits for input.

#include <string>

#include "Support/FrameBenchmark.h"

namespace Carbon::Benchmarks
{
    namespace
    {
        // Runs the frames an event-driven host would render during `duration` seconds without input, and returns
        // how many those were. The first frame is the one the host renders anyway.
        template <typename Build>
        int SimulateIdleHost(FrameBenchmark& frame, const Build& build, float duration)
        {
            int frames = 0;
            float time = 0.0f;
            while (time < duration)
            {
                frame.RunFrame(build);
                frames++;
                // Nothing moves: the host sleeps until the next event, and there is none.
                if (!IsAnimating())
                    break;
                time += FrameBenchmark::FrameTime;
            }
            return frames;
        }

        template <typename Build>
        void MeasureIdle(benchmark::State& state, FrameBenchmark& frame, const Build& build, float duration)
        {
            int64_t frames = 0;
            for (auto _ : state)
                frames += SimulateIdleHost(frame, build, duration);
            const double seconds = static_cast<double>(duration) * static_cast<double>(state.iterations());
            state.counters["frames_per_second"] = static_cast<double>(frames) / seconds;
        }

        void BuildForm(std::string& text)
        {
            BeginVStack({.Padding = 20.0f});
            Text("Name");
            TextField("Name", &text);
            Button("Save");
            EndVStack();
        }

        // A focused text field without input: only its caret blinks.
        void FocusedTextField(benchmark::State& state)
        {
            std::string text = "Idle";
            FrameBenchmark frame;
            const auto build = [&] { BuildForm(text); };
            frame.RunFrame(build);
            frame.RunFrame(
                [&]
                {
                    SetFocus(GetID("Name"));
                    build();
                });
            frame.Settle(build);
            MeasureIdle(state, frame, build, 10.0f);
        }

        // The same interface without focus: nothing should ask for frames.
        void StaticInterface(benchmark::State& state)
        {
            std::string text = "Idle";
            FrameBenchmark frame;
            const auto build = [&] { BuildForm(text); };
            frame.Settle(build);
            MeasureIdle(state, frame, build, 10.0f);
        }

        // One notch of the mouse wheel over a scroll view, then nothing for three seconds: the frames until the
        // scroll indicator has faded out again.
        void AfterScrolling(benchmark::State& state)
        {
            FrameBenchmark frame;
            const auto build = [&]
            {
                BeginScrollView("content", {.Spacing = 8.0f, .Padding = 20.0f});
                for (int i = 0; i < 200; i++)
                    Text("A line of content");
                EndScrollView();
            };
            GetIO().AddMousePosEvent(400.0f, 300.0f);
            frame.Settle(build);

            int64_t frames = 0;
            for (auto _ : state)
            {
                GetIO().AddMouseWheelEvent(0.0f, -1.0f);
                frames += SimulateIdleHost(frame, build, 3.0f);
            }
            state.counters["frames_per_scroll"] = static_cast<double>(frames) / static_cast<double>(state.iterations());
        }
    } // namespace

    BENCHMARK(FocusedTextField)->Name("Idle/FocusedTextField")->Unit(benchmark::kMicrosecond);
    BENCHMARK(StaticInterface)->Name("Idle/StaticInterface")->Unit(benchmark::kMicrosecond);
    BENCHMARK(AfterScrolling)->Name("Idle/AfterScrolling")->Unit(benchmark::kMicrosecond);
} // namespace Carbon::Benchmarks
