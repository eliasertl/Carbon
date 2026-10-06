#include "Support/ContextTest.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace Carbon
{
    namespace
    {
        /// What a RecordingBackend saw. Lives in the test, so it can be read after the backend is destroyed.
        struct BackendRecord
        {
            /// Every call in order: "BeginFrame", "EndFrame", "UpdateGlyphAtlas", "Render", "ReleaseTexture".
            std::vector<std::string> Calls;
            /// The atlas updates, without their pixels.
            std::vector<GlyphAtlasUpdate> Updates;
            std::vector<size_t> UpdatePixelCounts;
            std::vector<uint64_t> BeginFrames;
            std::vector<TextureID> Released;
            int EndFrameCount = 0;
            int RenderCount = 0;
            size_t LastCommandCount = 0;
            bool IsDestroyed = false;
            Context* ContextAtDestruction = nullptr;

            size_t CountCalls(std::string_view name) const
            {
                return static_cast<size_t>(std::count(Calls.begin(), Calls.end(), name));
            }
        };

        /// A backend without a GPU that writes down what Carbon asks of it.
        class RecordingBackend : public RendererBackend
        {
        public:
            explicit RecordingBackend(BackendRecord& record, uint32_t maxTextureSize = 4096)
                : m_Record(record), m_MaxTextureSize(maxTextureSize)
            {
            }

            ~RecordingBackend() override
            {
                m_Record.IsDestroyed = true;
                m_Record.ContextAtDestruction = GetCurrentContext();
            }

            std::string_view GetName() const override { return "Recording"; }

            RendererBackendCapabilities GetCapabilities() const override
            {
                RendererBackendCapabilities capabilities;
                capabilities.MaxTextureSize = m_MaxTextureSize;
                return capabilities;
            }

            void BeginFrame(uint64_t frameCount) override
            {
                m_Record.Calls.emplace_back("BeginFrame");
                m_Record.BeginFrames.push_back(frameCount);
            }

            void EndFrame() override
            {
                m_Record.Calls.emplace_back("EndFrame");
                m_Record.EndFrameCount++;
            }

            void UpdateGlyphAtlas(const GlyphAtlasUpdate& update) override
            {
                m_Record.Calls.emplace_back("UpdateGlyphAtlas");
                m_Record.UpdatePixelCounts.push_back(update.Pixels.size());
                GlyphAtlasUpdate copy = update;
                copy.Pixels = {};
                m_Record.Updates.push_back(copy);
                if (InvalidatesDuringNextUpdate)
                {
                    InvalidatesDuringNextUpdate = false;
                    InvalidateGlyphAtlas();
                }
            }

            void Render(const DrawData& drawData) override
            {
                m_Record.Calls.emplace_back("Render");
                m_Record.RenderCount++;
                m_Record.LastCommandCount = drawData.Commands.size();
            }

            void ReleaseTexture(TextureID texture) override
            {
                m_Record.Calls.emplace_back("ReleaseTexture");
                m_Record.Released.push_back(texture);
            }

        public:
            /// Simulates a backend whose texture creation failed: it asks for the update again.
            bool InvalidatesDuringNextUpdate = false;

        private:
            BackendRecord& m_Record;
            uint32_t m_MaxTextureSize;
        };

        /// A second backend type, to tell typed lookups apart.
        class OtherBackend : public RecordingBackend
        {
        public:
            using RecordingBackend::RecordingBackend;
        };

        void DrawRect()
        {
            GetDrawList().AddRect(Rect(10.0f, 10.0f, 20.0f, 20.0f), Color::Black());
        }

        void DrawSmallText(std::string_view text)
        {
            TextSpec spec;
            spec.Size = 13.0f;
            GetDrawList().AddText(Vec2(10.0f, 10.0f), text, spec, Color::Black());
        }
    } // namespace

    class RendererBackendTests : public ContextTest
    {
    protected:
        RecordingBackend* Install(uint32_t maxTextureSize = 4096)
        {
            std::unique_ptr<RecordingBackend> backend = std::make_unique<RecordingBackend>(m_Record, maxTextureSize);
            RecordingBackend* installed = backend.get();
            EXPECT_TRUE(InstallRendererBackend(std::move(backend)));
            return installed;
        }

        size_t CountLogs(LogLevel level, std::string_view source) const
        {
            return static_cast<size_t>(std::count_if(m_Logs.begin(), m_Logs.end(), [&](const LogEntry& entry)
                                                     { return entry.Level == level && entry.Source == source; }));
        }

    protected:
        BackendRecord m_Record;
    };

    TEST_F(RendererBackendTests, HeadlessContextBuildsDrawDataAndReportsRenderingOnce)
    {
        EXPECT_EQ(GetRendererBackend(), nullptr);
        EXPECT_EQ(GetRendererBackend<RecordingBackend>(), nullptr);

        Frame(DrawRect);
        EXPECT_FALSE(GetDrawData().Commands.empty());
        RenderDrawData();
        RenderDrawData();
        EXPECT_EQ(CountLogs(LogLevel::Error, "Renderer"), 1u);

        // Everything else is harmless without a backend.
        FlushGlyphAtlas();
        InvalidateGlyphAtlas();
        RemoveRendererBackend();
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(RendererBackendTests, InstallAndRemove)
    {
        RecordingBackend* backend = Install();
        EXPECT_EQ(GetRendererBackend(), backend);
        EXPECT_EQ(GetRendererBackend<RecordingBackend>(), backend);
        EXPECT_EQ(GetRendererBackend<OtherBackend>(), nullptr);
        EXPECT_FALSE(m_Record.IsDestroyed);

        RemoveRendererBackend();
        EXPECT_TRUE(m_Record.IsDestroyed);
        EXPECT_EQ(m_Record.ContextAtDestruction, m_Context);
        EXPECT_EQ(GetRendererBackend(), nullptr);
        EXPECT_EQ(GetRendererBackend<RecordingBackend>(), nullptr);
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(RendererBackendTests, AContextHasOneBackendAtATime)
    {
        RecordingBackend* first = Install();

        BackendRecord secondRecord;
        EXPECT_FALSE(InstallRendererBackend(std::make_unique<OtherBackend>(secondRecord)));
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("already has the renderer backend 'Recording'"), std::string::npos);
        EXPECT_TRUE(secondRecord.IsDestroyed);
        EXPECT_EQ(GetRendererBackend(), first);
        EXPECT_FALSE(m_Record.IsDestroyed);

        // Switching is removing one and installing the other.
        m_AssertMessages.clear();
        RemoveRendererBackend();
        BackendRecord thirdRecord;
        EXPECT_TRUE(InstallRendererBackend(std::make_unique<OtherBackend>(thirdRecord)));
        EXPECT_NE(GetRendererBackend<OtherBackend>(), nullptr);
        EXPECT_EQ(GetRendererBackend<RecordingBackend>(), nullptr);
        EXPECT_TRUE(m_AssertMessages.empty());
        RemoveRendererBackend();
    }

    TEST_F(RendererBackendTests, InstallingNothingIsReported)
    {
        EXPECT_FALSE(InstallRendererBackend(std::unique_ptr<RecordingBackend>()));
        EXPECT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_EQ(GetRendererBackend(), nullptr);
    }

    TEST_F(RendererBackendTests, DestroyContextDestroysTheBackendWithItsContextCurrent)
    {
        ContextDescription description;
        Context* second = CreateContext(description);
        SetCurrentContext(second);
        EXPECT_TRUE(InstallRendererBackend(std::make_unique<RecordingBackend>(m_Record)));
        SetCurrentContext(m_Context);
        EXPECT_EQ(GetRendererBackend(), nullptr); // backends belong to their context

        DestroyContext(second);
        EXPECT_TRUE(m_Record.IsDestroyed);
        EXPECT_EQ(m_Record.ContextAtDestruction, second);
        EXPECT_EQ(GetCurrentContext(), m_Context);
    }

    TEST_F(RendererBackendTests, FramesAreAnnounced)
    {
        Install();
        RunFrame();
        RunFrame();
        EXPECT_EQ(m_Record.BeginFrames, (std::vector<uint64_t>{1, 2}));
        EXPECT_EQ(m_Record.EndFrameCount, 2);
        EXPECT_EQ(m_Record.Calls, (std::vector<std::string>{"BeginFrame", "EndFrame", "BeginFrame", "EndFrame"}));
    }

    TEST_F(RendererBackendTests, TheFirstRenderSendsTheWholeAtlasThenTheDrawData)
    {
        Install();
        Frame(DrawRect); // no text: the atlas is still sent, because every command samples it
        m_Record.Calls.clear();
        RenderDrawData();

        ASSERT_EQ(m_Record.Calls, (std::vector<std::string>{"UpdateGlyphAtlas", "Render"}));
        const GlyphAtlasUpdate& update = m_Record.Updates[0];
        EXPECT_TRUE(update.IsFull);
        EXPECT_GT(update.Width, 0u);
        EXPECT_GT(update.Height, 0u);
        EXPECT_GT(update.Generation, 0u);
        EXPECT_EQ(update.FirstRow, 0u);
        EXPECT_EQ(update.RowCount, update.Height);
        EXPECT_EQ(m_Record.UpdatePixelCounts[0], static_cast<size_t>(update.Width) * update.Height);
        EXPECT_EQ(m_Record.LastCommandCount, GetDrawData().Commands.size());
    }

    TEST_F(RendererBackendTests, NewGlyphsAreSentAsChangedRows)
    {
        Install();
        Frame(DrawRect);
        RenderDrawData();
        ASSERT_EQ(m_Record.Updates.size(), 1u);

        Frame([] { DrawSmallText("Carbon"); });
        RenderDrawData();
        ASSERT_EQ(m_Record.Updates.size(), 2u);
        const GlyphAtlasUpdate& update = m_Record.Updates[1];
        EXPECT_FALSE(update.IsFull);
        EXPECT_EQ(update.Generation, m_Record.Updates[0].Generation);
        EXPECT_GT(update.RowCount, 0u);
        EXPECT_LT(update.RowCount, update.Height);
        EXPECT_LE(update.FirstRow + update.RowCount, update.Height);
        EXPECT_EQ(m_Record.UpdatePixelCounts[1], static_cast<size_t>(update.Width) * update.Height);

        // The same text again: nothing new to upload.
        Frame([] { DrawSmallText("Carbon"); });
        RenderDrawData();
        EXPECT_EQ(m_Record.Updates.size(), 2u);
        EXPECT_EQ(m_Record.RenderCount, 3);
    }

    TEST_F(RendererBackendTests, ChangesAccumulateWhileFramesAreNotRendered)
    {
        Install();
        Frame(DrawRect);
        RenderDrawData();

        Frame([] { DrawSmallText("abc"); });
        Frame([] { DrawSmallText("abcdef"); });
        Frame([] { DrawSmallText("abcdefghi"); });
        EXPECT_EQ(m_Record.Updates.size(), 1u);
        RenderDrawData();
        ASSERT_EQ(m_Record.Updates.size(), 2u);
        EXPECT_FALSE(m_Record.Updates[1].IsFull);
        EXPECT_GT(m_Record.Updates[1].RowCount, 0u);
    }

    TEST_F(RendererBackendTests, ANewAtlasGenerationIsSentInFull)
    {
        Install();
        Frame([] { DrawSmallText("Carbon"); });
        RenderDrawData();
        ASSERT_EQ(m_Record.Updates.size(), 1u);

        // A new content scale clears the atlas: every glyph is rasterized again.
        GetIO().SetContentScale(2.0f);
        Frame([] { DrawSmallText("Carbon"); });
        RenderDrawData();
        ASSERT_EQ(m_Record.Updates.size(), 2u);
        EXPECT_TRUE(m_Record.Updates[1].IsFull);
        EXPECT_GT(m_Record.Updates[1].Generation, m_Record.Updates[0].Generation);
        EXPECT_EQ(m_Record.Updates[1].RowCount, m_Record.Updates[1].Height);
    }

    TEST_F(RendererBackendTests, InvalidatingTheAtlasSendsItInFullAgain)
    {
        RecordingBackend* backend = Install();
        Frame(DrawRect);
        RenderDrawData();
        RenderDrawData();
        ASSERT_EQ(m_Record.Updates.size(), 1u);

        InvalidateGlyphAtlas();
        RenderDrawData();
        ASSERT_EQ(m_Record.Updates.size(), 2u);
        EXPECT_TRUE(m_Record.Updates[1].IsFull);

        // A backend may also ask from inside the update, when it could not take it.
        backend->InvalidatesDuringNextUpdate = true;
        InvalidateGlyphAtlas();
        RenderDrawData();
        ASSERT_EQ(m_Record.Updates.size(), 3u);
        RenderDrawData();
        ASSERT_EQ(m_Record.Updates.size(), 4u);
        EXPECT_TRUE(m_Record.Updates[3].IsFull);
        RenderDrawData();
        EXPECT_EQ(m_Record.Updates.size(), 4u);
    }

    TEST_F(RendererBackendTests, FlushSendsTheAtlasWithoutDrawing)
    {
        Install();
        Frame([] { DrawSmallText("Carbon"); });
        FlushGlyphAtlas();
        EXPECT_EQ(m_Record.Updates.size(), 1u);
        EXPECT_EQ(m_Record.RenderCount, 0);

        RenderDrawData();
        EXPECT_EQ(m_Record.Updates.size(), 1u);
        EXPECT_EQ(m_Record.RenderCount, 1);
    }

    TEST_F(RendererBackendTests, AFrameCanBeRenderedMoreThanOnce)
    {
        Install();
        Frame([] { DrawSmallText("Carbon"); });
        RenderDrawData();
        RenderDrawData();
        EXPECT_EQ(m_Record.RenderCount, 2);
        EXPECT_EQ(m_Record.Updates.size(), 1u);
    }

    TEST_F(RendererBackendTests, AnEmptyFrameIsNotRendered)
    {
        Install();
        RunFrame();
        m_Record.Calls.clear();
        RenderDrawData();
        EXPECT_TRUE(m_Record.Calls.empty());

        GetIO().SetDisplaySize(0.0f, 0.0f);
        Frame(DrawRect);
        RenderDrawData();
        EXPECT_EQ(m_Record.RenderCount, 0);
    }

    TEST_F(RendererBackendTests, RenderingInsideAFrameIsReported)
    {
        Install();
        NewFrame();
        DrawRect();
        RenderDrawData();
        EndFrame();
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("after EndFrame"), std::string::npos);
        EXPECT_EQ(m_Record.RenderCount, 0);
    }

    TEST_F(RendererBackendTests, TheBackendsTextureLimitCapsTheAtlas)
    {
        Install(1024);
        const auto drawLargeText = []
        {
            TextSpec spec;
            spec.Size = 300.0f;
            GetDrawList().AddText(Vec2(0.0f, 0.0f), "ABCDEFGHIJKLMNOPQRSTUVWXYZ", spec, Color::Black());
            GetDrawList().AddText(Vec2(0.0f, 0.0f), "abcdefghijklmnopqrstuvwxyz0123456789", spec, Color::Black());
        };
        Frame(drawLargeText);
        RenderDrawData();
        Frame(drawLargeText);
        RenderDrawData();

        ASSERT_FALSE(m_Record.Updates.empty());
        uint32_t largest = 0;
        for (const GlyphAtlasUpdate& update : m_Record.Updates)
            largest = std::max({largest, update.Width, update.Height});
        EXPECT_EQ(largest, 1024u);
        // The glyphs do not all fit into 1024 x 1024: the atlas reports that it is full instead of growing.
        EXPECT_GE(CountLogs(LogLevel::Warning, "Text"), 1u);
    }

    TEST_F(RendererBackendTests, TextureKeysBecomeTextureIDs)
    {
        EXPECT_EQ(RegisterHostTexture(0), TextureID());
        EXPECT_EQ(RegisterHostTexture(42).Value, 42u);
        EXPECT_EQ(RegisterHostTexture(42), RegisterHostTexture(42));
        EXPECT_NE(RegisterHostTexture(42), RegisterHostTexture(43));
    }

    TEST_F(RendererBackendTests, AnUnusedTextureIsReleasedWithTheNextRender)
    {
        Install();
        Frame([] { RegisterHostTexture(42); });
        RenderDrawData();
        RunFrame(); // still kept: the frame before could be rendered now
        RenderDrawData();
        EXPECT_TRUE(m_Record.Released.empty());

        RunFrame(); // a whole frame passed without the texture
        EXPECT_TRUE(m_Record.Released.empty()) << "NewFrame must not make the backend touch the GPU";
        RenderDrawData();
        EXPECT_EQ(m_Record.Released, (std::vector<TextureID>{TextureID{42}}));

        RunFrame();
        RenderDrawData();
        EXPECT_EQ(m_Record.Released.size(), 1u);
    }

    TEST_F(RendererBackendTests, FlushAlsoDeliversReleases)
    {
        Install();
        Frame([] { RegisterHostTexture(42); });
        RunFrame();
        RunFrame();
        FlushGlyphAtlas();
        EXPECT_EQ(m_Record.Released, (std::vector<TextureID>{TextureID{42}}));
    }

    TEST_F(RendererBackendTests, ATextureLivesWhileItIsRegisteredOrDrawn)
    {
        Install();
        TextureID texture;
        Frame([&] { texture = RegisterHostTexture(42); });

        // Drawn in every frame, never registered again, and never rendered (a minimized window).
        for (int i = 0; i < 5; i++)
            Frame([&] { GetDrawList().AddImage(texture, Rect(0.0f, 0.0f, 32.0f, 32.0f)); });
        RenderDrawData();
        EXPECT_TRUE(m_Record.Released.empty());

        // Registered in every frame without being drawn.
        for (int i = 0; i < 5; i++)
            Frame([] { RegisterHostTexture(42); });
        RenderDrawData();
        EXPECT_TRUE(m_Record.Released.empty());

        RunFrame();
        RunFrame();
        RenderDrawData();
        EXPECT_EQ(m_Record.Released.size(), 1u);
    }

    TEST_F(RendererBackendTests, RegisteringAgainBeforeTheReleaseIsDeliveredCancelsIt)
    {
        Install();
        Frame([] { RegisterHostTexture(42); });
        RunFrame();
        Frame([] { RegisterHostTexture(42); }); // expired in this NewFrame, registered again right away
        RenderDrawData();
        EXPECT_TRUE(m_Record.Released.empty());
    }

    TEST_F(RendererBackendTests, ReleaseHostTextureReleasesAtOnce)
    {
        Install();
        Frame([] { RegisterHostTexture(42); });
        ReleaseHostTexture(42);
        EXPECT_EQ(m_Record.Released, (std::vector<TextureID>{TextureID{42}}));

        // Unknown keys, and a second release, are ignored.
        ReleaseHostTexture(42);
        ReleaseHostTexture(7);
        EXPECT_EQ(m_Record.Released.size(), 1u);

        RunFrame();
        RunFrame();
        RenderDrawData();
        EXPECT_EQ(m_Record.Released.size(), 1u);
    }

    TEST_F(RendererBackendTests, ANewBackendStartsFromScratch)
    {
        Install();
        Frame(
            []
            {
                RegisterHostTexture(42);
                DrawSmallText("Carbon");
            });
        RenderDrawData();
        RemoveRendererBackend();

        BackendRecord record;
        EXPECT_TRUE(InstallRendererBackend(std::make_unique<RecordingBackend>(record)));
        Frame([] { DrawSmallText("Carbon"); });
        RenderDrawData();
        ASSERT_EQ(record.Updates.size(), 1u);
        EXPECT_TRUE(record.Updates[0].IsFull);

        // The first backend's textures are not the second one's business.
        RunFrame();
        RunFrame();
        RenderDrawData();
        EXPECT_TRUE(record.Released.empty());
        RemoveRendererBackend();
    }

    TEST_F(RendererBackendTests, TheContractHasAVersion)
    {
        // A backend outside the repository pins the version like this.
        static_assert(RendererBackendVersion == 1);
        SUCCEED();
    }
} // namespace Carbon
