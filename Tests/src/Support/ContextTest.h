#pragma once

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Carbon/Carbon.h"

namespace Carbon
{
    /// Test fixture with a headless context (no GPU device). Log messages and failed asserts are recorded instead
    /// of breaking into the debugger, so tests can assert on them.
    class ContextTest : public ::testing::Test
    {
    protected:
        struct LogEntry
        {
            LogLevel Level;
            std::string Source;
            std::string Message;
        };

        void SetUp() override
        {
            ContextDescription description;
            description.Callbacks.Log = [this](LogLevel level, std::string_view source, std::string_view message)
            { m_Logs.push_back(LogEntry{level, std::string(source), std::string(message)}); };
            description.Callbacks.AssertFailed = [this](const AssertInfo& info)
            { m_AssertMessages.emplace_back(info.Message); };

            m_Context = CreateContext(description);
            SetCurrentContext(m_Context);
            GetIO().SetDisplaySize(800.0f, 600.0f);
            GetIO().SetDeltaTime(FrameTime);
        }

        void TearDown() override
        {
            DestroyContext(m_Context);
            m_Context = nullptr;
        }

        /// Runs one empty frame, which applies queued input.
        void RunFrame(float deltaTime = FrameTime)
        {
            GetIO().SetDeltaTime(deltaTime);
            NewFrame();
            EndFrame();
        }

    protected:
        static constexpr float FrameTime = 1.0f / 60.0f;

        Context* m_Context = nullptr;
        std::vector<LogEntry> m_Logs;
        std::vector<std::string> m_AssertMessages;
    };
} // namespace Carbon
