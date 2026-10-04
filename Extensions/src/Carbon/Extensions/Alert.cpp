#include "Carbon/Extensions/Alert.h"

namespace Carbon
{
    namespace
    {
        constexpr float AlertPadding = 20.0f;
        constexpr float AlertCornerRadius = 12.0f;
        constexpr float IconSize = 48.0f;
    } // namespace

    void OpenAlert(std::string_view id)
    {
        OpenOverlay(GetID(id));
    }

    AlertResult Alert(std::string_view id, std::string_view title, const AlertOptions& options)
    {
        const ID alert = GetID(id);
        OverlayOptions overlay;
        overlay.Placement = OverlayPlacement::Center;
        overlay.IsModal = true;
        overlay.HasScrim = true;
        overlay.DismissOnOutsideClick = false;
        // Escape answers the alert instead of just closing it; see below.
        overlay.DismissOnEscape = false;
        overlay.Padding = EdgeInsets(AlertPadding);
        overlay.Spacing = 0.0f;
        overlay.Width = Size::Fixed(options.Width);
        overlay.ContentAlignment = Alignment::Center;
        overlay.CornerRadius = AlertCornerRadius;
        if (!BeginOverlay(alert, overlay))
            return AlertResult::None;

        AlertResult result = AlertResult::None;
        const bool hasSecondary = !options.SecondaryLabel.empty();

        if (!options.Icon.empty())
        {
            IconOptions icon;
            icon.Size = IconSize;
            icon.Color = GetStyleColor(options.IsDestructive ? StyleColor::Destructive : StyleColor::Accent);
            icon.Variant = IconVariant::Fill;
            Icon(options.Icon, icon);
            Spacer({.Length = 12.0f});
        }

        TextOptions titleOptions;
        titleOptions.Style = TextStyle::Headline;
        titleOptions.Width = Size::Fill();
        titleOptions.Wraps = true;
        titleOptions.Alignment = TextAlignment::Center;
        Text(title, titleOptions);

        if (!options.Message.empty())
        {
            Spacer({.Length = 6.0f});
            TextOptions message;
            message.Style = TextStyle::Subheadline;
            message.Width = Size::Fill();
            message.Wraps = true;
            message.Alignment = TextAlignment::Center;
            Text(options.Message, message);
        }

        Spacer({.Length = 16.0f});
        BeginHStack({.Spacing = 8.0f, .Width = Size::Fill()});
        if (hasSecondary)
        {
            ButtonOptions secondary;
            secondary.ControlSize = ControlSize::Large;
            secondary.Width = Size::Fill();
            secondary.IsDefault = options.IsDestructive;
            if (Button(options.SecondaryLabel, secondary))
                result = AlertResult::Secondary;
        }
        ButtonOptions primary;
        primary.ControlSize = ControlSize::Large;
        primary.Width = Size::Fill();
        primary.Role = options.IsDestructive ? ButtonRole::Destructive : ButtonRole::Prominent;
        primary.IsDefault = !options.IsDestructive || !hasSecondary;
        if (Button(options.PrimaryLabel, primary))
            result = AlertResult::Primary;
        EndHStack();

        // Escape backs out when there is a way to back out; an alert with one button just closes.
        if (result == AlertResult::None && IsKeyPressed(Key::Escape, false))
            result = hasSecondary ? AlertResult::Secondary : AlertResult::Primary;

        if (result != AlertResult::None)
            CloseCurrentOverlay();
        EndOverlay();
        return result;
    }
} // namespace Carbon
