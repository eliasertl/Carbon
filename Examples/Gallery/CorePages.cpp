// The gallery's pages for the components of the Carbon core library.

#include <format>
#include <string>

#include "Pages.h"

namespace Gallery
{
    using namespace Carbon;

    void BeginSection(std::string_view title, std::string_view description)
    {
        BeginVStack({.Spacing = 6.0f, .Width = Size::Fill()});
        Text(title, {.Style = TextStyle::Headline});
        if (!description.empty())
            Text(description,
                 {.Style = TextStyle::Subheadline, .Secondary = true, .Width = Size::Fill(), .Wraps = true});
        Spacer({.Length = 2.0f});
        BeginVStack({.Spacing = 12.0f,
                     .Padding = 16.0f,
                     .Width = Size::Fill(),
                     .Background = GetStyleColor(StyleColor::SecondaryBackground)});
    }

    void EndSection()
    {
        EndVStack();
        EndVStack();
    }

    void BeginRow(std::string_view label)
    {
        BeginHStack({.Spacing = 12.0f, .Width = Size::Fill()});
        Text(label, {.Secondary = true, .Width = LabelColumn});
    }

    void EndRow()
    {
        EndHStack();
    }

    void TypographyPage()
    {
        BeginSection("Typography",
                     "The macOS type ramp, set in Public Sans. Sizes and line heights follow the "
                     "Human Interface Guidelines.");
        struct Entry
        {
            TextStyle Style;
            const char* Name;
        };
        static const Entry Entries[] = {
            {TextStyle::LargeTitle, "Large Title"}, {TextStyle::Title1, "Title 1"},
            {TextStyle::Title2, "Title 2"},         {TextStyle::Title3, "Title 3"},
            {TextStyle::Headline, "Headline"},      {TextStyle::Body, "Body"},
            {TextStyle::Callout, "Callout"},        {TextStyle::Subheadline, "Subheadline"},
            {TextStyle::Footnote, "Footnote"},      {TextStyle::Caption1, "Caption 1"},
            {TextStyle::Caption2, "Caption 2"},
        };
        for (const Entry& entry : Entries)
        {
            const TextSpec spec = GetTextSpec(entry.Style);
            PushID(entry.Name);
            BeginHStack({.Spacing = 12.0f, .Alignment = VerticalAlignment::Center, .Width = Size::Fill()});
            Text(std::format("{} / {}", spec.Size, spec.LineHeight),
                 {.Style = TextStyle::Caption1, .Secondary = true, .Width = LabelColumn});
            Text(entry.Name, {.Style = entry.Style});
            Spacer();
            Text(entry.Name, {.Style = entry.Style, .Emphasized = true});
            EndHStack();
            PopID();
        }
        EndSection();
    }

    void IconsPage()
    {
        BeginSection("Icons",
                     "1530 Phosphor icons in three weights. They are text: they sit inside labels, follow "
                     "the text color and scale with the content.");
        static const char* const Samples[] = {Icons::House,   Icons::Gear,     Icons::MagnifyingGlass, Icons::Bell,
                                              Icons::Heart,   Icons::Star,     Icons::Folder,          Icons::Trash,
                                              Icons::Lock,    Icons::Cloud,    Icons::WifiHigh,        Icons::Palette,
                                              Icons::Command, Icons::Lightning};
        struct Row
        {
            const char* Name;
            IconVariant Variant;
        };
        static const Row Rows[] = {
            {"Regular", IconVariant::Regular}, {"Bold", IconVariant::Bold}, {"Fill", IconVariant::Fill}};
        for (const Row& row : Rows)
        {
            PushID(row.Name);
            BeginRow(row.Name);
            for (const char* icon : Samples)
            {
                IconOptions options;
                options.Size = 20.0f;
                options.Variant = row.Variant;
                if (row.Variant == IconVariant::Fill)
                    options.Color = GetStyleColor(StyleColor::Accent);
                Icon(icon, options);
            }
            EndRow();
            PopID();
        }
        BeginRow("In text");
        Text(std::string(Icons::Folder) + "  Documents      " + Icons::CloudArrowDown + "  12 items downloading");
        EndRow();
        EndSection();
    }

