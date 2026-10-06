#include <cmath>

#include "Support/BackendTest.h"
#include "Support/TestScene.h"

namespace Carbon
{
    namespace
    {
        // Counts the pixels that differ from the background by more than a little: what was drawn.
        size_t CountInk(const RenderedImage& image, Color background)
        {
            size_t ink = 0;
            for (uint32_t y = 0; y < image.Height; y++)
            {
                for (uint32_t x = 0; x < image.Width; x++)
                {
                    const Color pixel = image.GetPixel(x, y);
                    if (std::abs(pixel.R - background.R) + std::abs(pixel.G - background.G) +
                            std::abs(pixel.B - background.B) >
                        0.1f)
                        ink++;
                }
            }
            return ink;
        }
    } // namespace

    using BackendSmokeTests = BackendTest;

    TEST_P(BackendSmokeTests, RendersFramesShutsDownAndStartsAgain)
    {
        const size_t texture =
            m_Harness->CreateTexture(TestScene::ImageSize, TestScene::ImageSize, TestScene::GetImageTexels());
        TestScene scene;
        IO& io = GetIO();
        io.SetDisplaySize(TestScene::Width, TestScene::Height);

        // Frames at two content scales, so the atlas is rebuilt in between.
        for (const float scale : {1.0f, 2.0f})
        {
            io.SetContentScale(scale);
            for (int frame = 0; frame < 4; frame++)
            {
                io.SetDeltaTime(0.1f);
                NewFrame();
                scene.Build(m_Harness->GetTextureID(texture));
                EndFrame();
                const RenderedImage image =
                    m_Harness->RenderFrame(static_cast<uint32_t>(TestScene::Width * scale),
                                           static_cast<uint32_t>(TestScene::Height * scale), Color::White());
                ASSERT_EQ(image.Width, static_cast<uint32_t>(TestScene::Width * scale));
                if (frame == 3)
                    EXPECT_GT(CountInk(image, Color::White()), static_cast<size_t>(2000 * scale * scale));
            }
        }

        // Switching backends is Shutdown, then Init; here the same one comes back.
        m_Harness->ShutdownBackend();
        EXPECT_EQ(GetRendererBackend(), nullptr);
        ASSERT_TRUE(m_Harness->InitBackend(TextureFormat::RGBA8Unorm));
        EXPECT_NE(GetRendererBackend(), nullptr);
        for (int frame = 0; frame < 3; frame++)
        {
            NewFrame();
            scene.Build(m_Harness->GetTextureID(texture));
            EndFrame();
            const RenderedImage image =
                m_Harness->RenderFrame(static_cast<uint32_t>(TestScene::Width * 2.0f),
                                       static_cast<uint32_t>(TestScene::Height * 2.0f), Color::White());
            if (frame == 2)
                EXPECT_GT(CountInk(image, Color::White()), 8000u);
        }
        m_Harness->ShutdownBackend();
    }

    TEST_P(BackendSmokeTests, SrgbTargetsAreLinearized)
    {
        // A mid gray written as sRGB into an sRGB target reads back as the same sRGB value.
        m_Harness->ShutdownBackend();
        ASSERT_TRUE(m_Harness->InitBackend(TextureFormat::RGBA8UnormSrgb));
        const Color gray = Color::FromHex(0x808080);
        IO& io = GetIO();
        io.SetDisplaySize(32.0f, 32.0f);
        io.SetContentScale(1.0f);
        NewFrame();
        GetDrawList().AddRect(Rect(0.0f, 0.0f, 32.0f, 32.0f), gray);
        EndFrame();
        const RenderedImage image = m_Harness->RenderFrame(32, 32, Color::Black());
        EXPECT_NEAR(image.GetPixel(16, 16).R, gray.R, 2.5f / 255.0f);
        EXPECT_NEAR(image.GetPixel(16, 16).G, gray.G, 2.5f / 255.0f);
    }

    TEST_P(BackendSmokeTests, TexturesAreReleasedAfterAFrameWithoutThem)
    {
        const uint8_t white[4] = {255, 255, 255, 255};
        const size_t texture = m_Harness->CreateTexture(1, 1, white);
        IO& io = GetIO();
        io.SetDisplaySize(16.0f, 16.0f);
        io.SetDeltaTime(1.0f / 60.0f);

        const TextureID id = m_Harness->GetTextureID(texture);
        for (int frame = 0; frame < 4; frame++)
        {
            NewFrame();
            if (frame < 2)
                GetDrawList().AddImage(id, Rect(0.0f, 0.0f, 16.0f, 16.0f));
            else
                GetDrawList().AddRect(Rect(0.0f, 0.0f, 8.0f, 8.0f), Color::Black());
            EndFrame();
            m_Harness->RenderFrame(16, 16, Color::Black());
        }

        // The ID was released; drawing it now is skipped with a warning, not a crash.
        NewFrame();
        GetDrawList().AddImage(id, Rect(0.0f, 0.0f, 16.0f, 16.0f));
        EndFrame();
        const RenderedImage image = m_Harness->RenderFrame(16, 16, Color::Black());
        EXPECT_NEAR(image.GetPixel(8, 8).R, 0.0f, 0.01f);
        ExpectProblem("unknown or released texture");
    }

    CB_INSTANTIATE_BACKEND_TESTS(BackendSmokeTests);
} // namespace Carbon
