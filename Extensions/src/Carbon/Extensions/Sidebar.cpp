#include "Carbon/Extensions/Sidebar.h"

#include <algorithm>

#include "Carbon/Extensions/Internal/NavigationStack.h"
#include "Carbon/Extensions/Internal/SelectionList.h"

namespace Carbon
{
    namespace
    {
        constexpr float RowHeight = 28.0f;
        constexpr float RowPadding = 8.0f;
        constexpr float RowCornerRadius = 6.0f;
        constexpr float IconSize = 16.0f;
        constexpr float IconGap = 7.0f;
        constexpr float HeaderHeight = 26.0f;
        constexpr float ChevronSize = 13.0f;

        // In the root of a collapsed navigation the sidebar is the list that leads to the content: it fills the
        // width, and its rows end with a chevron, as in iOS.
        bool IsNavigationList()
        {
            return Internal::IsInCollapsedNavigationRoot();
        }
    } // namespace

    void BeginSidebar(std::string_view id, const SidebarOptions& options)
    {
        Internal::SelectionListDescription description;
        const bool isNavigationList = IsNavigationList();
        description.Scroll.Width = isNavigationList ? Size::Fill() : Size::Fixed(options.Width);
        description.Scroll.Height = Size::Fill();
        description.Scroll.Spacing = 1.0f;
        description.Scroll.Padding = options.Padding;
        description.Background = GetStyleColor(StyleColor::SecondaryBackground);
        description.HasTrailingSeparator = !isNavigationList;
        description.RowRadius = RowCornerRadius;
        description.AnimatesHighlight = true;
        Internal::BeginSelectionList(id, description);
    }

    void EndSidebar()
    {
        Internal::EndSelectionList();
    }

    void SidebarHeader(std::string_view title)
    {
        const TextSpec spec = GetTextSpec(TextStyle::Subheadline, true);
        ItemOptions item;
        item.Width = Size::Fill();
        const Rect rect = AllocateItem(Vec2(0.0f, HeaderHeight), item);
        // The title sits at the bottom of its row, close to the items it names.
        const FontMetrics metrics = GetFontMetrics(spec);
        GetDrawList().AddText(Vec2(rect.X + RowPadding, rect.GetBottom() - metrics.LineHeight - 4.0f), title, spec,
                              GetStyleColor(StyleColor::SecondaryLabel));
    }

    bool SidebarItem(std::string_view label, bool isSelected, const SidebarItemOptions& options)
    {
        const ID id = GetID(label);
        const std::string_view title = GetDisplayLabel(label);

        PushDisabled(options.Disabled);
        const Internal::SelectionRow row =
            Internal::SelectionListRow(id, GetAdaptiveRowHeight(RowHeight), isSelected, options.Disabled);
        if (!row.IsVisible)
        {
            PopDisabled();
            return row.Clicked;
        }

        DrawList& drawList = GetDrawList();
        const Color onAccent = GetStyleColor(StyleColor::OnAccent);
        const Color text = row.IsEmphasized ? onAccent : GetStyleColor(StyleColor::Label);
        const float centerY = row.Bounds.GetCenter().Y;
        float x = row.Bounds.X + RowPadding;
        if (!options.Icon.empty())
        {
            const Color tint = row.IsEmphasized ? onAccent : Resolve(options.IconTint, StyleColor::Accent);
            DrawIcon(drawList, Vec2(x + IconSize * 0.5f, centerY), options.Icon, IconSize, tint);
            x += IconSize + IconGap;
        }

        float right = row.Bounds.GetRight() - RowPadding;
        if (IsNavigationList())
        {
            const Color chevron = row.IsEmphasized ? onAccent : GetStyleColor(StyleColor::TertiaryLabel);
            DrawIcon(drawList, Vec2(right - ChevronSize * 0.5f, centerY), Icons::CaretRight, ChevronSize, chevron,
                     IconVariant::Bold);
            right -= ChevronSize + IconGap;
        }
        TextSpec spec = GetTextSpec(TextStyle::Body);
        if (!options.Badge.empty())
        {
            const float width = MeasureText(options.Badge, spec).X;
            DrawLabel(drawList, row.Bounds, right - width, options.Badge, spec,
                      row.IsEmphasized ? onAccent : GetStyleColor(StyleColor::SecondaryLabel));
            right -= width + IconGap;
        }

        // A title that does not fit is cut off with an ellipsis.
        spec.MaxWidth = std::max(right - x, 1.0f);
        spec.Wraps = false;
        DrawLabel(drawList, row.Bounds, x, title, spec, text);

        PopDisabled();
        SetLastItem(id, row.Bounds, row.Interaction);
        return row.Clicked;
    }
} // namespace Carbon
