#include "Carbon/Extensions/List.h"

#include <algorithm>

#include "Carbon/Extensions/Internal/SelectionList.h"

namespace Carbon
{
    namespace
    {
        constexpr float ListPadding = 5.0f;
        constexpr float ListCornerRadius = 7.0f;
        constexpr float RowPadding = 8.0f;
        constexpr float IconSize = 15.0f;
        constexpr float IconGap = 6.0f;

        struct ListBuild
        {
            float RowHeight;
        };

        ListBuild& GetBuild()
        {
            return *GetState<ListBuild>(HashID("Carbon.List.Build"), StateLifetime::Persistent);
        }
    } // namespace

    void BeginList(std::string_view id, const ListOptions& options)
    {
        Internal::SelectionListDescription description;
        description.Scroll.Width = options.Width;
        description.Scroll.Height = options.Height;
        description.Scroll.Spacing = 0.0f;
        description.Scroll.Padding = EdgeInsets(ListPadding);
        if (options.HasBorder)
        {
            description.Background = GetStyleColor(StyleColor::ControlBackground);
            description.BackgroundRadius = ListCornerRadius;
            description.HasBorder = true;
        }
        Internal::BeginSelectionList(id, description);
        GetBuild().RowHeight = options.RowHeight;
    }

    void EndList()
    {
        Internal::EndSelectionList();
    }

    bool ListItem(std::string_view label, bool isSelected, const ListItemOptions& options)
    {
        const ID id = GetID(label);
        const std::string_view title = GetDisplayLabel(label);

        PushDisabled(options.Disabled);
        const Internal::SelectionRow row =
            Internal::SelectionListRow(id, GetBuild().RowHeight, isSelected, options.Disabled);

        DrawList& drawList = GetDrawList();
        const Color onAccent = GetStyleColor(StyleColor::OnAccent);
        const float centerY = row.Bounds.GetCenter().Y;
        float x = row.Bounds.X + RowPadding;
        if (!options.Icon.empty())
        {
            DrawIcon(drawList, Vec2(x + IconSize * 0.5f, centerY), options.Icon, IconSize,
                     row.IsEmphasized ? onAccent : GetStyleColor(StyleColor::Accent));
            x += IconSize + IconGap;
        }

        float right = row.Bounds.GetRight() - RowPadding;
        TextSpec spec = GetTextSpec(TextStyle::Body);
        if (!options.Detail.empty())
        {
            const float width = MeasureText(options.Detail, spec).X;
            DrawLabel(drawList, row.Bounds, right - width, options.Detail, spec,
                      row.IsEmphasized ? onAccent : GetStyleColor(StyleColor::SecondaryLabel));
            right -= width + RowPadding;
        }

        spec.MaxWidth = std::max(right - x, 1.0f);
        spec.Wraps = false;
        DrawLabel(drawList, row.Bounds, x, title, spec, row.IsEmphasized ? onAccent : GetStyleColor(StyleColor::Label));

        PopDisabled();
        SetLastItem(id, row.Bounds, row.Interaction);
        return row.Clicked;
    }
} // namespace Carbon
