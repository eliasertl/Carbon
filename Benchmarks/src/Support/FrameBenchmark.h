#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <benchmark/benchmark.h>

#include <Carbon/Carbon.h>

#include "Support/AllocationCounter.h"

/// Registers a function as a frame benchmark: times are the ones FrameBenchmark::Measure reports, in microseconds.
#define CB_FRAME_BENCHMARK(function) BENCHMARK(function)->UseManualTime()->Unit(benchmark::kMicrosecond)

namespace Carbon::Benchmarks
{
    /// How a frame benchmark is set up. All fields are optional.
    struct FrameBenchmarkOptions
    {
        /// Size of the display in points.
        Vec2 DisplaySize = Vec2(1280.0f, 800.0f);
        float ContentScale = 1.0f;
        /// Frames of a quarter second each that come first: layout settles, appear fades and scroll indicators
        /// finish.
        int SettleFrames = 10;
        /// Frames of 1/60 s that follow, so that the measured frames continue a steady state.
        int WarmUpFrames = 4;
        /// Prints the warnings and errors Carbon logs. Off for a scenario that provokes one on purpose.
        bool PrintsWarnings = true;
    };

    /// A headless Carbon context (no GPU) and the measurement of its steady-state frames.
    ///
    /// Measure reports, per frame from NewFrame to EndFrame: the time (Google Benchmark's own column is the mean;
    /// `p50_us` and `p95_us` are the median and the 95th percentile), the calls of the global `operator new`
    /// (`allocs`), the size of the draw data (`vertices`, `indices`, `primitives`, `commands`, `draw_bytes`) and
    /// the glyph-atlas bytes handed to the renderer backend (`atlas_bytes`; `atlas_full_per_1k` counts the complete
    /// uploads per 1,000 frames).
    class FrameBenchmark
    {
    public:
        static constexpr float FrameTime = 1.0f / 60.0f;

        explicit FrameBenchmark(const FrameBenchmarkOptions& options = {});
        ~FrameBenchmark();

        FrameBenchmark(const FrameBenchmark&) = delete;
        FrameBenchmark& operator=(const FrameBenchmark&) = delete;

        /// Runs one frame whose interface `build` builds, outside of the measurement.
        template <typename Build>
        void RunFrame(const Build& build, float deltaTime = FrameTime)
        {
            GetIO().SetDeltaTime(deltaTime);
            NewFrame();
            build();
            EndFrame();
            RenderDrawData();
        }

        /// Runs the settle and warm-up frames.
        template <typename Build>
        void Settle(const Build& build)
        {
            for (int i = 0; i < m_Options.SettleFrames; i++)
                RunFrame(build, 0.25f);
            for (int i = 0; i < m_Options.WarmUpFrames; i++)
                RunFrame(build);
        }

        /// Settles, then measures frames of `build` until Google Benchmark has enough, and reports the counters.
        template <typename Build>
        void Measure(benchmark::State& state, const Build& build)
        {
            Settle(build);
            MeasureSettled(state, build);
        }

        /// Measure without the settle frames, for a benchmark that prepares its own state.
        template <typename Build>
        void MeasureSettled(benchmark::State& state, const Build& build)
        {
            BeginMeasurement(state);
            GetIO().SetDeltaTime(FrameTime);
            for (auto _ : state)
            {
                const size_t allocationsBefore = GetAllocationCount();
                const Clock::time_point start = Clock::now();
                NewFrame();
                build();
                EndFrame();
                const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
                m_Allocations += GetAllocationCount() - allocationsBefore;
                state.SetIterationTime(seconds);
                AddSample(seconds);
                // Hands the glyph-atlas changes to the recording backend. Not part of the frame's time.
                RenderDrawData();
            }
            EndMeasurement(state);
        }

    private:
        using Clock = std::chrono::steady_clock;

        void BeginMeasurement(benchmark::State& state);
        void EndMeasurement(benchmark::State& state);
        void AddSample(double seconds)
        {
            if (m_Samples.size() < m_Samples.capacity())
                m_Samples.push_back(seconds);
        }

    private:
        FrameBenchmarkOptions m_Options;
        Context* m_Context = nullptr;
        Context* m_PreviousContext = nullptr;
        /// Frame times of the measurement; the capacity is reserved up front so that a sample never allocates.
        std::vector<double> m_Samples;
        size_t m_Allocations = 0;
    };
} // namespace Carbon::Benchmarks
