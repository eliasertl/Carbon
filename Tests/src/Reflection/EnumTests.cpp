#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>

#include "Carbon/Reflection/Reflection.h"

// Reflected types must not be local to a function, and CB_REFLECT_ENUM must be written in the enum's namespace.
namespace ReflectionTests
{
    enum class Quality
    {
        Low,
        Medium,
        High,
        VeryHigh
    };

    enum class Sparse : int
    {
        Negative = -100,
        Minus = -1,
        Zero = 0,
        Ten = 10,
        Edge = 127,
        Outside = 128,
        FarAway = 5000
    };
    CB_REFLECT_ENUM(Sparse, CB_VALUE(FarAway, {.DisplayName = "Far Away (named)"}));

    enum class Small : uint8_t
    {
        One = 1,
        Big = 200
    };

    enum class Wide : int
    {
        Low = 0,
        Mid = 300,
        High = 1000
    };
    CB_REFLECT_ENUM(Wide, {.Min = 0, .Max = 1000});

    enum class Counted
    {
        First,
        Second,
        Count
    };
    CB_REFLECT_ENUM(Counted, CB_VALUE(Count, {.Hidden = true}), CB_VALUE(First, {.DisplayName = "Number One"}));

    enum Unscoped : int
    {
        UnscopedRed,
        UnscopedGreen
    };

    enum class Empty : int
    {
    };

    enum class Aliased
    {
        Original = 1,
        Alias = 1,
        Other = 2
    };

    struct Outer
    {
        enum class Nested
        {
            Inside,
            AlsoInside
        };
    };
    CB_REFLECT_ENUM(Outer::Nested, CB_VALUE(AlsoInside, {.DisplayName = "Also"}));
} // namespace ReflectionTests

namespace Carbon
{
    using namespace ReflectionTests;

    TEST(EnumTests, FindsEveryEnumeratorAutomatically)
    {
        static_assert(GetEnumCount<Quality>() == 4);
        static_assert(GetEnumValue<Quality>(3) == Quality::VeryHigh);
        static_assert(GetEnumIndex(Quality::High) == 2);
        static_assert(GetEnumName(Quality::VeryHigh) == "VeryHigh");
        EXPECT_EQ(GetEnumDisplayName(Quality::VeryHigh), "Very High");
        EXPECT_EQ(GetEnumDisplayName(Quality::Low), "Low");
        ASSERT_EQ(GetEnumDisplayNames<Quality>().size(), 4u);
        EXPECT_EQ(GetEnumDisplayNames<Quality>()[1], "Medium");
        EXPECT_EQ(GetEnumValues<Quality>().size(), 4u);
    }

    TEST(EnumTests, NegativeAndSparseValuesAreSortedAndOutsideTheRangeOnlyWhenNamed)
    {
        // -100, -1, 0, 10 and 127 are in the default range [-128, 127]; 128 is not; 5000 is named by the macro.
        static_assert(GetEnumCount<Sparse>() == 6);
        EXPECT_EQ(GetEnumValue<Sparse>(0), Sparse::Negative);
        EXPECT_EQ(GetEnumValue<Sparse>(1), Sparse::Minus);
        EXPECT_EQ(GetEnumValue<Sparse>(4), Sparse::Edge);
        EXPECT_EQ(GetEnumValue<Sparse>(5), Sparse::FarAway);
        EXPECT_EQ(GetEnumIndex(Sparse::Outside), -1);
        EXPECT_EQ(GetEnumName(Sparse::Outside), "");
        EXPECT_EQ(GetEnumDisplayName(Sparse::Outside), "");
        EXPECT_EQ(GetEnumDisplayName(Sparse::FarAway), "Far Away (named)");
        EXPECT_EQ(GetEnumName(Sparse::FarAway), "FarAway");
    }

    TEST(EnumTests, ValuesThatAreNoEnumeratorAreNotReflected)
    {
        EXPECT_EQ(GetEnumIndex(static_cast<Quality>(7)), -1);
        EXPECT_EQ(GetEnumIndex(static_cast<Quality>(-3)), -1);
        EXPECT_EQ(GetEnumName(static_cast<Quality>(7)), "");
    }

    TEST(EnumTests, TheRangeIsClampedToTheUnderlyingType)
    {
        // [-128, 127] becomes [0, 127] for uint8_t; 200 lies outside.
        static_assert(GetEnumCount<Small>() == 1);
        EXPECT_EQ(GetEnumName(Small::One), "One");
        EXPECT_EQ(GetEnumIndex(Small::Big), -1);
    }

    TEST(EnumTests, TheMacroWidensTheRange)
    {
        static_assert(GetEnumCount<Wide>() == 3);
        EXPECT_EQ(GetEnumName(Wide::Mid), "Mid");
        EXPECT_EQ(GetEnumIndex(Wide::High), 2);
    }

    TEST(EnumTests, HiddenValuesAreLeftOutAndDisplayNamesReplaceLabels)
    {
        static_assert(GetEnumCount<Counted>() == 2);
        EXPECT_EQ(GetEnumIndex(Counted::Count), -1);
        EXPECT_EQ(GetEnumDisplayName(Counted::First), "Number One");
        EXPECT_EQ(GetEnumDisplayName(Counted::Second), "Second");
    }

    TEST(EnumTests, UnscopedNestedEmptyAndAliasedEnums)
    {
        static_assert(GetEnumCount<Unscoped>() == 2);
        EXPECT_EQ(GetEnumDisplayName(UnscopedGreen), "Unscoped Green");

        static_assert(GetEnumCount<Outer::Nested>() == 2);
        EXPECT_EQ(GetEnumDisplayName(Outer::Nested::Inside), "Inside");
        EXPECT_EQ(GetEnumDisplayName(Outer::Nested::AlsoInside), "Also");

        static_assert(GetEnumCount<Empty>() == 0);
        EXPECT_TRUE(GetEnumDisplayNames<Empty>().empty());

        // Two enumerators with one value are one value, named by either.
        static_assert(GetEnumCount<Aliased>() == 2);
        EXPECT_FALSE(GetEnumName(Aliased::Alias).empty());
    }

    TEST(EnumTests, LabelsLiveInStaticStorage)
    {
        const std::string_view first = GetEnumDisplayName(Quality::VeryHigh);
        const std::string_view second = GetEnumDisplayName(Quality::VeryHigh);
        EXPECT_EQ(first.data(), second.data());
        EXPECT_EQ(GetEnumDisplayNames<Quality>()[3].data(), first.data());
    }
} // namespace Carbon
