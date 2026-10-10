#include "Carbon/Extensions/Internal/SelectionList.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Carbon/Extensions/Internal/BuildState.h"
#include "Carbon/Extensions/Internal/NavigationStack.h"
#include "Carbon/Extensions/Internal/RowClipping.h"

namespace Carbon::Internal
{
    namespace
    {
        constexpr size_t MaxNameLength = 63;
        // Distance kept between a row revealed by the keyboard and the edge of the visible area.
        constexpr float RevealMargin = 4.0f;
        constexpr AnimationSpec HighlightSpring = AnimationSpec::Spring(0.28f, 0.9f);
        // Rows that make room for a dragged row, or slide into their places after a drop.
        constexpr AnimationSpec SlideSpring = AnimationSpec::Spring(0.25f, 0.9f);
        constexpr float SettleDuration = 0.6f;
        // The insertion line that marks where a dragged row would go.
        constexpr float InsertionLineWidth = 2.0f;
        constexpr float InsertionCircleRadius = 3.5f;

        struct SelectionListState;

        // The list whose rows are being added.
        struct SelectionListBuild
        {
            ID Id;
            /// The list's remembered state, looked up once per frame.
            SelectionListState* State;
            /// The visible area: rows outside of it are neither hit-tested nor drawn.
            Rect Clip;
            /// The distance between two rows.
            float Spacing;
            /// Rows declared with ClipSelectionListRows that follow the ones that were added; their space is
            /// reserved when the list ends.
            int TrailingRows;
            float TrailingHeight;
            int TrailingSelected;
            /// The scroll view's name, to scroll it after it has been ended.
            char Name[MaxNameLength + 1];
            size_t NameLength;
            DeferredShape Highlight;
            DeferredShape Hover;
            Rect Viewport;
            Rect Content;
            /// Where the first row starts; moves with the scroll offset.
            Vec2 ContentOrigin;
            Rect SelectedRect;
            Rect HoveredRect;
            float RowRadius;
            float BackgroundRadius;
            int Count;
            int SelectedOrdinal;
            bool HasSelection;
            bool HasHoveredRow;
            bool IsFocused;
            bool IsEmphasized;
            /// A pick requested through RequestSelectionListPick during this frame, or -1.
            int RequestedOrdinal;
            bool AnimatesHighlight;
            bool HasBorder;
            bool IsOpen;

            SelectionListReordering Reordering;
            /// The index of the next row among all rows, added or skipped.
            int RowCounter;
            /// The row of this list being dragged; Index is -1 when none is.
            RowDragPayload Drag;
            /// A row of this list is dragged over it: the rows from the insertion slot on make room.
            bool IsDropTarget;
            /// Rows are drawn at animated offsets: making room, or sliding into their places after a drop.
            bool AreRowsMoving;
            /// The first frame after a drop: rows start from where they were drawn.
            bool IsSettleStart;
            float PointerY;
            /// Where the dragged row would go: the index of the row it would be inserted before, and that row's
            /// top relative to the content; and the top of the row after the last one.
            int Slot;
            float SlotTop;
            float NextTop;
            float Pitch;
        };

        // Remembered per list.
        struct SelectionListState
        {
            /// The row the keyboard moved to, plus one; it reports being picked. 0 when there is none.
            int PendingOrdinal;
            /// The frame in which the newly selected row is scrolled into view.
            uint64_t RevealFrame;
            /// Where the highlight is heading, relative to the content, and the last frame a row was selected.
            /// An application that selects a row in response to a click changes its selection in the middle of
            /// a frame; when the new row was submitted before the old one, no row is selected in that frame.
            /// The highlight then stays where it was instead of vanishing and jumping.
            Rect HighlightTarget;
            uint64_t LastSelectionFrame;
            bool HasHighlightTarget;
            /// The unselected row under the pointer, relative to the content, for the hover tint.
            Rect HoverRect;
            bool HasHoveredRow;
            /// A row of this list was dragged over it during the last frame.
            bool WasDropTarget;
            /// A row of this list was being dragged during the last frame.
            bool WasDragging;
            /// Rows slide into their places for a moment after a drag; the next frame starts the slide.
            bool IsSettling;
            bool IsSettleStart;
            float SettleTime;
        };

        BuildState<SelectionListBuild> s_Build("Carbon.SelectionList.Build");

        SelectionListBuild& GetBuild()
        {
            return s_Build.Get();
        }

        bool IsRowVisible(const SelectionListBuild& build, float height)
        {
            const float top = GetContentScale().Snap(GetCursorPos().Y);
            return top < build.Clip.GetBottom() && top + height > build.Clip.Y;
        }

