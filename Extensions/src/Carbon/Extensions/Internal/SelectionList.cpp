#include "Carbon/Extensions/Internal/SelectionList.h"

#include <algorithm>
#include <cstring>

namespace Carbon::Internal
{
    namespace
    {
        constexpr size_t MaxNameLength = 63;
        // Distance kept between a row revealed by the keyboard and the edge of the visible area.
        constexpr float RevealMargin = 4.0f;
        constexpr AnimationSpec HighlightSpring = AnimationSpec::Spring(0.28f, 0.9f);

        // The list whose rows are being added.
        struct SelectionListBuild
        {
            ID Id;
            /// The scroll view's name, to scroll it after it has been ended.
            char Name[MaxNameLength + 1];
            size_t NameLength;
            DeferredShape Highlight;
            Rect Viewport;
            Rect Content;
            /// Where the first row starts; moves with the scroll offset.
            Vec2 ContentOrigin;
            Rect SelectedRect;
            float RowRadius;
            float BackgroundRadius;
            int Count;
            int SelectedOrdinal;
            bool HasSelection;
            bool IsFocused;
            bool IsEmphasized;
            bool AnimatesHighlight;
            bool HasBorder;
            bool IsOpen;
        };

        // Remembered per list.
        struct SelectionListState
        {
            /// The row the keyboard moved to, plus one; it reports being picked. 0 when there is none.
            int PendingOrdinal;
            /// The frame in which the newly selected row is scrolled into view.
            uint64_t RevealFrame;
            bool HadSelection;
        };

        SelectionListBuild& GetBuild()
        {
            return *GetState<SelectionListBuild>(HashID("Carbon.SelectionList.Build"), StateLifetime::Persistent);
        }
    } // namespace

    void BeginSelectionList(std::string_view id, const SelectionListDescription& description)
    {
        SelectionListBuild& build = GetBuild();
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
        build.IsOpen = true;

        BeginScrollView(id, description.Scroll);

        const EdgeInsets& padding = description.Scroll.Padding;
        build.Content = GetContentRect();
        build.Viewport = Rect(build.Content.X - padding.Left, build.Content.Y - padding.Top,
                              build.Content.Width + padding.Left + padding.Right,
                              build.Content.Height + padding.Top + padding.Bottom);
        build.ContentOrigin = GetCursorPos();

        DrawList& drawList = GetDrawList();
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
    }

    SelectionRow SelectionListRow(ID id, float height, bool isSelected, bool isDisabled)
    {
        SelectionListBuild& build = GetBuild();
        SelectionRow row;
        CB_VERIFY(build.IsOpen, "Rows must be added between the Begin and End calls of their list");
        if (!build.IsOpen)
            return row;
        SelectionListState& state = *GetState<SelectionListState>(build.Id, StateLifetime::Persistent);

        ItemOptions item;
        item.Width = Size::Fill();
        row.Bounds = AllocateItem(Vec2(0.0f, height), item);

        ButtonBehaviorOptions behavior;
        behavior.Focusable = false;
        behavior.Disabled = isDisabled;
        row.Interaction = ButtonBehavior(id, row.Bounds, behavior);
        row.Clicked = row.Interaction.Clicked;
        if (row.Clicked)
            SetFocus(build.Id);

        if (!isDisabled && !IsDisabled())
        {
            const int ordinal = build.Count++;
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
        return row;
    }

    Rect GetSelectionListContentRect()
    {
        return GetBuild().Content;
    }

    void EndSelectionList()
    {
        SelectionListBuild& build = GetBuild();
        CB_VERIFY(build.IsOpen, "A list was ended that was never begun");
        if (!build.IsOpen)
            return;
        SelectionListState& state = *GetState<SelectionListState>(build.Id, StateLifetime::Persistent);

        if (build.HasSelection)
        {
            // Animated relative to the content, so that scrolling does not leave the highlight behind.
            const ID animation = HashID("##highlight", build.Id);
            const Rect local = build.SelectedRect.Offset(Vec2() - build.ContentOrigin);
            if (!state.HadSelection || !build.AnimatesHighlight)
                SetAnimationValue(animation, local);
            const Rect animated = Animate(animation, local, HighlightSpring);
            GetDrawList().ResolveDeferredSquircle(build.Highlight, animated.Offset(build.ContentOrigin),
                                                  build.RowRadius, GetStyleVar(StyleVar::CornerSmoothing));
        }
        state.HadSelection = build.HasSelection;
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
    }
} // namespace Carbon::Internal
