// Backends: the CPU time of each compiled-in backend's render function (WebGPURender, VulkanRender, ...), drawing
// offscreen through the harnesses of the renderer tests. Creating the target and reading it back are not measured.
// A backend whose device cannot be created on this machine is skipped with a message.

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "Support/BackendHarness.h"
#include "Support/TestScene.h"

namespace Carbon::Benchmarks
{
    namespace
    {
        constexpr float FrameTime = 1.0f / 60.0f;
        constexpr float LargeSceneWidth = 1280.0f;
        constexpr float LargeSceneHeight = 800.0f;
        // Every iteration also creates a target and reads it back, which takes far longer than the render function
        // that is measured. A fixed number of iterations keeps a run short.
        constexpr int Iterations = 400;

        enum class Scene
        {
            /// The scene of the renderer tests: every kind of shape, about 300 quads.
            Small,
            /// A window full of text over striped rows: about 12,000 quads.
            Large
        };

        struct Case
        {
            std::string Backend;
            Scene Kind = Scene::Small;
            /// The frame is rendered again without a new frame in between, as a host does that redraws a window
            /// (after an expose event, or into a second swap chain).
            bool IsRedraw = false;
        };

        void BuildLargeScene()
        {
            DrawList& drawList = GetDrawList();
            const Color label = GetStyleColor(StyleColor::Label);
            const Color stripe = GetStyleColor(StyleColor::ControlFill);
            const TextSpec spec = GetTextSpec(TextStyle::Body);
            static const std::string s_Line =
                "The quick brown fox jumps over the lazy dog while Carbon draws squircles, text and images. "
                "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz";
            for (int row = 0; row < 50; row++)
            {
                const float y = 8.0f + 16.0f * static_cast<float>(row);
                if (row % 2 == 1)
                    drawList.AddSquircle(Rect(4.0f, y, LargeSceneWidth - 8.0f, 16.0f), stripe, 4.0f);
                drawList.AddText(Vec2(8.0f, y), s_Line, spec, label);
                drawList.AddText(Vec2(8.0f + 640.0f, y), s_Line.substr(0, 90), spec, label);
            }
        }

        void BackendRender(benchmark::State& state, const Case* benchmarkCase)
        {
            const std::unique_ptr<BackendHarness> harness = CreateBackendHarness(benchmarkCase->Backend);
            if (harness == nullptr)
            {
                state.SkipWithMessage("the backend is not compiled in");
                return;
            }
            if (const std::string reason = harness->CreateDevice(); !reason.empty())
            {
                state.SkipWithMessage(reason);
                return;
            }

            Context* previous = GetCurrentContext();
            Context* context = CreateContext({});
            SetCurrentContext(context);
            if (!harness->InitBackend(TextureFormat::RGBA8Unorm))
            {
                DestroyContext(context);
                SetCurrentContext(previous);
                state.SkipWithMessage("the backend could not be initialized");
                return;
            }

            const bool isLarge = benchmarkCase->Kind == Scene::Large;
            const float width = isLarge ? LargeSceneWidth : TestScene::Width;
            const float height = isLarge ? LargeSceneHeight : TestScene::Height;
            const size_t texture =
                harness->CreateTexture(TestScene::ImageSize, TestScene::ImageSize, TestScene::GetImageTexels());
            TestScene scene;
            IO& io = GetIO();
            io.SetDisplaySize(width, height);
            io.SetContentScale(1.0f);
            const auto runFrame = [&](float deltaTime)
            {
                io.SetDeltaTime(deltaTime);
                NewFrame();
                if (isLarge)
                    BuildLargeScene();
                else
                    scene.Build(harness->GetTextureID(texture));
                EndFrame();
            };
            const uint32_t pixelWidth = static_cast<uint32_t>(std::lround(width));
            const uint32_t pixelHeight = static_cast<uint32_t>(std::lround(height));
            const Color background = GetStyleColor(StyleColor::Background);

            // Settled frames, and a few rendered ones: pipelines, buffers and the glyph atlas exist afterwards.
            for (int i = 0; i < 10; i++)
                runFrame(0.25f);
            for (int i = 0; i < 4; i++)
            {
                runFrame(FrameTime);
                harness->RenderFrame(pixelWidth, pixelHeight, background);
            }

            for (auto _ : state)
            {
                if (!benchmarkCase->IsRedraw)
                    runFrame(FrameTime);
                harness->RenderFrame(pixelWidth, pixelHeight, background);
                state.SetIterationTime(harness->GetLastRenderSeconds());
            }

            const DrawData& drawData = GetDrawData();
            state.counters["vertices"] = static_cast<double>(drawData.Vertices.size());
            // OpenGL only: the state queries of one render call (glGet*, glIsEnabled).
            if (harness->GetLastRenderStateQueries() > 0)
                state.counters["state_queries"] = static_cast<double>(harness->GetLastRenderStateQueries());
            state.counters["commands"] = static_cast<double>(drawData.Commands.size());

            harness->ShutdownBackend();
            DestroyContext(context);
            SetCurrentContext(previous);
            for (const std::string& message : harness->TakeMessages())
                std::fprintf(stderr, "[%s] %s\n", benchmarkCase->Backend.c_str(), message.c_str());
        }

        const int s_Registered = []
        {
            // The cases outlive the registration: Google Benchmark keeps the pointers.
            static std::vector<Case> s_Cases;
            for (const std::string& backend : GetCompiledBackends())
            {
                // Variants of a backend that the tests add (objects released every frame, the render-pass mode)
                // run the same render function.
                if (backend == "DX9Invalidate" || backend == "VulkanRenderPass")
                    continue;
                for (const Scene scene : {Scene::Small, Scene::Large})
                {
                    for (const bool isRedraw : {false, true})
                        s_Cases.push_back(Case{backend, scene, isRedraw});
                }
            }
            for (const Case& benchmarkCase : s_Cases)
            {
                const std::string name = "Backends/" + benchmarkCase.Backend + "/" +
                                         (benchmarkCase.Kind == Scene::Large ? "Large" : "Small") +
                                         (benchmarkCase.IsRedraw ? "/Redraw" : "/NewFrame");
                benchmark::RegisterBenchmark(name, BackendRender, &benchmarkCase)
                    ->Iterations(Iterations)
                    ->UseManualTime()
                    ->Unit(benchmark::kMicrosecond);
            }
            return 0;
        }();
    } // namespace
} // namespace Carbon::Benchmarks