    void ButtonsPage(GalleryState& state)
    {
        BeginSection("Buttons",
                     "Use a prominent button for the most likely action of a view, and no more than one or "
                     "two of them.");
        BeginRow("Roles");
        if (Button("Default"))
            state.Clicks++;
        Tooltip("A standard bordered button");
        if (Button("Prominent", {.Role = ButtonRole::Prominent}))
            state.Clicks++;
        Tooltip("The most likely action");
        if (Button("Plain", {.Role = ButtonRole::Plain}))
            state.Clicks++;
        Tooltip("No background until hovered");
        if (Button("Delete", {.Role = ButtonRole::Destructive}))
            state.Clicks++;
        Tooltip("Destroys data");
        Spacer();
        Text(std::format("{} clicks", state.Clicks), {.Secondary = true});
        EndRow();

        BeginRow("Sizes");
        Button("Small", {.ControlSize = ControlSize::Small});
        Button("Regular");
        Button("Large", {.ControlSize = ControlSize::Large});
        Button("Large##prominent", {.Role = ButtonRole::Prominent, .ControlSize = ControlSize::Large});
        EndRow();

        BeginRow("With icons");
        Button("Add", {.Icon = Icons::Plus});
        Button("Share", {.Role = ButtonRole::Prominent, .Icon = Icons::Export});
        Button("##more", {.Icon = Icons::DotsThree});
        Tooltip("More actions");
        Button("##trash", {.Role = ButtonRole::Destructive, .Icon = Icons::Trash});
        Tooltip("Move to trash");
        EndRow();

        BeginRow("Disabled");
        Button("Default##disabled", {.Disabled = true});
        Button("Prominent##disabled", {.Role = ButtonRole::Prominent, .Disabled = true});
        Button("Plain##disabled", {.Role = ButtonRole::Plain, .Disabled = true});
        EndRow();

        BeginRow("Custom");
        Button("Tinted", {.Role = ButtonRole::Prominent, .Tint = GetStyleColor(StyleColor::Green)});
        Button("Pill",
               {.Role = ButtonRole::Prominent, .CornerRadius = 12.0f, .Tint = GetStyleColor(StyleColor::Indigo)});
        Button("Square corners", {.CornerRadius = 2.0f});
        Button("Fills the row", {.Width = Size::Fill()});
        EndRow();
        EndSection();
    }

    void TogglesPage(GalleryState& state)
    {
        BeginSection("Toggles",
                     "Switches for settings that deserve visual weight, checkboxes for lists and "
                     "hierarchies. The whole row is clickable.");
        Toggle("Wi-Fi", &state.WiFi, {.Width = Size::Fill()});
        Separator();
        Toggle("Bluetooth", &state.Bluetooth, {.Width = Size::Fill()});
        Separator();
        Toggle("Airplane Mode", &state.AirplaneMode, {.Width = Size::Fill(), .Disabled = true});
        Separator();

        BeginRow("Sizes");
        Toggle("##small", &state.WiFi, {.ControlSize = ControlSize::Small});
        Toggle("##regular", &state.WiFi);
        Toggle("##large", &state.WiFi, {.ControlSize = ControlSize::Large});
        Toggle("##tinted", &state.WiFi, {.Tint = GetStyleColor(StyleColor::Green)});
        EndRow();

        BeginRow("Checkboxes");
        BeginVStack({.Spacing = 8.0f});
        // A parent checkbox shows a dash while its children differ.
        const bool isMixed = state.AutoSave != state.ShowHidden;
        bool all = state.AutoSave && state.ShowHidden;
        if (Toggle("Select all", &all, {.Kind = ToggleKind::Checkbox, .IsMixed = isMixed}))
        {
            state.AutoSave = all;
            state.ShowHidden = all;
        }
        BeginVStack({.Spacing = 8.0f, .Padding = EdgeInsets(20.0f, 0.0f, 0.0f, 0.0f)});
        Toggle("Save automatically", &state.AutoSave, {.Kind = ToggleKind::Checkbox});
        Toggle("Show hidden files", &state.ShowHidden, {.Kind = ToggleKind::Checkbox});
        Toggle("Sync all devices", &state.SyncAll, {.Kind = ToggleKind::Checkbox, .Disabled = true});
        EndVStack();
        EndVStack();
        EndRow();
        EndSection();
    }

