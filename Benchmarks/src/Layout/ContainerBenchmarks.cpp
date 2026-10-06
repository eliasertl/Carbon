// Layout: containers opened from one call site in a loop, without PushID.

#include "Support/FrameBenchmark.h"

namespace Carbon::Benchmarks
{
    namespace
    {
        void ContainersInLoop(benchmark::State& state)
        {
            const int count = static_cast<int>(state.range(0));
            // The frames get long, so there are fewer of them before the measurement: one in which the stacks
            // appear, one in which they fade in, and a few settled ones.
            FrameBenchmark frame({.SettleFrames = 3, .WarmUpFrames = 2});
            const auto build = [&]
            {
                BeginScrollView("containers", {.Spacing = 0.0f});
                for (int i = 0; i < count; i++)
                {
                    BeginHStack();
                    AllocateItem(Vec2(40.0f, 20.0f));
                    EndHStack();
                }
                EndScrollView();
            };
            frame.Measure(state, build);
        }
    } // namespace

    CB_FRAME_BENCHMARK(ContainersInLoop)
        ->Name("Layout/ContainersInLoop")
        ->ArgName("stacks")
        ->Arg(100)
        ->Arg(1000)
        ->Arg(10000);
} // namespace Carbon::Benchmarks