        // Reserves the space of `count` rows that are not added one by one, and keeps the bookkeeping of the
        // rows as if they had been: their ordinals, and the selection when it is the row at `selected`.
        void SkipRows(SelectionListBuild& build, int count, float height, int selected)
        {
            if (count <= 0)
                return;
            build.RowCounter += count;
            const Rect block = ReserveRows(count, height, build.Spacing);
            const bool hasSelection = selected >= 0 && selected < count;
            if (hasSelection)
            {
                const float offset = GetRowPitch(height, build.Spacing) * static_cast<float>(selected);
                build.SelectedRect = Rect(block.X, block.Y + offset, block.Width, height);
                build.HasSelection = true;
            }
            if (!IsDisabled())
            {
                if (hasSelection)
                    build.SelectedOrdinal = build.Count + selected;
                build.Count += count;
            }
        }
        // Where a row is drawn while rows move, relative to its place in the layout: rows from the insertion slot on
        // make room for the dragged row, and after a drop every row slides from where it was drawn into its new
        // place.
        float GetRowOffset(SelectionListBuild& build, ID id, const Rect& layout)
        {
            const float top = layout.Y - build.ContentOrigin.Y;
            build.Pitch = GetRowPitch(layout.Height, build.Spacing);
            float target = 0.0f;
            if (build.IsDropTarget)
            {
                if (build.Slot < 0 && build.PointerY < layout.GetCenter().Y)
                {
                    build.Slot = build.RowCounter - 1;
                    build.SlotTop = top;
                }
                if (build.Slot >= 0)
                    target = build.Pitch;
            }
            build.NextTop = top + build.Pitch;
            if (!id.IsValid())
                return 0.0f;

            const ID slide = HashID("##slide", id);
            const ID recordID = HashID("##drawntop", id);
            if (build.IsSettleStart)
            {
                bool isNew = false;
                const float drawn = *GetState<float>(recordID, StateLifetime::Transient, &isNew);
                if (!isNew)
                    SetAnimationValue(slide, drawn - top);
            }
            const float offset = Animate(slide, target, SlideSpring);
            if (build.Drag.Index >= 0)
                *GetState<float>(recordID, StateLifetime::Transient) = top + offset;
            return offset;
        }

        void DrawInsertionLine(const SelectionListBuild& build)
        {
            const float y = build.SlotTop + build.Pitch * 0.5f - build.Spacing * 0.5f;
            const float animated = Animate(HashID("##insertion", build.Id), y, SlideSpring);
            const float lineY = build.ContentOrigin.Y + animated;
            const Color accent = GetStyleColor(StyleColor::Accent);
            DrawList& drawList = GetDrawList();
            const float left = build.Content.X + InsertionCircleRadius + 2.0f;
            drawList.AddCircleStroke(Vec2(left, lineY), InsertionCircleRadius, accent, InsertionLineWidth * 0.75f);
            drawList.AddLine(Vec2(left + InsertionCircleRadius, lineY), Vec2(build.Content.GetRight() - 2.0f, lineY),
                             accent, InsertionLineWidth);
        }
    } // namespace

