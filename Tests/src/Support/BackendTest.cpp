#include "Support/BackendTest.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <map>

namespace Carbon
{
    namespace
    {
        struct SharedHarness
        {
            std::unique_ptr<BackendHarness> Harness;
            /// Why the device could not be created; empty when it was.
            std::string Reason;
        };

        std::map<std::string, SharedHarness>& GetSharedHarnesses()
        {
            static std::map<std::string, SharedHarness> s_Harnesses;
            return s_Harnesses;
        }

        // Destroys the devices after the last test, while the libraries they come from are still in working
        // order, instead of from a static destructor.
        class SharedHarnessEnvironment : public ::testing::Environment
        {
        public:
            void TearDown() override { GetSharedHarnesses().clear(); }
        };

        // GoogleTest owns the environment.
        const ::testing::Environment* const s_SharedHarnessEnvironment =
            ::testing::AddGlobalTestEnvironment(new SharedHarnessEnvironment());
    } // namespace

    BackendHarness* GetSharedBackendHarness(const std::string& name, std::string& reason)
    {
        std::map<std::string, SharedHarness>& harnesses = GetSharedHarnesses();
        auto found = harnesses.find(name);
        if (found == harnesses.end())
        {
            SharedHarness shared;
            shared.Harness = CreateBackendHarness(name);
            shared.Reason = shared.Harness != nullptr ? shared.Harness->CreateDevice()
                                                      : std::format("No harness for backend {}", name);
            found = harnesses.emplace(name, std::move(shared)).first;
        }
        reason = found->second.Reason;
        return reason.empty() ? found->second.Harness.get() : nullptr;
    }

    void BackendTest::SetUp()
    {
        std::string reason;
        m_Harness = GetSharedBackendHarness(GetParam(), reason);
        if (m_Harness == nullptr)
            GTEST_SKIP() << reason;
        // Messages of the API that came after the last test took them belong to nobody.
        m_Harness->TakeMessages();

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
            m_Harness = nullptr;
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
