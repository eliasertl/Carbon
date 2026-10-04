#include "Carbon/Extensions/TabView.h"

#include "Carbon/Extensions/SegmentedControl.h"

namespace Carbon
{
    namespace
    {
        constexpr float TabGap = 10.0f;
    }

    void BeginTabView(std::string_view id, int* selected, std::span<const std::string_view> tabs,
                      const TabViewOptions& options)
    {
        // Tabs on top, centered, and below them a grouped area that holds the selected pane.
        BeginVStack({.Spacing = TabGap,
                     .Alignment = Alignment::Center,
                     .Width = options.Width,
                     .Height = options.Height,
                     .ID = id});
        PushID(id);
        SegmentedControl("##tabs", selected, tabs);

        BeginVStack({.Spacing = options.Spacing,
                     .Padding = options.Padding,
                     .Width = Size::Fill(),
                     .Height = options.Height.Mode == SizeMode::Fit ? Size::Fit() : Size::Fill(),
                     .Background = GetStyleColor(StyleColor::ControlFill).WithOpacity(0.5f)});
    }

    void BeginTabView(std::string_view id, int* selected, std::initializer_list<std::string_view> tabs,
                      const TabViewOptions& options)
    {
        BeginTabView(id, selected, std::span<const std::string_view>(tabs.begin(), tabs.size()), options);
    }

    void EndTabView()
    {
        EndVStack();
        PopID();
        EndVStack();
    }
} // namespace Carbon
