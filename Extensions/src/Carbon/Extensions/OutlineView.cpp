#include "Carbon/Extensions/OutlineView.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Carbon/Extensions/Internal/BuildState.h"
#include "Carbon/Extensions/Internal/ColumnLayout.h"
#include "Carbon/Extensions/Internal/SelectionList.h"

namespace Carbon
{
    namespace
    {
        constexpr int MaxDepth = 32;
        constexpr float ListPadding = 5.0f;
        constexpr float CornerRadius = 7.0f;
        // Each level of the hierarchy is indented by this much.
        constexpr float IndentWidth = 16.0f;
        constexpr float DisclosureWidth = 18.0f;
        constexpr float IconSize = 15.0f;
        constexpr float IconGap = 6.0f;
        constexpr AnimationSpec DisclosureSpring = AnimationSpec::Spring(0.22f);

        using Internal::CellPadding;
        using Internal::ColumnLayout;

        // The outline view whose items are being added.
        struct OutlineBuild
        {
            /// Indexed like the columns; points into s_Columns.
            ColumnLayout* Columns;
            int ColumnCount;
            int NextColumn;
            /// The ordinal of the open item at each depth, so that the left arrow can move to the parent.
            int Ordinals[MaxDepth];
            int Depth;
            int RowIndex;
            Rect Row;
            float RowHeight;
            /// While above zero, items deeper than this take ForceValue as their expansion: expanding or collapsing
            /// everything nested inside an item.
            int ForceDepth;
            bool ForceValue;
            bool HasColumns;
            bool HasFrame;
            bool ShowsAlternatingRows;
            bool IsRowEmphasized;
            /// The current item is inside the visible area: its cells are drawn.
            bool IsRowVisible;
            bool IsOpen;
        };

        // Remembered per item, also while it is not shown.
        struct ExpansionState
        {
            bool IsExpanded;
            bool IsKnown;
            /// The item was collapsed with everything inside it: collapse its children when it opens again.
            bool CollapseDescendants;
        };

        Internal::BuildState<OutlineBuild> s_Build("Carbon.OutlineView.Build");
        // The layout of the columns of the outline view being built. Outline views do not nest, so one is enough;
        // it grows with the largest number of columns seen.
        std::vector<ColumnLayout> s_Columns;

        OutlineBuild& GetBuild()
        {
            return s_Build.Get();
        }

        // A chevron that points right when collapsed and down when expanded, turning in between.
        void DrawDisclosure(Vec2 center, float turn, Color color)
        {
            const float angle = turn * 1.5707963f;
            const float cosine = std::cos(angle);
            const float sine = std::sin(angle);
            const auto rotate = [&](Vec2 point)
            { return center + Vec2(point.X * cosine - point.Y * sine, point.X * sine + point.Y * cosine); };
            DrawList& drawList = GetDrawList();
            drawList.AddLine(rotate(Vec2(-1.75f, -3.5f)), rotate(Vec2(1.75f, 0.0f)), color, 1.6f);
            drawList.AddLine(rotate(Vec2(1.75f, 0.0f)), rotate(Vec2(-1.75f, 3.5f)), color, 1.6f);
        }
    } // namespace

    void BeginOutlineView(std::string_view id, const OutlineViewOptions& options)
    {
        OutlineBuild& build = s_Build.Begin();
        build = OutlineBuild();
        build.RowHeight = options.RowHeight;
        build.ShowsAlternatingRows = options.ShowsAlternatingRows;
        build.HasColumns = !options.Columns.empty();
        build.ForceDepth = -1;
        build.IsOpen = true;

        Internal::SelectionListDescription description;
        description.Scroll.Spacing = 0.0f;
        if (build.HasColumns)
        {
            // Header and rows share a frame, as in a table.
            build.HasFrame = true;
            BeginVStack({.Spacing = 0.0f,
                         .Width = options.Width,
                         .Height = options.Height,
                         .Background = options.HasBorder
                                           ? std::optional<Color>(GetStyleColor(StyleColor::ControlBackground))
                                           : std::nullopt,
                         .CornerRadius = CornerRadius,
                         .ID = id});
            PushID(id);
            ItemOptions item;
            item.Width = Size::Fill();
            const Rect header = AllocateItem(Vec2(0.0f, Internal::ColumnHeaderHeight), item);
            const std::span<ColumnLayout> layouts = Internal::GrowStorage(s_Columns, options.Columns.size());
            Internal::LayoutColumns(options.Columns, header.Width - ListPadding * 2.0f, layouts);
            Internal::DrawColumnHeader(header, ListPadding, options.Columns, layouts);
            build.Columns = layouts.data();
            build.ColumnCount = static_cast<int>(layouts.size());
            description.Scroll.Padding = EdgeInsets(ListPadding, 3.0f);
            Internal::BeginSelectionList("##rows", description);
        }
        else
        {
            description.Scroll.Width = options.Width;
            description.Scroll.Height = options.Height;
            description.Scroll.Padding = EdgeInsets(ListPadding);
            if (options.HasBorder)
            {
                description.Background = GetStyleColor(StyleColor::ControlBackground);
                description.BackgroundRadius = CornerRadius;
                description.HasBorder = true;
            }
            Internal::BeginSelectionList(id, description);
        }
    }

