// The gallery's page for CarbonReflection: interface generated from the application's own enums and structs.
// Each section shows the C++ source of a type in a code box, then what Reflect builds from that very type.

#include <string>
#include <string_view>

#include <Carbon/Reflection/Reflection.h>

#include "Pages.h"

namespace Gallery
{
    // The types below are shown as source code on the page. Each raw string literal must stay in sync with the
    // definition next to it.

    enum class Quality
    {
        Low,
        Medium,
        High,
        VeryHigh
    };

    constexpr std::string_view QualitySource = R"(enum class Quality
{
    Low,
    Medium,
    High,
    VeryHigh
};)";

    struct GraphicsSettings
    {
        Quality TextureQuality = Quality::High;
        bool VSync = true;
        float RenderScale = 1.0f;
        int FrameRateLimit = 120;
        std::string Profile = "Balanced";
    };

    constexpr std::string_view GraphicsSettingsSource = R"(struct GraphicsSettings
{
    Quality TextureQuality = Quality::High;
    bool VSync = true;
    float RenderScale = 1.0f;
    int FrameRateLimit = 120;
    std::string Profile = "Balanced";
};)";

    struct Equalizer
    {
        float Bass = 2.0f;
        float Treble = -1.5f;
    };
    CB_REFLECT_STRUCT(Equalizer, CB_FIELD(Bass, {.Min = -12.0, .Max = 12.0, .Step = 0.5}),
                      CB_FIELD(Treble, {.Min = -12.0, .Max = 12.0, .Step = 0.5}));

    struct AudioSettings
    {
        float Volume = 0.8f;
        int Bitrate = 256;
        Quality Resampling = Quality::High;
        bool Mono = false;
        int DeviceId = 3;
        std::string DeviceName = "Built-in Output";
        Carbon::Color MeterColor = Carbon::Color::FromHex(0x30D158);
        Carbon::DateTime LastCalibrated = {.Year = 2026, .Month = 9, .Day = 14};
        Equalizer EQ = {};
    };
    CB_REFLECT_STRUCT(
        AudioSettings, CB_FIELD(Volume, {.Min = 0.0, .Max = 1.0, .Tooltip = "Output level of every sound"}),
        CB_FIELD(Bitrate, {.Min = 64, .Max = 320, .Step = 32, .Control = Carbon::ReflectControl::Stepper}),
        CB_FIELD(Resampling, {.Control = Carbon::ReflectControl::SegmentedControl}),
        CB_FIELD(Mono, {.DisplayName = "Play stereo audio as mono", .Control = Carbon::ReflectControl::Checkbox}),
        CB_FIELD(DeviceId, {.Hidden = true}), CB_FIELD(DeviceName, {.DisplayName = "Output Device", .ReadOnly = true}),
        CB_FIELD(EQ, {.DisplayName = "Equalizer"}));

    constexpr std::string_view AudioSettingsSource = R"(struct Equalizer
{
    float Bass = 2.0f;
    float Treble = -1.5f;
};
CB_REFLECT_STRUCT(Equalizer,
    CB_FIELD(Bass, { .Min = -12.0, .Max = 12.0, .Step = 0.5 }),
    CB_FIELD(Treble, { .Min = -12.0, .Max = 12.0, .Step = 0.5 }));

struct AudioSettings
{
    float Volume = 0.8f;
    int Bitrate = 256;
    Quality Resampling = Quality::High;
    bool Mono = false;
    int DeviceId = 3;
    std::string DeviceName = "Built-in Output";
    Carbon::Color MeterColor = Carbon::Color::FromHex(0x30D158);
    Carbon::DateTime LastCalibrated = { .Year = 2026, .Month = 9, .Day = 14 };
    Equalizer EQ;
};
CB_REFLECT_STRUCT(AudioSettings,
    CB_FIELD(Volume, { .Min = 0.0, .Max = 1.0, .Tooltip = "Output level of every sound" }),
    CB_FIELD(Bitrate, { .Min = 64, .Max = 320, .Step = 32, .Control = ReflectControl::Stepper }),
    CB_FIELD(Resampling, { .Control = ReflectControl::SegmentedControl }),
    CB_FIELD(Mono, { .DisplayName = "Play stereo audio as mono",
                     .Control = ReflectControl::Checkbox }),
    CB_FIELD(DeviceId, { .Hidden = true }),
    CB_FIELD(DeviceName, { .DisplayName = "Output Device", .ReadOnly = true }),
    CB_FIELD(EQ, { .DisplayName = "Equalizer" }));)";

    using namespace Carbon;

    namespace
    {
        // Padding inside a code box, and its corner radius.
        constexpr float CodePadding = 14.0f;
        constexpr float CodeCornerRadius = 8.0f;

        Quality s_PopUpQuality = Quality::High;
        Quality s_SegmentedQuality = Quality::Medium;
        Quality s_RadioQuality = Quality::Low;
        GraphicsSettings s_LeadingGraphics;
        GraphicsSettings s_AboveGraphics;
        AudioSettings s_Audio;

        // Source code in the monospaced font, on a rounded fill that sets it off from the section.
        void CodeBox(std::string_view code)
        {
            BeginVStack({.Padding = CodePadding,
                         .Width = Size::Fill(),
                         .Background = GetStyleColor(StyleColor::ControlFill),
                         .CornerRadius = CodeCornerRadius});
            PushFont(GetMonospacedFont());
            Text(code, {.Style = TextStyle::Callout});
            PopFont();
            EndVStack();
        }
    } // namespace

    void ReflectionPage()
    {
        BeginSection("Enums",
                     "An enum becomes a pop-up button of its values, labeled from their names, with no code per type: "
                     "Reflect(\"quality\", &quality).");
        CodeBox(QualitySource);
        BeginRow("Texture quality");
        Reflect("quality", &s_PopUpQuality);
        EndRow();
        EndSection();

        BeginSection("Enum styles",
                     "ReflectOptions::EnumStyle shows the same enum as a segmented control or as radio buttons.");
        CodeBox(QualitySource);
        BeginRow("Segmented control");
        Reflect("segmented", &s_SegmentedQuality, {.EnumStyle = ReflectEnumStyle::SegmentedControl});
        EndRow();
        // A radio group is taller than one line: its label sits at the top, next to the first button.
        BeginHStack({.Spacing = 12.0f, .Alignment = VerticalAlignment::Top, .Width = Size::Fill()});
        Text("Radio buttons", {.Secondary = true, .Width = LabelColumn});
        Reflect("radio", &s_RadioQuality, {.EnumStyle = ReflectEnumStyle::RadioGroup});
        EndHStack();
        EndSection();

        BeginSection("Structs",
                     "A plain struct becomes one labeled control per field. Labels sit in a column as wide as the "
                     "longest one. Numbers without a range get a stepper.");
        CodeBox(GraphicsSettingsSource);
        Reflect("graphics", &s_LeadingGraphics);
        EndSection();

        BeginSection("Labels above controls",
                     "The same struct with { .Layout = ReflectLayout::LabelAbove }, for narrow places such as "
                     "inspectors.");
        CodeBox(GraphicsSettingsSource);
        Reflect("graphics", &s_AboveGraphics, {.Layout = ReflectLayout::LabelAbove});
        EndSection();

        BeginSection("Metadata",
                     "CB_REFLECT_STRUCT adds display names, a tooltip, ranges, control overrides and hidden or "
                     "read-only fields, without changing the struct. Nested structs become indented groups.");
        CodeBox(AudioSettingsSource);
        Reflect("audio", &s_Audio);
        EndSection();
    }
} // namespace Gallery
