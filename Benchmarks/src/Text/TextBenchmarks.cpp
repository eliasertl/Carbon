// Text: many labels of which few are visible, and labels whose text changes every frame.

#include <cstdio>
#include <format>
#include <string>
#include <vector>

#include "Support/FrameBenchmark.h"

namespace Carbon::Benchmarks
{
    namespace
    {
        constexpr int LabelCount = 10000;
        constexpr int ChangingLabelCount = 50;

        // 10,000 text labels in a scroll view that shows about 30 of them, at the top or in the middle.
        void ScrolledLabels(benchmark::State& state)
        {
            const bool isMiddle = state.range(0) != 0;
            std::vector<std::string> labels;
            for (int i = 0; i < LabelCount; i++)
                labels.push_back(std::format("Line {:05} of the log, with a little more text", i));

            FrameBenchmark frame;
            const auto build = [&]
            {
                BeginScrollView("labels", {.Spacing = 8.0f, .Padding = 20.0f});
                for (const std::string& label : labels)
                    Text(label);
                EndScrollView();
            };
            for (int i = 0; i < 3; i++)
                frame.RunFrame(build, 0.25f);
            if (isMiddle)
            {
                frame.RunFrame(
                    [&]
                    {
                        SetScrollOffset("labels", Vec2(0.0f, 24.0f * LabelCount / 2));
                        build();
                    });
            }
            frame.Measure(state, build);
        }

        // 50 labels that show a different text in every frame, like timers and the value next to a slider.
        void ChangingStrings(benchmark::State& state)
        {
            FrameBenchmark frame;
            uint64_t tick = 0;
            const auto build = [&]
            {
                tick++;
                BeginVStack({.Spacing = 4.0f, .Padding = 20.0f});
                for (int i = 0; i < ChangingLabelCount; i++)
                {
                    char text[32];
                    const uint64_t milliseconds = tick * 16 + static_cast<uint64_t>(i) * 7919;
                    const int length = std::snprintf(
                        text, sizeof(text), "%02u:%02u.%03u", static_cast<unsigned>(milliseconds / 60000 % 100),
                        static_cast<unsigned>(milliseconds / 1000 % 60), static_cast<unsigned>(milliseconds % 1000));
                    Text(std::string_view(text, static_cast<size_t>(length)));
                }
                EndVStack();
            };
            frame.Measure(state, build);
        }
    } // namespace

    CB_FRAME_BENCHMARK(ScrolledLabels)->Name("Text/ScrolledLabels")->ArgName("middle")->Arg(0)->Arg(1);
    CB_FRAME_BENCHMARK(ChangingStrings)->Name("Text/ChangingStrings");
} // namespace Carbon::Benchmarks
