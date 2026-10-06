// The gallery's pages for the components of the Carbon core library.

#include <format>
#include <string>

#include "Pages.h"

namespace Gallery
{
    using namespace Carbon;

    namespace
    {
        std::string s_CapturedSections;
        Rect s_CapturedArea;
        bool s_IsCapturing = false;

        // Whether `title` matches one of the comma-separated keys: compared in lower case, letters and digits only.
        bool IsCaptured(std::string_view title)
        {
            const auto isKeyCharacter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
            const auto toLower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
            std::string_view keys = s_CapturedSections;
            while (!keys.empty())
            {
                const size_t comma = keys.find(',');
                const std::string_view key = keys.substr(0, comma);
                keys = comma == std::string_view::npos ? std::string_view() : keys.substr(comma + 1);
                size_t k = 0;
                bool matches = true;
                for (const char c : title)
                {
                    const char lower = toLower(c);
                    if (!isKeyCharacter(lower))
                        continue;
                    if (k >= key.size() || key[k] != lower)
                    {
                        matches = false;
                        break;
                    }
                    k++;
                }
                if (matches && k == key.size() && !key.empty())
                    return true;
            }
            return false;
        }
    } // namespace

    void SetCapturedSections(std::string_view keys)
    {
        s_CapturedSections = keys;
    }

    Rect GetCapturedArea()
    {
        return s_CapturedArea;
    }

    void ResetCapturedArea()
    {
        s_CapturedArea = Rect();
    }

    void BeginSection(std::string_view title, std::string_view description)
    {
        s_IsCapturing = !s_CapturedSections.empty() && IsCaptured(title);
        // Every section's stacks are begun from these lines, so the title tells them apart. The ID is pushed only
        // around the Begin calls: the widgets in the section keep their IDs.
        PushID(title);
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
        PopID();
    }

    void EndSection()
    {
        EndVStack();
        if (s_IsCapturing)
        {
            const Rect box = GetLastItemRect();
            s_CapturedArea = s_CapturedArea.Width > 0.0f ? s_CapturedArea.GetUnion(box) : box;
            s_IsCapturing = false;
        }
        EndVStack();
    }

