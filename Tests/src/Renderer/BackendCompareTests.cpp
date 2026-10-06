#include <gtest/gtest.h>

#include <format>
#include <string>
#include <vector>

#include "Support/BackendHarness.h"
#include "Support/BackendTest.h"
#include "Support/ImageCompare.h"
#include "Support/TestScene.h"

namespace Carbon
{
    namespace
    {
        // Rasterizers differ in how they round coverage and filter textures. A channel may be off by this much
        // (out of 255) without counting as a different pixel...
        constexpr uint32_t ChannelTolerance = 4;
        // ...and this share of the pixels may differ by more, for the odd antialiased edge pixel.
        constexpr double AllowedMismatchPercentage = 0.2;

        // The reference every backend is compared with.
        constexpr std::string_view ReferenceBackend = "WebGPU";

        struct SceneCase
        {
            bool IsDark = false;
            float Scale = 1.0f;

            std::string GetName() const
            {
                return std::format("{}-{}x", IsDark ? "Dark" : "Light", static_cast<int>(Scale));
            }
        };

        constexpr SceneCase Cases[] = {{false, 1.0f}, {true, 1.0f}, {false, 2.0f}, {true, 2.0f}};
    } // namespace

    /// Renders the test scene with every backend and compares it with the WebGPU rendering. On failure the
    /// images are written to <build>/Tests/BackendCompare/<case>-<backend>-{actual,reference,diff}.png.
    ///
    /// WebGPU is compared with a second rendering of its own: the reference must be stable for the comparison
    /// to mean anything.
    class BackendCompareTests : public ::testing::TestWithParam<std::string>
    {
    };

    TEST_P(BackendCompareTests, TestSceneMatchesTheReference)
    {
        const std::unique_ptr<BackendHarness> reference = CreateBackendHarness(ReferenceBackend);
        if (reference == nullptr)
            GTEST_SKIP() << "The reference backend (WebGPU) is not compiled in";
        if (const std::string reason = reference->CreateDevice(); !reason.empty())
            GTEST_SKIP() << "No reference: " << reason;
        const std::unique_ptr<BackendHarness> harness = CreateBackendHarness(GetParam());
        ASSERT_NE(harness, nullptr);
        if (const std::string reason = harness->CreateDevice(); !reason.empty())
            GTEST_SKIP() << reason;

        for (const SceneCase& sceneCase : Cases)
        {
            std::vector<std::string> problems;
            const RenderedImage expected = RenderTestScene(*reference, sceneCase.IsDark, sceneCase.Scale, problems);
            const RenderedImage actual = RenderTestScene(*harness, sceneCase.IsDark, sceneCase.Scale, problems);
            for (const std::string& problem : problems)
                ADD_FAILURE() << sceneCase.GetName() << ": " << problem;

            const ImageDifference difference = CompareImages(actual, expected, ChannelTolerance);
            const bool matches =
                difference.SizesMatch && difference.GetMismatchPercentage() <= AllowedMismatchPercentage;
            EXPECT_TRUE(matches) << sceneCase.GetName() << ": " << difference.MismatchedPixels << " of "
                                 << difference.TotalPixels << " pixels differ (" << difference.GetMismatchPercentage()
                                 << " %), largest channel difference " << difference.LargestDifference;
            if (!matches)
            {
                const std::filesystem::path directory = GetTestOutputDirectory("BackendCompare");
                const std::string prefix = std::format("{}-{}-", sceneCase.GetName(), GetParam());
                WritePng(directory / (prefix + "actual.png"), actual);
                WritePng(directory / (prefix + "reference.png"), expected);
                WritePng(directory / (prefix + "diff.png"), MakeDifferenceImage(actual, expected, ChannelTolerance));
                ADD_FAILURE() << "Images written to " << directory.string();
            }
        }
    }

    CB_INSTANTIATE_BACKEND_TESTS(BackendCompareTests);
} // namespace Carbon
