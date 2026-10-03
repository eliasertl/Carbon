#pragma once

#include <format>
#include <string_view>

#include "Carbon/Core/Platform.h"

namespace Carbon
{
    /// Describes a failed CB_ASSERT or CB_VERIFY.
    struct AssertInfo
    {
        std::string_view Condition;
        std::string_view Message;
        std::string_view File;
        int Line = 0;
    };

    namespace Internal
    {
        /// Logs the failure and notifies the host's assert callback. Returns true when the caller should break
        /// into the debugger: debug builds without an assert callback.
        bool ReportAssertFailure(std::string_view condition, std::string_view message, std::string_view file, int line);
    } // namespace Internal
} // namespace Carbon

/// Checks a condition in every build type. Use for cheap checks of API misuse; code after a failed CB_VERIFY must
/// still be safe to run. The message is a std::format string followed by its arguments and is required.
#define CB_VERIFY(condition, ...)                                                                                  \
    do                                                                                                             \
    {                                                                                                              \
        if (!(condition)) [[unlikely]]                                                                             \
        {                                                                                                          \
            if (::Carbon::Internal::ReportAssertFailure(#condition, std::format(__VA_ARGS__), __FILE__, __LINE__)) \
            {                                                                                                      \
                CB_DEBUG_BREAK();                                                                                  \
            }                                                                                                      \
        }                                                                                                          \
    } while (false)

/// Checks an internal invariant. Active in debug builds, or in every build when CB_FORCE_ASSERTS is defined.
#if defined(CB_DEBUG) || defined(CB_FORCE_ASSERTS)
#define CB_ASSERT(condition, ...) CB_VERIFY(condition, __VA_ARGS__)
#else
#define CB_ASSERT(condition, ...)   \
    do                              \
    {                               \
        (void)sizeof(!(condition)); \
    } while (false)
#endif
