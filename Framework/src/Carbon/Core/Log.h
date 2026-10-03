#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace Carbon
{
    /// Severity of a log message.
    enum class LogLevel
    {
        Trace,
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    /// Returns the name of a log level, e.g. "Warning".
    std::string_view ToString(LogLevel level);

    /// Returns true when the current context has a log callback. Lets callers skip formatting work.
    bool IsLogEnabled();

    /// Sends a message to the current context's log callback. Dropped when there is no context or callback.
    /// `source` names the subsystem, e.g. "Renderer".
    void Log(LogLevel level, std::string_view source, std::string_view message);

    /// Formats with std::format and logs. Does no formatting when logging is disabled.
    template <typename... Args>
    void LogFormat(LogLevel level, std::string_view source, std::format_string<Args...> format, Args&&... args)
    {
        if (!IsLogEnabled())
            return;
        Log(level, source, std::format(format, std::forward<Args>(args)...));
    }
} // namespace Carbon

#define CB_LOG_TRACE(source, ...) ::Carbon::LogFormat(::Carbon::LogLevel::Trace, source, __VA_ARGS__)
#define CB_LOG_DEBUG(source, ...) ::Carbon::LogFormat(::Carbon::LogLevel::Debug, source, __VA_ARGS__)
#define CB_LOG_INFO(source, ...) ::Carbon::LogFormat(::Carbon::LogLevel::Info, source, __VA_ARGS__)
#define CB_LOG_WARNING(source, ...) ::Carbon::LogFormat(::Carbon::LogLevel::Warning, source, __VA_ARGS__)
#define CB_LOG_ERROR(source, ...) ::Carbon::LogFormat(::Carbon::LogLevel::Error, source, __VA_ARGS__)
#define CB_LOG_FATAL(source, ...) ::Carbon::LogFormat(::Carbon::LogLevel::Fatal, source, __VA_ARGS__)
