#include "Support/BackendTest.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace Carbon
{
    void BackendTest::SetUp()
    {
        m_Harness = CreateBackendHarness(GetParam());
        ASSERT_NE(m_Harness, nullptr) << "No harness for backend " << GetParam();
        const std::string reason = m_Harness->CreateDevice();
        if (!reason.empty())
            GTEST_SKIP() << reason;

        ContextDescription description;
        description.Callbacks.Log = [this](LogLevel level, std::string_view source, std::string_view message)
        {
            if (level >= LogLevel::Warning)
                m_Problems.push_back(std::format("[{}] {}: {}", ToString(level), source, message));
        };
        description.Callbacks.AssertFailed = [this](const AssertInfo& info)
        { m_Problems.push_back(std::format("[Check] {}", info.Message)); };
        m_Context = CreateContext(description);
        SetCurrentContext(m_Context);
        ASSERT_TRUE(m_Harness->InitBackend(TextureFormat::RGBA8Unorm)) << "Could not initialize " << GetParam();
    }

    void BackendTest::TearDown()
    {
        if (m_Context != nullptr)
        {
            DestroyContext(m_Context);
            m_Context = nullptr;
        }
        if (m_Harness != nullptr)
        {
            for (std::string& message : m_Harness->TakeMessages())
                m_Problems.push_back(std::format("[{}] {}", m_Harness->GetName(), message));
            m_Harness.reset();
        }
        std::string problems;
        for (const std::string& problem : m_Problems)
            problems += problem + "\n";
        EXPECT_TRUE(m_Problems.empty()) << problems;
    }

    RenderedImage BackendTest::RenderFrame(float width, float height, float contentScale, Color background,
                                           const std::function<void(DrawList&)>& build)
    {
        IO& io = GetIO();
        io.SetDisplaySize(width, height);
        io.SetContentScale(contentScale);
        io.SetDeltaTime(1.0f / 60.0f);
        NewFrame();
        build(GetDrawList());
        EndFrame();
        return m_Harness->RenderFrame(static_cast<uint32_t>(std::lround(width * contentScale)),
                                      static_cast<uint32_t>(std::lround(height * contentScale)), background);
    }

    void BackendTest::ExpectProblem(std::string_view text)
    {
        const auto found = std::find_if(m_Problems.begin(), m_Problems.end(), [&](const std::string& problem)
                                        { return problem.find(text) != std::string::npos; });
        EXPECT_NE(found, m_Problems.end()) << "Expected a message containing '" << text << "'";
        if (found != m_Problems.end())
            m_Problems.erase(found);
    }

    std::string GetBackendTestName(const ::testing::TestParamInfo<std::string>& info)
    {
        return info.param;
    }
} // namespace Carbon