    void BeginRow(std::string_view label)
    {
        PushID(label);
        BeginHStack({.Spacing = 12.0f, .Width = Size::Fill()});
        PopID();
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

        BeginRow("Whole numbers");
        Slider("Rating", &state.Rating, 1, 5, {.ShowsTicks = true, .Width = 220.0f});
        Text(std::format("{} of 5", state.Rating), {.Secondary = true});
        EndRow();

        BeginRow("Logarithmic");
        Slider("Frequency", &state.Frequency, 20.0, 20000.0, {.Width = 220.0f, .Scale = SliderScale::Logarithmic});
        Text(std::format("{:.0f} Hz", state.Frequency), {.Secondary = true});
        EndRow();

        BeginRow("Vertical");
        for (int band = 0; band < static_cast<int>(std::size(state.Equalizer)); band++)
        {
            PushID(band);
            Slider("Band", &state.Equalizer[band], -12.0f, 12.0f, {.Axis = Axis::Vertical, .Height = 96.0f});
            PopID();
        }
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

        BeginSection("Text area",
                     "Several lines of plain text. Lines wrap at the edge and longer text scrolls; Return starts a "
                     "new line.");
        // The label sits on the first line of the text, as macOS aligns labels of multi-line controls.
        const auto beginTopRow = [](std::string_view label)
        {
            PushID(label);
            BeginHStack({.Spacing = 12.0f, .Alignment = VerticalAlignment::Top, .Width = Size::Fill()});
            PopID();
            BeginVStack({.Padding = EdgeInsets(0.0f, 4.0f, 0.0f, 0.0f)});
            Text(label, {.Secondary = true, .Width = LabelColumn});
            EndVStack();
        };
        beginTopRow("Notes");
        TextArea("Notes", &state.Notes, {.Width = 360.0f, .Height = 96.0f});
        EndHStack();
        beginTopRow("Grows, takes Tab");
        TextArea("Outline", &state.Outline, {.Width = 360.0f, .Height = Size::Fit(), .AcceptsTab = true});
        EndHStack();
        beginTopRow("Disabled");
        TextArea("Disabled notes", &state.Notes, {.Width = 360.0f, .Height = 52.0f, .Disabled = true});
        EndHStack();
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

    void LayoutPage(GalleryState& state)
    {
        BeginSection("Layout", "Stacks, spacers and fill sizes. No positions are computed by hand.");
        const Color box = GetStyleColor(StyleColor::ControlFill);
        // Chips repeat their labels ("1", "2", "1"), so a running count gives each chip's stack its identity.
        int chipCount = 0;
        const auto chip = [&](std::string_view label, Size width = Size::Fit())
        {
            PushID(chipCount++);
            BeginHStack({.Padding = EdgeInsets(10.0f, 5.0f),
                         .Justify = Alignment::Center,
                         .Width = width,
                         .Background = box,
                         .CornerRadius = 6.0f});
            PopID();
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

        BeginSection("Grid", "Columns that line up across rows. Each column is as wide as its widest cell.");
        // A form as in macOS settings windows: labels aligned on their trailing edge, controls on their leading one.
        static constexpr Alignment FormColumns[] = {Alignment::Trailing, Alignment::Leading};
        BeginGrid({.HorizontalSpacing = 8.0f, .VerticalSpacing = 10.0f, .ColumnAlignments = FormColumns});
        BeginGridRow();
        Text("Name:");
        TextField("Name##form", &state.FormName, {.Width = 220.0f});
        EndGridRow();
        BeginGridRow();
        Text("Software updates:");
        Toggle("Download automatically", &state.FormUpdates, {.Kind = ToggleKind::Checkbox});
        EndGridRow();
        BeginGridRow();
        Spacer({.Length = 0.0f});
        Toggle("Include beta versions", &state.FormBeta, {.Kind = ToggleKind::Checkbox});
        EndGridRow();
        BeginGridRow();
        Text("Alert volume:");
        Slider("Alert volume##form", &state.FormVolume, 0.0f, 1.0f, {.Width = 220.0f});
        EndGridRow();
        EndGrid();

        Separator();

        // Cells of different widths, and a cell that spans every column.
        BeginGrid({.HorizontalSpacing = 6.0f, .VerticalSpacing = 6.0f});
        static constexpr std::string_view Planets[][3] = {
            {"Mercury", "0.39 AU", "88 days"},
            {"Jupiter", "5.2 AU", "11.9 years"},
            {"Neptune", "30 AU", "165 years"},
        };
        for (const auto& planet : Planets)
        {
            BeginGridRow({.ID = planet[0]});
            for (std::string_view cell : planet)
                chip(cell, Size::Fill());
            EndGridRow();
        }
        BeginGridRow();
        SetNextGridCell({.ColumnSpan = 3});
        chip("A cell can span several columns", Size::Fill());
        EndGridRow();
        EndGrid();
        EndSection();

        BeginSection("Scroll view",
                     "A clipped area whose content scrolls with the wheel. The overlay indicator appears while "
                     "scrolling.");
        BeginRow("Messages");
        BeginScrollView("Messages", {.Width = 320.0f, .Height = 132.0f, .Spacing = 0.0f});
        static const std::string_view Senders[] = {"Ada", "Grace", "Alan", "Edsger", "Barbara", "Ken", "Margaret"};
        for (int i = 0; i < 14; i++)
        {
            PushID(i);
            BeginHStack({.Spacing = 8.0f, .Padding = EdgeInsets(8.0f, 6.0f), .Width = Size::Fill()});
            Icon(Icons::EnvelopeSimple, {.Color = GetStyleColor(StyleColor::Accent)});
            Text(Senders[i % 7], {.Emphasized = true});
            Text(std::format("Message {}", i + 1), {.Secondary = true});
            EndHStack();
            Separator();
            PopID();
        }
        EndScrollView();
        EndRow();
        EndSection();
    }
} // namespace Gallery
