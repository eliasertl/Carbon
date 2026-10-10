#include "StartScreen.h"

#include <algorithm>
#include <format>
#include <string>

#include <Carbon/Carbon.h>
#include <Carbon/Extension.h>

#include "WebPlatform.h"

namespace WebApp
{
    using namespace Carbon;

    namespace
    {
        constexpr float CardWidth = 300.0f;
        constexpr float CardHeight = 168.0f;
        constexpr float CardRadius = 16.0f;
        constexpr float CardPadding = 22.0f;
        constexpr float BadgeSize = 44.0f;
        constexpr float LogoSize = 88.0f;

        // The app icon: a squircle in the accent color with a cube on it.
        void Logo()
        {
            const Rect rect = AllocateItem(Vec2(LogoSize));
            DrawList& drawList = GetDrawList();
            const float radius = LogoSize * 0.225f;
            const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
            drawList.AddShadow(rect, GetStyleColor(StyleColor::Shadow), radius, 16.0f, Vec2(0.0f, 6.0f), smoothing);
            drawList.AddSquircle(rect, GetStyleColor(StyleColor::Accent), radius, smoothing);
            DrawIcon(drawList, rect.GetCenter(), Icons::Cube, LogoSize * 0.56f, GetStyleColor(StyleColor::OnAccent),
                     IconVariant::Fill);
        }

        // A large button: an icon badge, a title and a description, lifting a little while hovered. Activates
        // with a click, or Space and Enter after Tab.
        bool Card(std::string_view label, std::string_view icon, std::string_view description, Color tint, float width)
        {
            const ID id = GetID(label);
            const Rect rect = AllocateItem(Vec2(width, CardHeight));
            const Interaction interaction = ButtonBehavior(id, rect);
            const ControlFeedback feedback = AnimateFeedback(id, interaction.Hovered, interaction.Pressed);
            const float lift = Animate(HashID("lift", id), interaction.Hovered && !interaction.Pressed ? 1.0f : 0.0f,
                                       AnimationSpec::Spring(0.3f, 0.8f));

            DrawList& drawList = GetDrawList();
            const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
            const Rect card(rect.X, rect.Y - lift * 2.0f, rect.Width, rect.Height);
            drawList.AddShadow(card, GetStyleColor(StyleColor::Shadow).WithAlpha(0.10f + 0.08f * lift), CardRadius,
                               12.0f + 10.0f * lift, Vec2(0.0f, 3.0f + 4.0f * lift), smoothing);
            const Color fill =
                ApplyFeedback(GetStyleColor(StyleColor::ControlBackground), GetStyleColor(StyleColor::Label), feedback);
            drawList.AddSquircle(card, fill, CardRadius, smoothing);
            drawList.AddSquircleStroke(card, GetStyleColor(StyleColor::Separator), CardRadius,
                                       GetContentScale().GetPixelSize(), smoothing);

            const Rect badge(card.X + CardPadding, card.Y + CardPadding, BadgeSize, BadgeSize);
            drawList.AddSquircle(badge, tint, BadgeSize * 0.25f, smoothing);
            DrawIcon(drawList, badge.GetCenter(), icon, 24.0f, Color::White(), IconVariant::Fill);
            DrawIcon(drawList, Vec2(card.GetRight() - CardPadding - 6.0f, badge.GetCenter().Y), Icons::ArrowRight,
                     16.0f, GetStyleColor(StyleColor::TertiaryLabel), IconVariant::Bold);

            const TextSpec title = GetTextSpec(TextStyle::Title3, true);
            drawList.AddText(Vec2(card.X + CardPadding, badge.GetBottom() + 16.0f), GetDisplayLabel(label), title,
                             GetStyleColor(StyleColor::Label));
            TextSpec body = GetTextSpec(TextStyle::Body);
            body.MaxWidth = width - CardPadding * 2.0f;
            body.Wraps = true;
            drawList.AddText(
                Vec2(card.X + CardPadding, badge.GetBottom() + 16.0f + GetFontMetrics(title).LineHeight + 4.0f),
                description, body, GetStyleColor(StyleColor::SecondaryLabel));

            if (interaction.Hovered)
                SetCursor(Cursor::PointingHand);
            DrawFocusRing(id, card, CardRadius);
            SetLastItem(id, rect, interaction);
            return interaction.Clicked;
        }
    } // namespace

    StartChoice BuildStartScreen(bool& isDark, std::string_view repositoryUrl)
    {
        StartChoice choice = StartChoice::None;
        BeginVStack({.Spacing = 0.0f, .Alignment = Alignment::Center, .Width = Size::Fill(), .Height = Size::Fill()});

        BeginHStack({.Padding = EdgeInsets(24.0f, 14.0f), .Width = Size::Fill()});
        Spacer();
        if (Toggle("Dark", &isDark, {.ControlSize = ControlSize::Small}))
            SetTheme(isDark ? Theme::Dark() : Theme::Light());
        EndHStack();

        // On a phone the texts wrap and the cards stand one above the other, as wide as the display allows.
        const bool isCompact = IsCompactWidth();
        const float cardWidth = isCompact ? std::min(CardWidth + 60.0f, GetContentRect().Width - 32.0f) : CardWidth;
        const TextOptions centered = {
            .Width = isCompact ? Size::Fill() : Size::Fit(), .Wraps = isCompact, .Alignment = TextAlignment::Center};
        Spacer();
        Logo();
        Spacer({.Length = 22.0f});
        Text("Carbon", {.Style = TextStyle::LargeTitle, .Emphasized = true});
        Spacer({.Length = 8.0f});
        BeginVStack({.Spacing = 4.0f,
                     .Padding = EdgeInsets(isCompact ? 24.0f : 0.0f, 0.0f),
                     .Alignment = Alignment::Center,
                     .Width = isCompact ? Size::Fill() : Size::Fit()});
        TextOptions subtitle = centered;
        subtitle.Style = TextStyle::Title3;
        subtitle.Secondary = true;
        Text("An immediate-mode C++20 UI framework with a macOS look", subtitle);
        TextOptions note = centered;
        note.Secondary = true;
        Text("Everything on this page is drawn by Carbon, compiled to WebAssembly and rendered with WebGL 2.", note);
        EndVStack();
        Spacer({.Length = isCompact ? 28.0f : 40.0f});

        if (isCompact)
            BeginVStack({.Spacing = 16.0f, .Alignment = Alignment::Center});
        else
            BeginHStack({.Spacing = 20.0f});
        if (Card("Gallery", Icons::SquaresFour, "Every component, live and interactive, in both appearances.",
                 GetStyleColor(StyleColor::Accent), cardWidth))
            choice = StartChoice::Gallery;
        if (Card("Documentation", Icons::BookOpenText,
                 "The guides and a page for every component, read from the repository's Docs folder.",
                 GetStyleColor(StyleColor::Orange), cardWidth))
            choice = StartChoice::Docs;
        if (isCompact)
            EndVStack();
        else
            EndHStack();

        Spacer();
        BeginHStack({.Spacing = 12.0f, .Padding = EdgeInsets(24.0f, 18.0f)});
        static const std::string version = std::format("Carbon {}", GetVersionString());
        Text(version, {.Style = TextStyle::Footnote, .Secondary = true});
        if (Button("Source on GitHub",
                   {.Role = ButtonRole::Plain, .ControlSize = ControlSize::Small, .Icon = Icons::GithubLogo}))
            OpenUrl(repositoryUrl);
        EndHStack();

        EndVStack();
        return choice;
    }
} // namespace WebApp