    void EndOutlineView()
    {
        OutlineBuild& build = GetBuild();
        CB_VERIFY(build.Depth == 0, "Unbalanced outline view: {} BeginOutlineItem call(s) without EndOutlineItem",
                  build.Depth);
        for (; build.Depth > 0; build.Depth--)
            PopID();
        build.IsOpen = false;
        Internal::EndSelectionList();
        if (build.HasFrame)
        {
            PopID();
            EndVStack();
            GetDrawList().AddSquircleStroke(GetLastItemRect(), GetStyleColor(StyleColor::ControlBorder), CornerRadius,
                                            GetContentScale().GetPixelSize(), GetStyleVar(StyleVar::CornerSmoothing));
        }
    }

    OutlineItem BeginOutlineItem(std::string_view label, bool isSelected, const OutlineItemOptions& options)
    {
        OutlineItem result;
        OutlineBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen, "Outline items must be added between BeginOutlineView and EndOutlineView");
        if (!build.IsOpen)
            return result;

        const ID id = GetID(label);
        const int depth = build.Depth;
        // Only an item that can contain others remembers whether it is expanded.
        ExpansionState leafExpansion = {};
        ExpansionState& expansion =
            options.HasChildren ? *GetState<ExpansionState>(HashID("##expansion", id), StateLifetime::Persistent)
                                : leafExpansion;
        if (!expansion.IsKnown)
        {
            expansion.IsExpanded = options.IsInitiallyExpanded;
            expansion.IsKnown = true;
        }
        if (build.ForceDepth >= 0 && depth > build.ForceDepth && options.HasChildren)
            expansion.IsExpanded = build.ForceValue;

        PushDisabled(options.Disabled);
        const Internal::SelectionRow row =
            Internal::SelectionListRow(id, build.RowHeight, isSelected, options.Disabled);
        build.Row = row.Bounds;
        build.IsRowEmphasized = row.IsEmphasized;
        build.IsRowVisible = row.IsVisible;
        build.NextColumn = 1;
        if (depth < MaxDepth)
            build.Ordinals[depth] = row.Ordinal;
        result.Picked = row.Clicked;
        result.Activated = row.Interaction.DoubleClicked;

        if (row.IsVisible && build.ShowsAlternatingRows && !isSelected && build.RowIndex % 2 == 1)
        {
            GetDrawList().AddSquircle(row.Bounds, GetStyleColor(StyleColor::ControlFill).WithOpacity(0.35f), 5.0f,
                                      GetStyleVar(StyleVar::CornerSmoothing));
        }
        build.RowIndex++;

