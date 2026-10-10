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
        // Reordering: a collapsed item that a drag rests on expands after this long; the item being dragged stays
        // in its place, faded.
        constexpr float AutoExpandDelay = 0.7f;
        constexpr float DraggedOpacity = 0.4f;
        constexpr float IndicatorWidth = 2.0f;
        constexpr float IndicatorCircleRadius = 3.5f;

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

            /// Reordering. The item being dragged, by key, or -1; and whether the items being added are inside
            /// it, from where it was added until the depth falls back to its own.
            bool AllowsReordering;
            int64_t DraggedKey;
            bool IsInsideDragged;
            int DraggedDepth;
            /// A row of this outline view is dragged over it.
            bool IsDropTarget;
            float PointerY;
            /// Where a drop would go, found among the rows as they are added: the target's key, position, row
            /// and depth, and whether it may receive the item at all.
            bool HasTarget;
            bool IsTargetValid;
            int64_t TargetKey;
            OutlineDropPosition TargetPosition;
            Rect TargetRow;
            int TargetDepth;
            /// The pointer is on the lower edge of an expanded item: the drop goes before its first child, which
            /// is the next item, or into it when it has none.
            bool IsTargetPending;
            int64_t PendingKey;
            Rect PendingRow;
            int PendingDepth;
            bool IsPendingValid;
            Rect LastRow;
        };

        // Remembered per outline view, for reordering.
        struct ReorderState
        {
            /// A row of this outline view was dragged over it during the last frame.
            bool WasDropTarget;
            /// The collapsed item a drag rests on, and for how long; and an item to expand when it is added.
            int64_t HoverKey;
            float HoverTime;
            int64_t ExpandKey;
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
        constexpr std::string_view ReorderStateName = "Carbon.OutlineView.Reorder";
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
        ReorderState& GetReorderState()
        {
            return *GetState<ReorderState>(HashID(ReorderStateName, Internal::GetSelectionListID()),
                                           StateLifetime::Persistent);
        }

        void SetTarget(OutlineBuild& build, int64_t key, OutlineDropPosition position, const Rect& row, int depth,
                       bool isValid)
        {
            build.HasTarget = true;
            build.TargetKey = key;
            build.TargetPosition = position;
            build.TargetRow = row;
            build.TargetDepth = depth;
            build.IsTargetValid = isValid;
        }

        // Finds where a drop would go, row by row: the upper quarter of an item that can contain others (the
        // upper half of a leaf) puts the item before it, the lower quarter (half) after it, and the middle into
        // it.
        void FindDropTarget(OutlineBuild& build, const Rect& row, int depth, int64_t key, bool hasChildren,
                            bool isExpanded, bool isValid)
        {
            if (!build.IsDropTarget)
                return;
            if (build.IsTargetPending)
            {
                build.IsTargetPending = false;
                if (depth == build.PendingDepth + 1)
                    SetTarget(build, key, OutlineDropPosition::Before, row, depth, isValid && key >= 0);
                else
                    SetTarget(build, build.PendingKey, OutlineDropPosition::Into, build.PendingRow, build.PendingDepth,
                              build.IsPendingValid);
            }
            build.LastRow = row;
            if (build.HasTarget || build.PointerY < row.Y || build.PointerY >= row.GetBottom())
                return;

            const float position = (build.PointerY - row.Y) / std::max(row.Height, 1.0f);
            const bool canTake = isValid && key >= 0;
            if (hasChildren && position >= 0.25f && position < 0.75f)
            {
                SetTarget(build, key, OutlineDropPosition::Into, row, depth, canTake);
            }
            else if (position < (hasChildren ? 0.25f : 0.5f))
            {
                SetTarget(build, key, OutlineDropPosition::Before, row, depth, canTake);
            }
            else if (hasChildren && isExpanded)
            {
                build.IsTargetPending = true;
                build.PendingKey = key;
                build.PendingRow = row;
                build.PendingDepth = depth;
                build.IsPendingValid = canTake;
            }
            else
            {
                SetTarget(build, key, OutlineDropPosition::After, row, depth, canTake);
            }
        }

        // An insertion line between rows, starting at the indentation of the items it goes between; or, for a
        // drop into an item, an outline around it.
        void DrawDropIndicator(const OutlineBuild& build)
        {
            DrawList& drawList = GetDrawList();
            const Color accent = GetStyleColor(StyleColor::Accent);
            const Rect& row = build.TargetRow;
            if (build.TargetPosition == OutlineDropPosition::Into && build.TargetKey >= 0)
            {
                const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
                drawList.AddSquircle(row, accent.WithOpacity(0.15f), 5.0f, smoothing);
                drawList.AddSquircleStroke(row.Inset(EdgeInsets(IndicatorWidth * 0.5f)), accent, 5.0f, IndicatorWidth,
                                           smoothing);
                return;
            }
            const float y = build.TargetPosition == OutlineDropPosition::Before ? row.Y : row.GetBottom();
            const float indent = IndentWidth * static_cast<float>(std::min(build.TargetDepth, MaxDepth));
            const float left = row.X + indent + DisclosureWidth - IndicatorCircleRadius;
            drawList.AddCircleStroke(Vec2(left, y), IndicatorCircleRadius, accent, IndicatorWidth * 0.75f);
            drawList.AddLine(Vec2(left + IndicatorCircleRadius, y), Vec2(row.GetRight() - 2.0f, y), accent,
                             IndicatorWidth);
        }
    } // namespace

    void BeginOutlineView(std::string_view id, const OutlineViewOptions& options)
    {
        OutlineBuild& build = s_Build.Begin();
        build = OutlineBuild();
        build.RowHeight = GetAdaptiveRowHeight(options.RowHeight);
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
            if (options.AllowsReordering)
                description.Reordering = Internal::SelectionListReordering::Custom;
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
            if (options.AllowsReordering)
                description.Reordering = Internal::SelectionListReordering::Custom;
            Internal::BeginSelectionList(id, description);
        }

        build.AllowsReordering = options.AllowsReordering;
        build.DraggedKey = -1;
        if (options.AllowsReordering)
        {
            const Internal::RowDragPayload drag = Internal::GetSelectionListDrag();
            build.DraggedKey = drag.Index >= 0 ? drag.Key : -1;
            build.IsDropTarget = build.DraggedKey >= 0 && GetReorderState().WasDropTarget;
            build.PointerY = GetMousePos().Y;
        }
    }

    OutlineMove EndOutlineView()
    {
        OutlineMove move;
        OutlineBuild& build = GetBuild();
        CB_VERIFY(build.Depth == 0, "Unbalanced outline view: {} BeginOutlineItem call(s) without EndOutlineItem",
                  build.Depth);
        for (; build.Depth > 0; build.Depth--)
            PopID();
        build.IsOpen = false;

        if (build.AllowsReordering)
        {
            ReorderState& state = GetReorderState();
            if (build.IsTargetPending)
            {
                build.IsTargetPending = false;
                SetTarget(build, build.PendingKey, OutlineDropPosition::Into, build.PendingRow, build.PendingDepth,
                          build.IsPendingValid);
            }
            // Below the last item: the dragged item becomes the last top-level one.
            if (build.IsDropTarget && !build.HasTarget && build.PointerY >= build.LastRow.GetBottom())
                SetTarget(build, -1, OutlineDropPosition::Into, build.LastRow, 0, true);

            DropTargetOptions options;
            options.ShowsHighlight = false;
            const Drop drop = AcceptDrop(HashID("##reorder", Internal::GetSelectionListID()),
                                         Internal::GetSelectionListViewport(), Internal::RowDragPayloadType, options);
            state.WasDropTarget = drop.IsHovered && build.DraggedKey >= 0;
            const bool hasValidTarget = build.IsDropTarget && build.HasTarget && build.IsTargetValid;
            if (hasValidTarget && !drop.IsDelivered)
                DrawDropIndicator(build);
            if (hasValidTarget && drop.IsDelivered)
            {
                move.Item = build.DraggedKey;
                move.Target = build.TargetKey;
                move.Position = build.TargetPosition;
            }

            // A drag that rests on a collapsed item expands it.
            const bool isOnItem =
                hasValidTarget && build.TargetPosition == OutlineDropPosition::Into && build.TargetKey >= 0;
            if (isOnItem && state.HoverKey == build.TargetKey)
            {
                state.HoverTime += GetDeltaTime();
                if (state.HoverTime >= AutoExpandDelay && state.ExpandKey != build.TargetKey)
                    state.ExpandKey = build.TargetKey;
                else if (state.HoverTime < AutoExpandDelay)
                    RequestFrameAfter(AutoExpandDelay - state.HoverTime);
            }
            else
            {
                state.HoverKey = isOnItem ? build.TargetKey : -1;
                state.HoverTime = 0.0f;
                if (isOnItem)
                    RequestFrameAfter(AutoExpandDelay);
            }
        }
        Internal::EndSelectionList();
        if (build.HasFrame)
        {
            PopID();
            EndVStack();
            GetDrawList().AddSquircleStroke(GetLastItemRect(), GetStyleColor(StyleColor::ControlBorder), CornerRadius,
                                            GetContentScale().GetPixelSize(), GetStyleVar(StyleVar::CornerSmoothing));
        }
        return move;
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

        // Reordering: the item being dragged and everything inside it cannot receive it, and a collapsed item
        // that a drag rested on expands.
        bool isDragged = false;
        if (build.AllowsReordering)
        {
            if (build.IsInsideDragged && depth <= build.DraggedDepth)
                build.IsInsideDragged = false;
            isDragged = build.DraggedKey >= 0 && options.Key == build.DraggedKey;
            if (isDragged)
            {
                build.IsInsideDragged = true;
                build.DraggedDepth = depth;
            }
            ReorderState& reorder = GetReorderState();
            if (reorder.ExpandKey >= 0 && reorder.ExpandKey == options.Key && options.HasChildren)
            {
                expansion.IsExpanded = true;
                reorder.ExpandKey = -1;
            }
        }

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

        if (build.AllowsReordering)
        {
            const bool isValidTarget = !isDragged && !build.IsInsideDragged && !options.Disabled;
            FindDropTarget(build, row.Layout, depth, options.Key, options.HasChildren, result.IsExpanded,
                           isValidTarget);
            if (options.Key >= 0 && !options.Disabled && Internal::BeginSelectionListRowDrag(row, options.Key))
            {
                BeginHStack({.Spacing = IconGap});
                if (!options.Icon.empty())
                    Icon(options.Icon, {.Size = IconSize, .Color = Resolve(options.IconTint, StyleColor::Accent)});
                Text(GetDisplayLabel(label));
                EndHStack();
                Internal::EndSelectionListRowDrag();
            }
        }

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
        if (isDragged)
            GetDrawList().PushOpacity(DraggedOpacity);
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
        if (isDragged)
            GetDrawList().PopOpacity();
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
