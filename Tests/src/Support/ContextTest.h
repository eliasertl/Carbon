#pragma once

#include <gtest/gtest.h>

#include <functional>
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

        /// Runs one frame whose interface is built by `build`.
        void Frame(const std::function<void()>& build, float deltaTime = FrameTime)
        {
            GetIO().SetDeltaTime(deltaTime);
            NewFrame();
            build();
            EndFrame();
        }

        /// The messages of the warnings the layout logged, such as call sites that begin several containers.
        std::vector<std::string> GetLayoutWarnings() const
        {
            std::vector<std::string> warnings;
            for (const LogEntry& entry : m_Logs)
            {
                if (entry.Level == LogLevel::Warning && entry.Source == "Layout")
                    warnings.push_back(entry.Message);
            }
            return warnings;
        }

        /// Runs `build` for enough frames that layout measurements and appear fades have settled.
        void Settle(const std::function<void()>& build, int frames = 20)
        {
            for (int i = 0; i < frames; i++)
                Frame(build);
        }

    protected:
        static constexpr float FrameTime = 1.0f / 60.0f;

        Context* m_Context = nullptr;
        std::vector<LogEntry> m_Logs;
        std::vector<std::string> m_AssertMessages;
    };
} // namespace Carbon