        // Expanding and collapsing: the disclosure triangle, or the arrow keys on the selected item.
        const bool isAltHeld = HasModifiers(GetKeyModifiers(), KeyModifiers::Alt);
        bool toggle = false;
        bool isRecursive = false;
        const float indent = IndentWidth * static_cast<float>(std::min(depth, MaxDepth));
        const Rect disclosure(row.Bounds.X + indent, row.Bounds.Y, DisclosureWidth, row.Bounds.Height);
        bool isDisclosureHovered = false;
        if (options.HasChildren && row.IsVisible)
        {
            ButtonBehaviorOptions behavior;
            behavior.Focusable = false;
            const Interaction interaction = ButtonBehavior(HashID("##disclosure", id), disclosure, behavior);
            isDisclosureHovered = interaction.Hovered;
            if (interaction.Clicked)
            {
                toggle = true;
                isRecursive = isAltHeld;
            }
        }
        if (isSelected && Internal::IsSelectionListFocused() && !IsDisabled())
        {
            if (IsKeyPressed(Key::RightArrow))
            {
                if (options.HasChildren && !expansion.IsExpanded)
                {
                    toggle = true;
                    isRecursive = isAltHeld;
                }
                else if (options.HasChildren)
                {
                    Internal::RequestSelectionListPick(row.Ordinal + 1);
                }
            }
            if (IsKeyPressed(Key::LeftArrow))
            {
                if (options.HasChildren && expansion.IsExpanded)
                {
                    toggle = true;
                    isRecursive = isAltHeld;
                }
                else if (depth > 0 && depth <= MaxDepth)
                {
                    Internal::RequestSelectionListPick(build.Ordinals[depth - 1]);
                }
            }
            if (IsKeyPressed(Key::Enter, false) || IsKeyPressed(Key::KeypadEnter, false))
                result.Activated = true;
        }
        if (toggle)
        {
            expansion.IsExpanded = !expansion.IsExpanded;
            if (isRecursive && expansion.IsExpanded)
            {
                // Expand everything inside, starting with the children added right after this item.
                build.ForceDepth = depth;
                build.ForceValue = true;
            }
            else if (isRecursive)
            {
                expansion.CollapseDescendants = true;
            }
        }
        if (expansion.IsExpanded && expansion.CollapseDescendants && build.ForceDepth < 0)
        {
            build.ForceDepth = depth;
            build.ForceValue = false;
            expansion.CollapseDescendants = false;
        }
        result.IsExpanded = options.HasChildren && expansion.IsExpanded;

        // An item that is scrolled out of view takes its space and keeps its place in the hierarchy; there is
        // nothing to draw.
        if (!row.IsVisible)
        {
            PopDisabled();
            PushID(id);
            build.Depth++;
            return result;
        }

        // Drawing: disclosure triangle, icon and title, within the first column.
        const std::string_view title = GetDisplayLabel(label);
        const Color onAccent = GetStyleColor(StyleColor::OnAccent);
        const float centerY = row.Bounds.GetCenter().Y;
        if (options.HasChildren)
        {
            const float turn = Animate(HashID("##turn", id), result.IsExpanded ? 1.0f : 0.0f, DisclosureSpring);
            const Color color =
                row.IsEmphasized ? onAccent
                                 : GetStyleColor(isDisclosureHovered ? StyleColor::Label : StyleColor::SecondaryLabel);
            DrawDisclosure(Vec2(disclosure.GetCenter().X, centerY), turn, color);
        }
        float x = disclosure.GetRight();
        const float firstColumnEnd = build.HasColumns && build.ColumnCount > 0
                                         ? row.Bounds.X + build.Columns[0].Width - CellPadding
                                         : row.Bounds.GetRight() - CellPadding;
        if (!options.Icon.empty())
        {
            const Color tint = row.IsEmphasized ? onAccent : Resolve(options.IconTint, StyleColor::Accent);
            DrawIcon(GetDrawList(), Vec2(x + IconSize * 0.5f, centerY), options.Icon, IconSize, tint);
            x += IconSize + IconGap;
        }
        TextSpec spec = GetTextSpec(TextStyle::Body);
        spec.MaxWidth = std::max(firstColumnEnd - x, 1.0f);
        spec.Wraps = false;
        DrawLabel(GetDrawList(), row.Bounds, x, title, spec,
                  row.IsEmphasized ? onAccent : GetStyleColor(StyleColor::Label));
        PopDisabled();
        SetLastItem(id, row.Bounds, row.Interaction);

        // The children belong to this item: its ID scopes theirs.
        PushID(id);
        build.Depth++;
        return result;
    }

    void EndOutlineItem()
    {
        OutlineBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen && build.Depth > 0, "EndOutlineItem called without BeginOutlineItem");
        if (!build.IsOpen || build.Depth <= 0)
            return;
        PopID();
        build.Depth--;
        if (build.Depth == build.ForceDepth)
            build.ForceDepth = -1;
    }

    void OutlineCell(std::string_view text, const TableCellOptions& options)
    {
        OutlineBuild& build = GetBuild();
        const bool hasColumn = build.IsOpen && build.NextColumn < build.ColumnCount;
        CB_VERIFY(hasColumn, "The outline item has more cells than the outline view has columns");
        if (!hasColumn)
            return;
        const ColumnLayout& column = build.Columns[build.NextColumn++];
        if (!build.IsRowVisible)
            return;
        const Rect cell(build.Row.X + column.X + CellPadding, build.Row.Y,
                        std::max(column.Width - CellPadding * 2.0f, 0.0f), build.Row.Height);
        Internal::DrawCellText(cell, text, options, column.Alignment, build.IsRowEmphasized);
    }
} // namespace Carbon
