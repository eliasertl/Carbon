// Charts: a line chart with 1,000 to 100,000 points, with and without a label per point.

#include <cmath>
#include <format>
#include <string>
#include <vector>

#include <Carbon/Extensions/Extensions.h>

#include "Support/FrameBenchmark.h"

namespace Carbon::Benchmarks
{
    namespace
    {
        void LineChartPoints(benchmark::State& state)
        {
            const size_t count = static_cast<size_t>(state.range(0));
            const bool hasLabels = state.range(1) != 0;

            std::vector<float> values(count);
            for (size_t i = 0; i < count; i++)
            {
                const float t = static_cast<float>(i);
                values[i] = std::sin(t * 0.013f) * 40.0f + std::sin(t * 0.21f) * 6.0f + 50.0f;
            }
            std::vector<std::string> labelTexts;
            std::vector<std::string_view> labels;
            if (hasLabels)
            {
                labelTexts.reserve(count);
                for (size_t i = 0; i < count; i++)
                    labelTexts.push_back(std::format("{}", i));
                labels.assign(labelTexts.begin(), labelTexts.end());
            }

            FrameBenchmark frame;
            const auto build = [&]
            {
                BeginVStack({.Padding = 20.0f, .Width = Size::Fill(), .Height = Size::Fill()});
                LineChart("chart", values, {.Height = 400.0f, .Labels = labels});
                EndVStack();
            };
            frame.Measure(state, build);
        }
    } // namespace

    CB_FRAME_BENCHMARK(LineChartPoints)
        ->Name("Charts/LineChart")
        ->ArgNames({"points", "labels"})
        ->ArgsProduct({{1000, 10000, 100000}, {0, 1}});
} // namespace Carbon::Benchmarks
