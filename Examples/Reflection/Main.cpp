// Reflection: an application's settings window, built from one AppSettings struct. Every section of the window is
// a field of AppSettings, and each one is drawn by a single Carbon::Reflect call; the struct definitions below are
// all there is to the forms. Docs/Reflection.md explains automatic reflection and the macros.
//
//   Reflection [--theme light|dark] [--scale <factor>] [--size <width>x<height>] [--screenshot <file.png>]
//              [--page general|appearance|audio|network]

#include <array>
#include <string>
#include <string_view>

#include <Carbon/Carbon.h>
#include <Carbon/Extensions/Extensions.h>
#include <Carbon/Reflection/Reflection.h>

#include "ExampleApp.h"

namespace Settings
{
    // ---- The settings ------------------------------------------------------------------------------------------

    enum class StartupBehavior
    {
        OpenLastDocument,
        ShowWelcomeWindow,
        StartEmpty
    };

    enum class UpdateChannel
    {
        Stable,
        Beta,
        Nightly
    };

    struct General
    {
        std::string ComputerName = "Studio";
        StartupBehavior OnStartup = StartupBehavior::OpenLastDocument;
        bool LaunchAtLogin = true;
        UpdateChannel Updates = UpdateChannel::Stable;
        bool InstallUpdatesAutomatically = true;
        int AutosaveMinutes = 5;
        Carbon::DateTime LicenseExpires = {.Year = 2027, .Month = 3, .Day = 31};
    };
    CB_REFLECT_STRUCT(General,
                      CB_FIELD(OnStartup, {.DisplayName = "On Startup", .Control = Carbon::ReflectControl::RadioGroup}),
                      CB_FIELD(InstallUpdatesAutomatically, {.DisplayName = "Install updates automatically",
                                                             .Control = Carbon::ReflectControl::Checkbox}),
                      CB_FIELD(AutosaveMinutes, {.DisplayName = "Autosave (minutes)",
                                                 .Min = 1,
                                                 .Max = 60,
                                                 .Control = Carbon::ReflectControl::Stepper}),
                      CB_FIELD(LicenseExpires,
                               {.Tooltip = "Renew your license from the account page", .ReadOnly = true}));

    enum class Appearance
    {
        Light,
        Dark
    };

    enum class TextSize
    {
        Small,
        Medium,
        Large
    };

    struct AppearanceSettings
    {
        Appearance Mode = Appearance::Light;
        Carbon::Color AccentColor = Carbon::Color::FromHex(0x007AFF);
        TextSize SidebarTextSize = TextSize::Medium;
        bool ShowScrollBars = true;
        bool ReduceMotion = false;
    };
    CB_REFLECT_STRUCT(AppearanceSettings,
                      CB_FIELD(Mode,
                               {.DisplayName = "Appearance", .Control = Carbon::ReflectControl::SegmentedControl}),
                      CB_FIELD(ReduceMotion, {.Tooltip = "Replaces movement with cross-fades"}));

    // Sample rates are far outside the automatically searched range, so the macro names them.
    enum class SampleRate
    {
        Rate44100 = 44100,
        Rate48000 = 48000,
        Rate96000 = 96000
    };
    CB_REFLECT_ENUM(SampleRate, CB_VALUE(Rate44100, {.DisplayName = "44.1 kHz"}),
                    CB_VALUE(Rate48000, {.DisplayName = "48 kHz"}), CB_VALUE(Rate96000, {.DisplayName = "96 kHz"}));

    struct Equalizer
    {
        float Bass = 0.0f;
        float Mid = 1.5f;
        float Treble = -2.0f;
    };
    CB_REFLECT_STRUCT(Equalizer, CB_FIELD(Bass, {.Min = -12.0, .Max = 12.0, .Step = 0.5}),
                      CB_FIELD(Mid, {.Min = -12.0, .Max = 12.0, .Step = 0.5}),
                      CB_FIELD(Treble, {.Min = -12.0, .Max = 12.0, .Step = 0.5}));

    struct Audio
    {
        std::string OutputDevice = "Built-in Speakers";
        int DeviceId = 7;
        float Volume = 0.65f;
        SampleRate Rate = SampleRate::Rate48000;
        bool PlayFeedbackSounds = true;
        bool Mono = false;
        Equalizer EQ = {};
    };
    CB_REFLECT_STRUCT(Audio, CB_FIELD(OutputDevice, {.ReadOnly = true}), CB_FIELD(DeviceId, {.Hidden = true}),
                      CB_FIELD(Volume, {.Min = 0.0, .Max = 1.0, .Tooltip = "Output level of every sound"}),
                      CB_FIELD(Rate, {.DisplayName = "Sample Rate"}),
                      CB_FIELD(Mono, {.DisplayName = "Play stereo audio as mono",
                                      .Control = Carbon::ReflectControl::Checkbox}),
                      CB_FIELD(EQ, {.DisplayName = "Equalizer"}));

    struct Proxy
    {
        bool Enabled = false;
        std::string Server = "proxy.example.com";
        int Port = 8080;
    };
    CB_REFLECT_STRUCT(Proxy, CB_FIELD(Enabled, {.DisplayName = "Use a proxy server"}),
                      CB_FIELD(Port, {.Min = 1, .Max = 65535, .Control = Carbon::ReflectControl::Stepper}));

