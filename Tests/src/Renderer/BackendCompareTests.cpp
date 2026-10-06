#include <gtest/gtest.h>

#include <format>
#include <map>
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

        // The reference rendering of a case, made once per process: every backend is compared with the same image.
        const RenderedImage& GetReferenceImage(BackendHarness& reference, const SceneCase& sceneCase,
                                               std::vector<std::string>& problems)
        {
            static std::map<std::string, RenderedImage> s_Images;
            const std::string name = sceneCase.GetName();
            auto found = s_Images.find(name);
            if (found == s_Images.end())
            {
                found = s_Images.emplace(name, RenderTestScene(reference, sceneCase.IsDark, sceneCase.Scale, problems))
                            .first;
            }
            return found->second;
        }
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
        std::string reason;
        BackendHarness* reference = GetSharedBackendHarness(std::string(ReferenceBackend), reason);
        if (reference == nullptr)
            GTEST_SKIP() << "No reference: " << reason;
        BackendHarness* harness = GetSharedBackendHarness(GetParam(), reason);
        if (harness == nullptr)
            GTEST_SKIP() << reason;

        for (const SceneCase& sceneCase : Cases)
        {
            std::vector<std::string> problems;
            const RenderedImage& expected = GetReferenceImage(*reference, sceneCase, problems);
            const RenderedImage actual = RenderTestScene(*harness, sceneCase.IsDark, sceneCase.Scale, problems);
            for (const std::string& problem : problems)
                ADD_FAILURE() << sceneCase.GetName() << ": " << problem;

            const ImageDifference difference = CompareImages(actual, expected, ChannelTolerance);
            const bool matches =
                difference.SizesMatch && difference.GetMismatchPercentage() <= AllowedMismatchPercentage;
            EXPECT_TRUE(matches) << sceneCase.GetName() << ": " << difference.MismatchedPixels << " of "
                                 << difference.TotalPixels << " pixels differ (" << difference.GetMismatchPercentage()
                                 << " %), largest channel difference " << difference.LargestDifference;
            // Images are written for failures, and for every case when CARBON_TESTS_WRITE_IMAGES is set, to look
            // at the backends side by side.
            if (!matches || ShouldWriteAllImages())
            {
                const std::filesystem::path directory = GetTestOutputDirectory("BackendCompare");
                const std::string prefix = std::format("{}-{}-", sceneCase.GetName(), GetParam());
                WritePng(directory / (prefix + "actual.png"), actual);
                WritePng(directory / (prefix + "reference.png"), expected);
                WritePng(directory / (prefix + "diff.png"), MakeDifferenceImage(actual, expected, ChannelTolerance));
                if (!matches)
                    ADD_FAILURE() << "Images written to " << directory.string();
            }
            RecordProperty(sceneCase.GetName() + "-LargestDifference", static_cast<int>(difference.LargestDifference));
            RecordProperty(sceneCase.GetName() + "-Mismatched", static_cast<int>(difference.MismatchedPixels));
        }
    }

    CB_INSTANTIATE_BACKEND_TESTS(BackendCompareTests);
} // namespace Carbon
