#include "Carbon/Core/Log.h"

#include "Carbon/Core/ContextInternal.h"

namespace Carbon
{
    std::string_view ToString(LogLevel level)
    {
        switch (level)
        {
            case LogLevel::Trace:
                return "Trace";
            case LogLevel::Debug:
                return "Debug";
            case LogLevel::Info:
                return "Info";
            case LogLevel::Warning:
                return "Warning";
            case LogLevel::Error:
                return "Error";
            case LogLevel::Fatal:
                return "Fatal";
        }
        return "Unknown";
    }

    bool IsLogEnabled()
    {
        const Context* context = Internal::g_CurrentContext;
        return context != nullptr && static_cast<bool>(context->HostCallbacks.Log);
    }

    void Log(LogLevel level, std::string_view source, std::string_view message)
    {
        const Context* context = Internal::g_CurrentContext;
        if (context == nullptr || !context->HostCallbacks.Log)
            return;
        context->HostCallbacks.Log(level, source, message);
    }
} // namespace Carbon