    struct Network
    {
        bool DownloadInBackground = true;
        int MaxDownloads = 4;
        double TimeoutSeconds = 30.0;
        Proxy HTTPProxy = {};
    };
    CB_REFLECT_STRUCT(Network, CB_FIELD(MaxDownloads, {.DisplayName = "Simultaneous Downloads", .Min = 1, .Max = 8}),
                      CB_FIELD(TimeoutSeconds, {.DisplayName = "Timeout (seconds)",
                                                .Min = 5.0,
                                                .Max = 120.0,
                                                .Step = 5.0,
                                                .Control = Carbon::ReflectControl::Stepper}),
                      CB_FIELD(HTTPProxy, {.DisplayName = "HTTP Proxy"}));

    /// Everything the window edits. Each field is one section of the sidebar.
    struct AppSettings
    {
        General GeneralSettings = {};
        AppearanceSettings Look = {};
        Audio Sound = {};
        Network Connection = {};
    };

    // ---- The window --------------------------------------------------------------------------------------------

    using namespace Carbon;

    enum class Section
    {
        General,
        Appearance,
        Audio,
        Network
    };

    struct SectionInfo
    {
        Section Id;
        std::string_view Title;
        std::string_view Key;
        std::string_view Icon;
        std::string_view Description;
    };

    const std::array<SectionInfo, 4> Sections = {{
        {Section::General, "General", "general", Icons::Gear, "How the app starts, saves and updates."},
        {Section::Appearance, "Appearance", "appearance", Icons::PaintBrush,
         "Colors and motion. Changes apply at once."},
        {Section::Audio, "Audio", "audio", Icons::SpeakerHigh, "Output, levels and the equalizer."},
        {Section::Network, "Network", "network", Icons::Globe, "Downloads and the proxy server."},
    }};

    struct WindowState
    {
        AppSettings Values;
        Section Current = Section::General;
    };

    // Applies what the Appearance section changes to the whole window.
    void ApplyAppearance(const AppearanceSettings& look, bool animated)
    {
        SetTheme(look.Mode == Appearance::Dark ? Theme::Dark() : Theme::Light(), animated);
        SetReduceMotion(look.ReduceMotion);
    }

    // The selected section: a title, a short description and its settings in a grouped box.
    void SectionContent(WindowState& state, const SectionInfo& section)
    {
        BeginScrollView(section.Key, {.Spacing = 6.0f, .Padding = EdgeInsets(28.0f, 22.0f)});
        Text(section.Title, {.Style = TextStyle::Title2, .Emphasized = true});
        Text(section.Description, {.Secondary = true});
        Spacer({.Length = 10.0f});
        BeginVStack(
            {.Padding = 18.0f, .Width = Size::Fill(), .Background = GetStyleColor(StyleColor::SecondaryBackground)});
        AppSettings& values = state.Values;
        switch (section.Id)
        {
            case Section::General:
                Reflect("general", &values.GeneralSettings);
                break;
            case Section::Appearance:
                if (Reflect("appearance", &values.Look))
                    ApplyAppearance(values.Look, true);
                break;
            case Section::Audio:
                Reflect("audio", &values.Sound);
                break;
            case Section::Network:
                Reflect("network", &values.Connection);
                break;
        }
        EndVStack();

        Spacer({.Length = 10.0f});
        BeginHStack({.Width = Size::Fill()});
        Spacer();
        if (Button("Restore Defaults"))
        {
            switch (section.Id)
            {
                case Section::General:
                    values.GeneralSettings = {};
                    break;
                case Section::Appearance:
                    values.Look = {};
                    ApplyAppearance(values.Look, true);
                    break;
                case Section::Audio:
                    values.Sound = {};
                    break;
                case Section::Network:
                    values.Connection = {};
                    break;
            }
        }
        EndHStack();
        EndScrollView();
    }

    void BuildWindow(WindowState& state)
    {
        // The accent color the Appearance section chose tints every control.
        PushStyleColor(StyleColor::Accent, state.Values.Look.AccentColor);
        BeginHStack(
            {.Spacing = 0.0f, .Alignment = VerticalAlignment::Top, .Width = Size::Fill(), .Height = Size::Fill()});
        BeginSidebar("sections", {.Width = 200.0f});
        SidebarHeader("Settings");
        for (const SectionInfo& section : Sections)
        {
            if (SidebarItem(section.Title, section.Id == state.Current, {.Icon = section.Icon}))
                state.Current = section.Id;
        }
        EndSidebar();
        SectionContent(state, Sections[static_cast<size_t>(state.Current)]);
        EndHStack();
        PopStyleColor();
    }
} // namespace Settings

int main(int argc, char** argv)
{
    Example::App app(argc, argv, "Carbon Reflection", 860, 600);
    if (!app.IsReady())
        return 1;

    Settings::WindowState state;
    state.Values.Look.Mode = app.GetArguments().IsDark ? Settings::Appearance::Dark : Settings::Appearance::Light;
    Settings::ApplyAppearance(state.Values.Look, false);
    for (const Settings::SectionInfo& section : Settings::Sections)
    {
        if (app.GetArguments().Page == section.Key)
            state.Current = section.Id;
    }
    return app.Run([&state] { Settings::BuildWindow(state); });
}