    void BeginSelectionList(std::string_view id, const SelectionListDescription& description)
    {
        SelectionListBuild& build = s_Build.Begin();
        const ID listID = GetID(id);

        build = SelectionListBuild();
        build.Id = listID;
        build.NameLength = std::min(id.size(), MaxNameLength);
        std::memcpy(build.Name, id.data(), build.NameLength);
        build.RowRadius = description.RowRadius;
        build.BackgroundRadius = description.BackgroundRadius;
        build.AnimatesHighlight = description.AnimatesHighlight;
        build.HasBorder = description.HasBorder;
        build.SelectedOrdinal = -1;
        build.RequestedOrdinal = -1;
        build.TrailingSelected = -1;
        build.IsOpen = true;
        build.Spacing = description.Scroll.Spacing.value_or(GetStyleVar(StyleVar::Spacing));

        BeginScrollView(id, description.Scroll);

        const EdgeInsets& padding = description.Scroll.Padding;
        build.Content = GetContentRect();
        build.Viewport = Rect(build.Content.X - padding.Left, build.Content.Y - padding.Top,
                              build.Content.Width + padding.Left + padding.Right,
                              build.Content.Height + padding.Top + padding.Bottom);
        build.ContentOrigin = GetCursorPos();

        DrawList& drawList = GetDrawList();
        build.Clip = drawList.GetClipRect();
        const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
        const float pixel = GetContentScale().GetPixelSize();
        if (description.Background.has_value())
            drawList.AddSquircle(build.Viewport, *description.Background, description.BackgroundRadius, smoothing);
        if (description.HasBorder)
        {
            drawList.AddSquircleStroke(build.Viewport, GetStyleColor(StyleColor::ControlBorder),
                                       description.BackgroundRadius, pixel, smoothing);
        }
        if (description.HasTrailingSeparator)
        {
            drawList.AddRect(Rect(build.Viewport.GetRight() - pixel, build.Viewport.Y, pixel, build.Viewport.Height),
                             GetStyleColor(StyleColor::Separator));
        }

        // The list is one stop for Tab. With focus, the keyboard moves the selection.
        RegisterFocusable(listID, build.Viewport);
        build.IsFocused = IsFocused(listID) && !IsDisabled();
        build.IsEmphasized = build.IsFocused && IsHostFocused();

        // The highlight is drawn before the rows but its place is known only after them.
        const Color color =
            Animate(HashID("##highlightcolor", listID),
                    GetStyleColor(build.IsEmphasized ? StyleColor::Selection : StyleColor::UnemphasizedSelection),
                    AnimationSpec::Fade(0.15f));
        build.Highlight = drawList.AddDeferredSquircle(color);

        // A row under the pointer is tinted. One tint per list is enough: it moves with the pointer and fades
        // when the pointer leaves. Its place, like the highlight's, is known after the rows.
        build.State = GetState<SelectionListState>(listID, StateLifetime::Persistent);
        SelectionListState& state = *build.State;
        // Reordering: a row of this list being dragged, over it or elsewhere, and rows sliding after a drag.
        build.Reordering = description.Reordering;
        build.Slot = -1;
        if (build.Reordering != SelectionListReordering::None)
        {
            const DragPayload payload = GetDragPayload();
            if (payload.Type == RowDragPayloadType && payload.As<RowDragPayload>().List == listID)
                build.Drag = payload.As<RowDragPayload>();
            const bool isDragging = build.Drag.Index >= 0;
            build.IsDropTarget = build.Reordering == SelectionListReordering::Gap && isDragging && state.WasDropTarget;
            build.PointerY = GetMousePos().Y;
            if (state.WasDragging && !isDragging && !state.IsSettling)
            {
                state.IsSettling = true;
                state.SettleTime = 0.0f;
            }
            state.WasDragging = isDragging;
            if (state.IsSettling)
            {
                state.SettleTime += GetDeltaTime();
                state.IsSettling = state.SettleTime < SettleDuration;
            }
            build.IsSettleStart = state.IsSettleStart;
            state.IsSettleStart = false;
            build.AreRowsMoving = build.Reordering == SelectionListReordering::Gap && (isDragging || state.IsSettling);
        }

        const float hover =
            Animate(HashID("##rowhover", listID), state.HasHoveredRow ? 1.0f : 0.0f, AnimationSpec::Fade(0.12f));
        build.Hover = drawList.AddDeferredSquircle(
            GetStyleColor(StyleColor::Label).WithOpacity(GetStyleVar(StyleVar::HoverAmount) * hover));
    }

    SelectionRow SelectionListRow(ID id, float height, bool isSelected, bool isDisabled)
    {
        SelectionListBuild& build = GetBuild();
        SelectionRow row;
        CB_VERIFY(build.IsOpen, "Rows must be added between the Begin and End calls of their list");
        if (!build.IsOpen)
            return row;
        SelectionListState& state = *build.State;

        row.Id = id;
        row.Index = build.RowCounter++;
        row.IsVisible = IsRowVisible(build, height);
        ItemOptions item;
        item.Width = Size::Fill();
        row.Layout = AllocateItem(Vec2(0.0f, height), item);
        row.Bounds = row.Layout;
        row.IsDragged = build.Drag.Index >= 0 && row.Index == build.Drag.Index;
        if (build.AreRowsMoving)
            row.Bounds = row.Layout.Offset(Vec2(0.0f, GetRowOffset(build, id, row.Layout)));

        if (row.IsVisible)
        {
            ButtonBehaviorOptions behavior;
            behavior.Focusable = false;
            behavior.Disabled = isDisabled;
            // As in macOS lists, a row is selected when the mouse button goes down, not when it is released.
            behavior.ActivateOnPress = true;
            row.Interaction = ButtonBehavior(id, row.Layout, behavior);
            row.Clicked = row.Interaction.Clicked;
            if (row.Clicked)
            {
                SetFocus(build.Id);
                // Picking a row of the list that leads to a collapsed navigation's content shows the content.
                NotifyNavigationChoice();
            }
        }
        else
        {
            // Nothing can point at a row outside the visible area. Whatever follows it must not take the row
            // before it for the last item.
            SetLastItem(id, row.Layout, row.Interaction);
        }

        if (!isDisabled && !IsDisabled())
        {
            const int ordinal = build.Count++;
            row.Ordinal = ordinal;
            if (state.PendingOrdinal == ordinal + 1)
            {
                row.Clicked = true;
                state.PendingOrdinal = 0;
            }
            if (isSelected)
                build.SelectedOrdinal = ordinal;
        }
        if (isSelected)
        {
            build.SelectedRect = row.Bounds;
            build.HasSelection = true;
            row.IsEmphasized = build.IsEmphasized;
        }
        else if (row.Interaction.Hovered)
        {
            build.HoveredRect = row.Bounds;
            build.HasHoveredRow = true;
        }
        return row;
    }

