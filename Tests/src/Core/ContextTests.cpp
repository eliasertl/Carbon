#include "Support/ContextTest.h"

namespace Carbon
{
    using ContextTests = ContextTest;

    TEST_F(ContextTests, FirstContextBecomesCurrent)
    {
        // The fixture's context is current; a second one does not steal that.
        ContextDescription description;
        Context* second = CreateContext(description);
        EXPECT_EQ(GetCurrentContext(), m_Context);

        SetCurrentContext(second);
        EXPECT_EQ(GetCurrentContext(), second);
        DestroyContext(second);
        EXPECT_EQ(GetCurrentContext(), nullptr);

        SetCurrentContext(m_Context);
    }

    TEST_F(ContextTests, ContextsKeepSeparateState)
    {
        ContextDescription description;
        Context* second = CreateContext(description);

        GetIO().SetDisplaySize(800.0f, 600.0f);
        SetCurrentContext(second);
        GetIO().SetDisplaySize(320.0f, 240.0f);
        EXPECT_EQ(GetIO().GetDisplaySize(), Vec2(320.0f, 240.0f));

        SetCurrentContext(m_Context);
        EXPECT_EQ(GetIO().GetDisplaySize(), Vec2(800.0f, 600.0f));
        DestroyContext(second);
    }

    TEST_F(ContextTests, FramesAdvanceCountAndTime)
    {
        EXPECT_EQ(GetFrameCount(), 0u);
        RunFrame(0.5f);
        RunFrame(0.25f);
        EXPECT_EQ(GetFrameCount(), 2u);
        EXPECT_DOUBLE_EQ(GetTime(), 0.75);
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(ContextTests, DrawDataCarriesDisplayMetrics)
    {
        GetIO().SetDisplaySize(640.0f, 480.0f);
        GetIO().SetContentScale(1.5f);
        RunFrame();
        const DrawData& drawData = GetDrawData();
        EXPECT_EQ(drawData.DisplaySize, Vec2(640.0f, 480.0f));
        EXPECT_FLOAT_EQ(drawData.ContentScale, 1.5f);
        EXPECT_TRUE(drawData.Commands.empty());
    }

    TEST_F(ContextTests, LifecycleMisuseIsReported)
    {
        EndFrame();
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("EndFrame called without NewFrame"), std::string::npos);

        m_AssertMessages.clear();
        NewFrame();
        NewFrame();
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("NewFrame called twice"), std::string::npos);
        EndFrame();
    }

    TEST_F(ContextTests, InvalidIOValuesAreReportedAndSanitized)
    {
        GetIO().SetContentScale(0.0f);
        EXPECT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_FLOAT_EQ(GetIO().GetContentScale(), 1.0f);

        GetIO().SetDeltaTime(-1.0f);
        EXPECT_EQ(m_AssertMessages.size(), 2u);
        EXPECT_FLOAT_EQ(GetIO().GetDeltaTime(), 0.0f);
    }

    TEST_F(ContextTests, LogsReachTheCallbackWithLevelAndSource)
    {
        m_Logs.clear();
        EXPECT_TRUE(IsLogEnabled());
        CB_LOG_WARNING("Tests", "value {} of {}", 3, 7);
        ASSERT_EQ(m_Logs.size(), 1u);
        EXPECT_EQ(m_Logs[0].Level, LogLevel::Warning);
        EXPECT_EQ(m_Logs[0].Source, "Tests");
        EXPECT_EQ(m_Logs[0].Message, "value 3 of 7");
        EXPECT_EQ(ToString(LogLevel::Warning), "Warning");
    }

    TEST_F(ContextTests, LogsAreDroppedWithoutCallback)
    {
        ContextDescription description;
        Context* silent = CreateContext(description);
        SetCurrentContext(silent);
        EXPECT_FALSE(IsLogEnabled());
        CB_LOG_ERROR("Tests", "nobody listens");
        SetCurrentContext(m_Context);
        DestroyContext(silent);

        SetCurrentContext(nullptr);
        EXPECT_FALSE(IsLogEnabled());
        Log(LogLevel::Error, "Tests", "no context at all");
        SetCurrentContext(m_Context);
    }

    TEST_F(ContextTests, FailedVerifyIsLoggedAndForwarded)
    {
        m_Logs.clear();
        int answer = 41; // not const: a constant condition would be folded at compile time
        CB_VERIFY(answer == 42, "answer was {}", answer);

        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_EQ(m_AssertMessages[0], "answer was 41");
        ASSERT_EQ(m_Logs.size(), 1u);
        EXPECT_EQ(m_Logs[0].Level, LogLevel::Fatal);
        EXPECT_EQ(m_Logs[0].Source, "Assert");
        EXPECT_NE(m_Logs[0].Message.find("answer == 42"), std::string::npos);
        EXPECT_NE(m_Logs[0].Message.find("answer was 41"), std::string::npos);

        // A passing check does nothing and does not evaluate its message.
        m_AssertMessages.clear();
        CB_VERIFY(answer == 41, "unused");
        EXPECT_TRUE(m_AssertMessages.empty());
    }
} // namespace Carbon
