// The entry point of CarbonTests. It differs from GoogleTest's own in one way: a run in which every test was
// skipped exits with SkippedExitCode instead of 0. CTest runs the tests of each renderer backend as one entry
// (see Tests/CMakeLists.txt) and shows such an entry as skipped: its backend has no device on this machine.

#include <gtest/gtest.h>

namespace
{
    constexpr int SkippedExitCode = 77;
} // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    const ::testing::UnitTest& tests = *::testing::UnitTest::GetInstance();
    if (result == 0 && tests.skipped_test_count() > 0 && tests.successful_test_count() == 0)
        return SkippedExitCode;
    return result;
}
