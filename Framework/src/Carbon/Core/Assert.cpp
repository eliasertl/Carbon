#include "Carbon/Core/Assert.h"

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/Log.h"

namespace Carbon::Internal
{
    bool ReportAssertFailure(std::string_view condition, std::string_view message, std::string_view file, int line)
    {
        CB_LOG_FATAL("Assert", "Assertion failed: {}: {} ({}:{})", condition, message, file, line);

        const Context* context = g_CurrentContext;
        if (context != nullptr && context->HostCallbacks.AssertFailed)
        {
            AssertInfo info;
            info.Condition = condition;
            info.Message = message;
            info.File = file;
            info.Line = line;
            context->HostCallbacks.AssertFailed(info);
            return false;
        }

#if defined(CB_DEBUG)
        return true;
#else
        return false;
#endif
    }
} // namespace Carbon::Internal
