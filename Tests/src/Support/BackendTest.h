#pragma once

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Carbon/Carbon.h"
#include "Support/BackendHarness.h"

namespace Carbon
{
    /// Test fixture that runs once for every renderer backend compiled into Carbon. Each test gets a context with
    /// the backend installed on a fresh headless device of its API.
    ///
    /// A test is skipped when the machine cannot create a device for the backend (no GPU, no driver, no software
    /// rasterizer), so the suite stays green everywhere. Any warning or error Carbon logs, any failed check and
    /// any message from the API's validation fails the test.
    class BackendTest : public ::testing::TestWithParam<std::string>
    {
    protected:
        void SetUp() override;
        void TearDown() override;

        /// Runs one frame: `build` draws into the frame's draw list, then the frame is rendered into a target of
        /// `width` x `height` points at `contentScale`, cleared to `background`.
        RenderedImage RenderFrame(float width, float height, float contentScale, Color background,
                                  const std::function<void(DrawList&)>& build);

        /// Lets a test accept a message it provokes on purpose.
        void ExpectProblem(std::string_view text);

    protected:
        std::unique_ptr<BackendHarness> m_Harness;
        Context* m_Context = nullptr;
        /// Warnings and errors logged by Carbon and failed checks, in order.
        std::vector<std::string> m_Problems;
    };

    /// Names parameterized tests after the backend ("RendererTests/WebGPU.Name").
    std::string GetBackendTestName(const ::testing::TestParamInfo<std::string>& info);

    /// Instantiates a BackendTest-derived suite for every compiled-in backend.
#define CB_INSTANTIATE_BACKEND_TESTS(suite)                                                         \
    GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(suite);                                           \
    INSTANTIATE_TEST_SUITE_P(Backends, suite, ::testing::ValuesIn(::Carbon::GetCompiledBackends()), \
                             ::Carbon::GetBackendTestName)
} // namespace Carbon
