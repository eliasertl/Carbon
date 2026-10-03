#include <gtest/gtest.h>

#include <format>

#include "Carbon/Carbon.h"

#if defined(CARBON_TESTS_HAVE_EXTENSIONS)
#include "Carbon/Extensions/Extensions.h"
#endif

namespace Carbon
{
    TEST(VersionTests, StringMatchesComponents)
    {
        const Version version = GetVersion();
        EXPECT_EQ(GetVersionString(), std::format("{}.{}.{}", version.Major, version.Minor, version.Patch));
    }

#if defined(CARBON_TESTS_HAVE_EXTENSIONS)
    TEST(VersionTests, ExtensionsMatchCore)
    {
        const Version core = GetVersion();
        const Version extensions = GetExtensionsVersion();
        EXPECT_EQ(core.Major, extensions.Major);
        EXPECT_EQ(core.Minor, extensions.Minor);
        EXPECT_EQ(core.Patch, extensions.Patch);
    }
#endif
} // namespace Carbon