    bool IsNextSelectionListRowVisible(float height)
    {
        return IsRowVisible(GetBuild(), height);
    }

    RowRange ClipSelectionListRows(int count, float height, int selected)
    {
        SelectionListBuild& build = GetBuild();
        RowRange range;
        CB_VERIFY(build.IsOpen, "Rows must be declared between the Begin and End calls of their list");
        if (!build.IsOpen || count <= 0)
            return range;

        range = GetVisibleRows(count, height, build.Spacing, build.Clip);
        // The row the keyboard moved to reports being picked, so it is added wherever it is. The rows between
        // it and the visible ones come along; they are outside the visible area and cost little.
        const int pending = build.State->PendingOrdinal - 1 - build.Count;
        if (pending >= 0 && pending < count)
        {
            range.First = std::min(range.First, pending);
            range.End = std::max(range.End, pending + 1);
        }
        // So is the row being dragged, which shows the preview.
        const int dragged = build.Drag.Index - build.RowCounter;
        if (build.Drag.Index >= 0 && dragged >= 0 && dragged < count)
        {
            range.First = std::min(range.First, dragged);
            range.End = std::max(range.End, dragged + 1);
        }

        SkipRows(build, range.First, height, selected);
        build.TrailingRows = count - range.End;
        build.TrailingHeight = height;
        build.TrailingSelected = selected >= range.End ? selected - range.End : -1;
        return range;
    }

    Rect GetSelectionListContentRect()
    {
        return GetBuild().Content;
    }

    ID GetSelectionListID()
    {
        return GetBuild().Id;
    }

    bool IsSelectionListFocused()
    {
        return GetBuild().IsFocused;
    }

    void RequestSelectionListPick(int ordinal)
    {
        SelectionListBuild& build = GetBuild();
        if (!build.IsOpen || ordinal < 0)
            return;
        build.RequestedOrdinal = ordinal;
    }

