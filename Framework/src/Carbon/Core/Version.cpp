#include "Carbon/Core/Version.h"

#define CB_STRINGIFY_IMPL(value) #value
#define CB_STRINGIFY(value) CB_STRINGIFY_IMPL(value)

namespace Carbon
{
    Version GetVersion()
    {
        return Version{CARBON_VERSION_MAJOR, CARBON_VERSION_MINOR, CARBON_VERSION_PATCH};
    }

    std::string_view GetVersionString()
    {
        return CB_STRINGIFY(CARBON_VERSION_MAJOR) "." CB_STRINGIFY(CARBON_VERSION_MINOR) "." CB_STRINGIFY(
            CARBON_VERSION_PATCH);
    }
} // namespace Carbon