    void SlidersPage(GalleryState& state)
    {
        BeginSection("Sliders", "Drag the knob, click the track, or use the arrow keys.");
        BeginRow("Volume");
        Icon(Icons::SpeakerLow, {.Color = GetStyleColor(StyleColor::SecondaryLabel)});
        Slider("Volume", &state.Volume, 0.0f, 1.0f, {.Width = Size::Fill()});
        Icon(Icons::SpeakerHigh, {.Color = GetStyleColor(StyleColor::SecondaryLabel)});
        Text(std::format("{:3.0f} %", state.Volume * 100.0f),
             {.Secondary = true, .Width = 44.0f, .Alignment = TextAlignment::Trailing});
        EndRow();

        BeginRow("Steps");
        Slider("Steps", &state.Steps, 0.0f, 10.0f, {.Step = 1.0f, .ShowsTicks = true, .Width = 220.0f});
        Text(std::format("{:.0f}", state.Steps), {.Secondary = true});
        EndRow();

        BeginRow("Sizes");
        Slider("Small", &state.Brightness, 0.0f, 1.0f, {.ControlSize = ControlSize::Small, .Width = 120.0f});
        Slider("Regular", &state.Brightness, 0.0f, 1.0f, {.Width = 120.0f});
        Slider("Large", &state.Brightness, 0.0f, 1.0f, {.ControlSize = ControlSize::Large, .Width = 120.0f});
        EndRow();

        BeginRow("Disabled");
        Slider("Disabled", &state.Brightness, 0.0f, 1.0f, {.Width = 220.0f, .Disabled = true});
        EndRow();
        EndSection();
    }

    void TextFieldsPage(GalleryState& state)
    {
        BeginSection("Text fields",
                     "Caret, selection, clipboard and undo. Double-click selects a word, triple-click "
                     "everything; Tab selects the whole field.");
        BeginRow("Name");
        TextField("Name", &state.Name, {.Width = 240.0f});
        EndRow();

        BeginRow("Email");
        TextField("Email", &state.Email, {.Placeholder = "name@example.com", .Width = 240.0f});
        EndRow();

        BeginRow("Password");
        TextField("Password", &state.Password, {.Width = 240.0f, .IsSecure = true});
        EndRow();

        BeginRow("Search");
        TextField("Search", &state.Search,
                  {.Icon = Icons::MagnifyingGlass, .ShowsClearButton = true, .Width = Size::Fill()});
        EndRow();

        BeginRow("Disabled");
        TextField("Disabled", &state.Name, {.Width = 240.0f, .Disabled = true});
        EndRow();
        EndSection();
    }

    void ImagesPage(GalleryState& state)
    {
        BeginSection("Images and separators",
                     "Image shows any texture view of the host, optionally with squircle "
                     "corners. Separators adapt to the direction of their stack.");
        BeginHStack({.Spacing = 16.0f, .Alignment = VerticalAlignment::Center});
        Image(state.Artwork, Vec2(72.0f, 72.0f));
        Image(state.Artwork, Vec2(72.0f, 72.0f), {.CornerRadius = 16.0f});
        Image(state.Artwork, Vec2(72.0f, 72.0f), {.CornerRadius = 36.0f});
        Separator();
        Image(state.Artwork, Vec2(128.0f, 72.0f), {.CornerRadius = 10.0f, .UV = Rect(0.0f, 0.25f, 1.0f, 0.5f)});
        Separator();
        Image(state.Artwork, Vec2(72.0f, 72.0f), {.CornerRadius = 16.0f, .Tint = GetStyleColor(StyleColor::Accent)});
        EndHStack();
        EndSection();
    }

    void LayoutPage()
    {
        BeginSection("Layout", "Stacks, spacers and fill sizes. No positions are computed by hand.");
        const Color box = GetStyleColor(StyleColor::ControlFill);
        const auto chip = [&](std::string_view label, Size width = Size::Fit())
        {
            BeginHStack({.Padding = EdgeInsets(10.0f, 5.0f),
                         .Justify = Alignment::Center,
                         .Width = width,
                         .Background = box,
                         .CornerRadius = 6.0f});
            Text(label, {.Style = TextStyle::Subheadline});
            EndHStack();
        };

        BeginRow("Spacer");
        chip("Leading");
        Spacer();
        chip("Trailing");
        EndRow();

        BeginRow("Fill 1 : 2 : 1");
        chip("1", Size::Fill(1.0f));
        chip("2", Size::Fill(2.0f));
        chip("1", Size::Fill(1.0f));
        EndRow();

        BeginRow("Centered");
        BeginHStack({.Justify = Alignment::Center, .Width = Size::Fill()});
        chip("Centered in the remaining width");
        EndHStack();
        EndRow();
        EndSection();
    }
} // namespace Gallery