    RowMove EndSelectionList()
    {
        SelectionListBuild& build = GetBuild();
        RowMove move;
        CB_VERIFY(build.IsOpen, "A list was ended that was never begun");
        if (!build.IsOpen)
            return move;
        SelectionListState& state = *build.State;
        SkipRows(build, build.TrailingRows, build.TrailingHeight, build.TrailingSelected);
        build.TrailingRows = 0;

        // A row of this list dropped onto it moves to the insertion slot: the index it has afterwards is the
        // slot's, less one when it came from before the slot.
        if (build.Reordering == SelectionListReordering::Gap)
        {
            DropTargetOptions options;
            options.ShowsHighlight = false;
            const Drop drop = AcceptDrop(HashID("##reorder", build.Id), build.Viewport, RowDragPayloadType, options);
            state.WasDropTarget = drop.IsHovered && build.Drag.Index >= 0;
            if (build.IsDropTarget)
            {
                if (build.Slot < 0)
                {
                    build.Slot = build.RowCounter;
                    build.SlotTop = build.NextTop;
                }
                if (!drop.IsDelivered)
                    DrawInsertionLine(build);
            }
            if (drop.IsDelivered && build.IsDropTarget)
            {
                const int from = build.Drag.Index;
                const int to = build.Slot > from ? build.Slot - 1 : build.Slot;
                if (to != from)
                {
                    move.From = from;
                    move.To = to;
                    state.IsSettleStart = true;
                    RequestAnimationFrame();
                }
            }
        }

        DrawList& drawList = GetDrawList();
        const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
        const uint64_t frame = GetFrameCount();

        // The highlight is animated relative to the content, so that scrolling does not leave it behind.
        const ID animation = HashID("##highlight", build.Id);
        const bool isContinuing = state.HasHighlightTarget && state.LastSelectionFrame + 2 >= frame;
        if (build.HasSelection)
        {
            const Rect local = build.SelectedRect.Offset(Vec2() - build.ContentOrigin);
            // A selection that appears out of nowhere is simply there; one that moves, slides.
            if (!isContinuing || !build.AnimatesHighlight)
                SetAnimationValue(animation, local);
            state.HighlightTarget = local;
            state.HasHighlightTarget = true;
            state.LastSelectionFrame = frame;
        }
        if (build.HasSelection || (isContinuing && state.LastSelectionFrame + 1 == frame))
        {
            const Rect animated = Animate(animation, state.HighlightTarget, HighlightSpring);
            drawList.ResolveDeferredSquircle(build.Highlight, animated.Offset(build.ContentOrigin), build.RowRadius,
                                             smoothing);
        }

        if (build.HasHoveredRow)
            state.HoverRect = build.HoveredRect.Offset(Vec2() - build.ContentOrigin);
        state.HasHoveredRow = build.HasHoveredRow;
        drawList.ResolveDeferredSquircle(build.Hover, state.HoverRect.Offset(build.ContentOrigin), build.RowRadius,
                                         smoothing);

        // A row that no longer exists cannot be picked.
        state.PendingOrdinal = 0;

        // The keyboard moves the selection from where it is now, which is known only after the rows. The row it
        // moves to reports being picked during the next frame, and is scrolled into view in the one after.
        if (build.IsFocused && build.Count > 0)
        {
            const int last = build.Count - 1;
            const bool hasCurrent = build.SelectedOrdinal >= 0;
            int target = -1;
            if (IsKeyPressed(Key::DownArrow))
                target = hasCurrent ? std::min(build.SelectedOrdinal + 1, last) : 0;
            if (IsKeyPressed(Key::UpArrow))
                target = hasCurrent ? std::max(build.SelectedOrdinal - 1, 0) : last;
            if (IsKeyPressed(Key::Home, false))
                target = 0;
            if (IsKeyPressed(Key::End, false))
                target = last;
            if (target >= 0 && target != build.SelectedOrdinal)
            {
                state.PendingOrdinal = target + 1;
                state.RevealFrame = GetFrameCount() + 2;
                RequestAnimationFrame();
            }
        }
        if (build.RequestedOrdinal >= 0 && build.RequestedOrdinal < build.Count)
        {
            state.PendingOrdinal = build.RequestedOrdinal + 1;
            state.RevealFrame = GetFrameCount() + 2;
            RequestAnimationFrame();
        }

        EndScrollView();

        const std::string_view name(build.Name, build.NameLength);
        if (state.RevealFrame == GetFrameCount() && build.HasSelection)
        {
            // The keyboard moved the selection: scroll just far enough to show it.
            Vec2 offset = GetScrollOffset(name);
            const float top = build.Viewport.Y + RevealMargin;
            const float bottom = build.Viewport.GetBottom() - RevealMargin;
            if (build.SelectedRect.Y < top)
                offset.Y -= top - build.SelectedRect.Y;
            else if (build.SelectedRect.GetBottom() > bottom)
                offset.Y += build.SelectedRect.GetBottom() - bottom;
            SetScrollOffset(name, offset, true);
        }

        if (build.HasBorder)
            DrawFocusRing(build.Id, build.Viewport, build.BackgroundRadius);
        build.IsOpen = false;
        return move;
    }

    bool BeginSelectionListRowDrag(const SelectionRow& row, int64_t key)
    {
        const SelectionListBuild& build = GetBuild();
        if (!build.IsOpen || build.Reordering == SelectionListReordering::None || !row.Id.IsValid())
            return false;
        if (!BeginDragSource(row.Id, row.Layout))
            return false;
        RowDragPayload payload;
        payload.List = build.Id;
        payload.Index = row.Index;
        payload.Key = key;
        SetDragPayload(RowDragPayloadType, payload);
        return true;
    }

    void EndSelectionListRowDrag()
    {
        EndDragSource();
    }

    RowDragPayload GetSelectionListDrag()
    {
        return GetBuild().Drag;
    }

    bool IsSelectionListDragging()
    {
        return GetBuild().Drag.Index >= 0;
    }

    Rect GetSelectionListViewport()
    {
        return GetBuild().Viewport;
    }
} // namespace Carbon::Internal
