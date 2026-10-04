// CustomComponent: a component that is not part of Carbon, built from Carbon's public extension API only.
// StarRating.h and StarRating.cpp are the component; this file is an application that uses it. The walk-through
// is in Docs/CustomComponents.md.
//
//   CustomComponent [--theme light|dark] [--scale <factor>] [--screenshot <file.png>]

#include <format>

#include <Carbon/Carbon.h>

#include "ExampleApp.h"
#include "StarRating.h"

namespace
{
    using namespace Carbon;

    struct ReviewState
    {
        int Overall = 4;
        int Design = 3;
        int Speed = 5;
        bool IsDark = false;
    };

    const char* Describe(int rating)
    {
        static const char* const Words[] = {"Not rated", "Poor", "Fair", "Good", "Very good", "Excellent"};
        return Words[rating < 0 ? 0 : (rating > 5 ? 5 : rating)];
    }

    void Row(std::string_view title, int* rating, const Example::StarRatingOptions& options = {})
    {
        BeginHStack({.Spacing = 12.0f, .Width = Size::Fill()});
        Text(title, {.Width = 90.0f});
        // The custom component is called exactly like a built-in one.
        Example::StarRating(title, rating, options);
        Tooltip("Click a star, or use the arrow keys");
        Spacer();
        Text(Describe(*rating), {.Secondary = true});
        EndHStack();
    }

    void BuildInterface(ReviewState& state)
    {
        BeginVStack({.Padding = 32.0f,
                     .Alignment = Alignment::Center,
                     .Justify = VerticalAlignment::Center,
                     .Width = Size::Fill(),
                     .Height = Size::Fill()});
        BeginVStack({.Spacing = 14.0f,
                     .Padding = 24.0f,
                     .Width = 380.0f,
                     .Background = GetStyleColor(StyleColor::SecondaryBackground)});

        Text("Rate your experience", {.Style = TextStyle::Title2, .Emphasized = true});
        Text(
            "StarRating is a custom component. It lives in this example, not in Carbon, and uses only the "
            "public extension API.",
            {.Style = TextStyle::Subheadline, .Secondary = true, .Width = Size::Fill(), .Wraps = true});
        Separator();

        Row("Overall", &state.Overall, {.StarSize = 26.0f});
        Row("Design", &state.Design);
        Row("Speed", &state.Speed, {.Tint = GetStyleColor(StyleColor::Orange)});
        int locked = 2;
        Row("Read-only", &locked, {.Disabled = true});

        Separator();
        BeginHStack({.Spacing = 12.0f, .Width = Size::Fill()});
        Text(std::format("Average: {:.1f} of 5", float(state.Overall + state.Design + state.Speed) / 3.0f),
             {.Secondary = true});
        Spacer();
        if (Toggle("Dark", &state.IsDark, {.ControlSize = ControlSize::Small}))
            SetTheme(state.IsDark ? Theme::Dark() : Theme::Light());
        if (Button("Reset"))
            state.Overall = state.Design = state.Speed = 0;
        EndHStack();

        EndVStack();
        EndVStack();
    }
} // namespace

int main(int argc, char** argv)
{
    Example::App app(argc, argv, "Carbon Custom Component", 560, 420);
    if (!app.IsReady())
        return 1;

    ReviewState state;
    state.IsDark = app.GetArguments().IsDark;
    return app.Run([&state] { BuildInterface(state); });
}
