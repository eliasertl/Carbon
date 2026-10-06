#include "Support/FrameBenchmark.h"

#include <algorithm>
#include <cstdio>
#include <memory>

namespace Carbon::Benchmarks
{
    namespace
    {
        // The most frame times a measurement keeps for its percentiles.
        constexpr size_t MaxSamples = size_t(1) << 20;

        // A renderer backend that draws nothing and counts what Carbon hands it.
        class RecordingBackend final : public RendererBackend
        {
        public:
            std::string_view GetName() const override { return "Benchmark"; }

            void UpdateGlyphAtlas(const GlyphAtlasUpdate& update) override
            {
                m_AtlasBytes += static_cast<uint64_t>(update.Width) * update.RowCount;
                if (update.IsFull)
                    m_FullAtlasUpdates++;
            }

            void Render(const DrawData&) override {}
            void ReleaseTexture(TextureID) override {}

            uint64_t GetAtlasBytes() const { return m_AtlasBytes; }
            uint64_t GetFullAtlasUpdates() const { return m_FullAtlasUpdates; }
            void Reset()
            {
                m_AtlasBytes = 0;
                m_FullAtlasUpdates = 0;
            }

        private:
            uint64_t m_AtlasBytes = 0;
            uint64_t m_FullAtlasUpdates = 0;
        };

        double GetPercentile(const std::vector<double>& sorted, double fraction)
        {
            if (sorted.empty())
                return 0.0;
            const size_t index = static_cast<size_t>(fraction * static_cast<double>(sorted.size() - 1) + 0.5);
            return sorted[std::min(index, sorted.size() - 1)];
        }
    } // namespace

    FrameBenchmark::FrameBenchmark(const FrameBenchmarkOptions& options) : m_Options(options)
    {
        ContextDescription description;
        // A benchmark that provokes warnings or failed checks measures the wrong thing: say so.
        description.Callbacks.Log =
            [printsWarnings = options.PrintsWarnings](LogLevel level, std::string_view source, std::string_view message)
        {
            if (printsWarnings && level >= LogLevel::Warning)
            {
                std::fprintf(stderr, "[Carbon] %.*s: %.*s\n", static_cast<int>(source.size()), source.data(),
                             static_cast<int>(message.size()), message.data());
            }
        };
        description.Callbacks.AssertFailed = [](const AssertInfo&) {};

        m_PreviousContext = GetCurrentContext();
        m_Context = CreateContext(description);
        SetCurrentContext(m_Context);
        InstallRendererBackend(std::make_unique<RecordingBackend>());

        IO& io = GetIO();
        io.SetDisplaySize(m_Options.DisplaySize.X, m_Options.DisplaySize.Y);
        io.SetContentScale(m_Options.ContentScale);
        io.SetDeltaTime(FrameTime);
    }

    FrameBenchmark::~FrameBenchmark()
    {
        DestroyContext(m_Context);
        SetCurrentContext(m_PreviousContext);
    }

    void FrameBenchmark::BeginMeasurement(benchmark::State& state)
    {
        m_Samples.clear();
        m_Samples.reserve(static_cast<size_t>(std::min<uint64_t>(state.max_iterations, MaxSamples)));
        m_Allocations = 0;
        if (RecordingBackend* backend = GetRendererBackend<RecordingBackend>())
            backend->Reset();
    }

    void FrameBenchmark::EndMeasurement(benchmark::State& state)
    {
        const double frames = static_cast<double>(std::max<int64_t>(state.iterations(), 1));
        std::sort(m_Samples.begin(), m_Samples.end());
        state.counters["p50_us"] = GetPercentile(m_Samples, 0.5) * 1.0e6;
        state.counters["p95_us"] = GetPercentile(m_Samples, 0.95) * 1.0e6;
        state.counters["allocs"] = static_cast<double>(m_Allocations) / frames;

        const DrawData& drawData = GetDrawData();
        state.counters["vertices"] = static_cast<double>(drawData.Vertices.size());
        state.counters["indices"] = static_cast<double>(drawData.Indices.size());
        state.counters["primitives"] = static_cast<double>(drawData.Primitives.size());
        state.counters["commands"] = static_cast<double>(drawData.Commands.size());
        state.counters["draw_bytes"] =
            static_cast<double>(drawData.Vertices.size_bytes() + drawData.Indices.size_bytes() +
                                drawData.Primitives.size_bytes() + drawData.Commands.size_bytes());

        if (const RecordingBackend* backend = GetRendererBackend<RecordingBackend>())
        {
            state.counters["atlas_bytes"] = static_cast<double>(backend->GetAtlasBytes()) / frames;
            state.counters["atlas_full_per_1k"] = static_cast<double>(backend->GetFullAtlasUpdates()) * 1000.0 / frames;
        }
    }
} // namespace Carbon::Benchmarks
