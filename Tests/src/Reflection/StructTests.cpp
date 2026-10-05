#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "Carbon/Reflection/Reflection.h"

namespace ReflectionTests
{
    enum class Mode
    {
        Off,
        On
    };

    struct Offset
    {
        int X = 0;
        int Y = 0;
    };

    struct Graphics
    {
        Mode TextureMode = Mode::On;
        bool VSync = true;
        float Gamma = 2.2f;
        std::string Name = "Default";
        Offset Position = {};
        double HDRBrightness = 1.0;
    };

    struct Annotated
    {
        float Volume = 0.5f;
        int Bitrate = 128;
        int DeviceId = 0;
        std::string DeviceName;
        bool Muted = false;
    };
    CB_REFLECT_STRUCT(Annotated, CB_FIELD(Volume, {.Min = 0.0, .Max = 1.0, .Tooltip = "Output level"}),
                      CB_FIELD(Bitrate,
                               {.Min = 64, .Max = 320, .Step = 32, .Control = Carbon::ReflectControl::Stepper}),
                      CB_FIELD(DeviceId, {.Hidden = true}),
                      CB_FIELD(DeviceName, {.DisplayName = "Output Device", .ReadOnly = true}));

    // Not an aggregate: a constructor and a private member. Only what the macro lists is reflected, in its order.
    class Account
    {
    public:
        Account() : m_Secret(42) {}

        int GetSecret() const { return m_Secret; }

        int Id = 7;
        std::string Owner = "Ada";
        bool IsActive = true;

    private:
        int m_Secret;
    };
    CB_REFLECT_STRUCT(Account, CB_FIELD(Owner, {}), CB_FIELD(IsActive, {.DisplayName = "Active"}));

    struct Empty
    {
    };

    struct Wide
    {
        int F0, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15, F16, F17, F18, F19, F20, F21, F22,
            F23, F24, F25, F26, F27, F28, F29, F30, F31, F32, F33, F34, F35, F36, F37, F38, F39, F40, F41, F42, F43,
            F44, F45, F46, F47, F48, F49, F50, F51, F52, F53, F54, F55, F56, F57, F58, F59, F60, F61, F62, F63;
    };

    struct NoDefaults
    {
        int A;
        std::vector<int> B;
    };
} // namespace ReflectionTests

namespace Carbon
{
    using namespace ReflectionTests;

    TEST(StructTests, AggregatesAreReflectedAutomatically)
    {
        static_assert(ReflectedStruct<Graphics>);
        static_assert(GetFieldCount<Graphics>() == 6);
        static_assert(GetFieldName<Graphics>(0) == "TextureMode");
        static_assert(GetFieldName<Graphics>(5) == "HDRBrightness");
        EXPECT_EQ(GetFieldName<Graphics>(1), "VSync");
        EXPECT_EQ(GetFieldName<Graphics>(3), "Name");
        EXPECT_EQ(GetFieldDisplayName<Graphics>(0), "Texture Mode");
        EXPECT_EQ(GetFieldDisplayName<Graphics>(1), "V Sync");
        EXPECT_EQ(GetFieldDisplayName<Graphics>(5), "HDR Brightness");
        EXPECT_FALSE(GetFieldOptions<Graphics>(2).Min.has_value());
        EXPECT_EQ(GetFieldOptions<Graphics>(2).Control, ReflectControl::Automatic);
    }

    TEST(StructTests, FieldsCanBeReadAndWritten)
    {
        Graphics graphics;
        GetField<2>(graphics) = 1.8f;
        GetField<3>(graphics) = "Custom";
        EXPECT_FLOAT_EQ(graphics.Gamma, 1.8f);
        EXPECT_EQ(graphics.Name, "Custom");
        const Graphics& constant = graphics;
        EXPECT_EQ(GetField<0>(constant), Mode::On);

        int visited = 0;
        ForEachField(graphics,
                     [&](size_t index, auto& field)
                     {
                         EXPECT_EQ(index, static_cast<size_t>(visited));
                         if constexpr (std::is_same_v<std::remove_cvref_t<decltype(field)>, bool>)
                             field = false;
                         visited++;
                     });
        EXPECT_EQ(visited, 6);
        EXPECT_FALSE(graphics.VSync);
    }

    TEST(StructTests, NestedAggregatesAreReflectedToo)
    {
        static_assert(ReflectedStruct<Offset>);
        Graphics graphics;
        Offset& position = GetField<4>(graphics);
        GetField<1>(position) = 12;
        EXPECT_EQ(graphics.Position.Y, 12);
        EXPECT_EQ(GetFieldDisplayName<Offset>(0), "X");
    }

    TEST(StructTests, TheMacroAddsMetadataToTheFieldsItNames)
    {
        static_assert(GetFieldCount<Annotated>() == 5);
        const ReflectFieldOptions& volume = GetFieldOptions<Annotated>(0);
        EXPECT_EQ(volume.Min, 0.0);
        EXPECT_EQ(volume.Max, 1.0);
        EXPECT_EQ(volume.Tooltip, "Output level");
        const ReflectFieldOptions& bitrate = GetFieldOptions<Annotated>(1);
        EXPECT_EQ(bitrate.Step, 32.0);
        EXPECT_EQ(bitrate.Control, ReflectControl::Stepper);
        EXPECT_TRUE(GetFieldOptions<Annotated>(2).Hidden);
        EXPECT_TRUE(GetFieldOptions<Annotated>(3).ReadOnly);
        EXPECT_EQ(GetFieldDisplayName<Annotated>(3), "Output Device");
        // A field the macro does not mention keeps the defaults.
        EXPECT_EQ(GetFieldName<Annotated>(4), "Muted");
        EXPECT_FALSE(GetFieldOptions<Annotated>(4).Hidden);
        EXPECT_EQ(GetFieldDisplayName<Annotated>(4), "Muted");
    }

    TEST(StructTests, NonAggregatesShowOnlyWhatTheMacroLists)
    {
        static_assert(ReflectedStruct<Account>);
        static_assert(GetFieldCount<Account>() == 2);
        EXPECT_EQ(GetFieldName<Account>(0), "Owner");
        EXPECT_EQ(GetFieldDisplayName<Account>(1), "Active");
        Account account;
        GetField<0>(account) = "Grace";
        EXPECT_EQ(account.Owner, "Grace");
        EXPECT_TRUE(GetField<1>(account));
    }

    TEST(StructTests, LimitsAndUnsupportedTypes)
    {
        static_assert(GetFieldCount<Empty>() == 0);
        static_assert(GetFieldCount<NoDefaults>() == 2);
        static_assert(GetFieldCount<Wide>() == 64);
        static_assert(MaxReflectedFieldCount >= 32);
        EXPECT_EQ(GetFieldName<Wide>(63), "F63");
        EXPECT_EQ(GetFieldName<Wide>(0), "F0");

        // Not aggregates and not described: no reflection.
        static_assert(!ReflectedStruct<std::string>);
        static_assert(!ReflectedStruct<int>);
        static_assert(!ReflectedStruct<Mode>);
    }
} // namespace Carbon
