#include "Carbon/Extensions/SplitView.h"

#include <algorithm>

namespace Carbon
{
    namespace
    {
        constexpr int MaxSplitDepth = 8;
        // The divider is a hairline, but it can be grabbed this far to either side of it.
        constexpr float GrabMargin = 4.0f;
        constexpr float KeyboardStep = 12.0f;

        // Remembered per split view, also while it is not shown.
        struct SplitViewState
        {
            float Size;
            float DragStartSize;
            bool IsInitialized;
        };

        // A split view that is open.
        struct SplitLevel
        {
            ID Id;
            Axis Direction;
            float MinSize;
            float MaxSize;
            /// False during the first frame, while the split view has not been measured.
            bool HasLength;
            bool HasDivider;
        };

        struct SplitStack
        {
            uint64_t Frame;
            int Depth;
            SplitLevel Levels[MaxSplitDepth];
        };

        SplitStack& GetStack()
        {
            SplitStack& stack = *GetState<SplitStack>(HashID("Carbon.SplitView.Stack"), StateLifetime::Persistent);
            if (stack.Frame != GetFrameCount())
            {
                stack.Frame = GetFrameCount();
                stack.Depth = 0;
            }
            return stack;
        }
    } // namespace

    void BeginSplitView(std::string_view id, const SplitViewOptions& options)
    {
        SplitStack& stack = GetStack();
        const bool hasRoom = stack.Depth < MaxSplitDepth;
        CB_VERIFY(hasRoom, "Split views can be nested at most {} deep", MaxSplitDepth);

        const ID splitID = GetID(id);
        SplitViewState& state = *GetState<SplitViewState>(splitID, StateLifetime::Persistent);
        if (!state.IsInitialized)
        {
            state.Size = options.InitialSize;
            state.IsInitialized = true;
        }

        const bool isHorizontal = options.Axis == Axis::Horizontal;
        if (isHorizontal)
            BeginHStack({.Spacing = 0.0f,
                         .Alignment = VerticalAlignment::Top,
                         .Width = options.Width,
                         .Height = options.Height,
                         .ID = id});
        else
            BeginVStack({.Spacing = 0.0f, .Width = options.Width, .Height = options.Height, .ID = id});
        PushID(splitID);

        // The first pane keeps its length within what both minimums allow.
        const Rect content = GetContentRect();
        const float total = isHorizontal ? content.Width : content.Height;
        const float maxSize = std::max(options.MinSize, total - options.MinSecondSize);
        if (total > 0.0f)
            state.Size = std::clamp(state.Size, options.MinSize, maxSize);

        if (hasRoom)
        {
            SplitLevel& level = stack.Levels[stack.Depth];
            level.Id = splitID;
            level.Direction = options.Axis;
            level.MinSize = options.MinSize;
            level.MaxSize = maxSize;
            level.HasLength = total > 0.0f;
            level.HasDivider = false;
        }
        stack.Depth++;

        const Size paneSize = Size::Fixed(GetContentScale().Snap(state.Size));
        BeginVStack(
            {.Width = isHorizontal ? paneSize : Size::Fill(), .Height = isHorizontal ? Size::Fill() : paneSize});
    }

    void SplitViewDivider()
    {
        SplitStack& stack = GetStack();
        const bool isOpen = stack.Depth > 0 && stack.Depth <= MaxSplitDepth;
        CB_VERIFY(isOpen, "SplitViewDivider must be called between BeginSplitView and EndSplitView");
        if (!isOpen)
            return;
        SplitLevel& level = stack.Levels[stack.Depth - 1];
        CB_VERIFY(!level.HasDivider, "A split view has exactly one divider");
        if (level.HasDivider)
            return;
        level.HasDivider = true;

        EndVStack();

        SplitViewState& state = *GetState<SplitViewState>(level.Id, StateLifetime::Persistent);
        const bool isHorizontal = level.Direction == Axis::Horizontal;
        const float pixel = GetContentScale().GetPixelSize();

        ItemOptions item;
        (isHorizontal ? item.Height : item.Width) = Size::Fill();
        const Rect line = AllocateItem(isHorizontal ? Vec2(pixel, 0.0f) : Vec2(0.0f, pixel), item);
        const Rect grab = isHorizontal ? Rect(line.X - GrabMargin, line.Y, line.Width + GrabMargin * 2.0f, line.Height)
                                       : Rect(line.X, line.Y - GrabMargin, line.Width, line.Height + GrabMargin * 2.0f);

        const ID id = GetID("##divider");
        const DragInteraction drag = DragBehavior(id, grab);
        if (drag.Started)
            state.DragStartSize = state.Size;
        float size = state.Size;
        if (drag.Active)
            size = state.DragStartSize + (isHorizontal ? drag.Total.X : drag.Total.Y);
        if (drag.Focused)
        {
            if (IsKeyPressed(isHorizontal ? Key::RightArrow : Key::DownArrow))
                size += KeyboardStep;
            if (IsKeyPressed(isHorizontal ? Key::LeftArrow : Key::UpArrow))
                size -= KeyboardStep;
        }
        state.Size = level.HasLength ? std::clamp(size, level.MinSize, level.MaxSize) : size;

        if (drag.Hovered || drag.Active)
            SetCursor(isHorizontal ? Cursor::ResizeHorizontal : Cursor::ResizeVertical);

        GetDrawList().AddRect(line, GetStyleColor(StyleColor::Separator));
        // Under the pointer and while it is dragged the divider shows as a wider bar.
        const ControlFeedback feedback = AnimateFeedback(id, drag.Hovered, drag.Active);
        const float emphasis = std::max(feedback.Hover, feedback.Press);
        if (emphasis > 0.001f)
        {
            const Rect bar = isHorizontal ? Rect(line.X - 1.0f, line.Y, line.Width + 2.0f, line.Height)
                                          : Rect(line.X, line.Y - 1.0f, line.Width, line.Height + 2.0f);
            GetDrawList().AddRect(bar, GetStyleColor(StyleColor::Label).WithOpacity(0.2f * emphasis));
        }
        DrawFocusRing(id,
                      isHorizontal ? Rect(line.X - 1.0f, line.Y, line.Width + 2.0f, line.Height)
                                   : Rect(line.X, line.Y - 1.0f, line.Width, line.Height + 2.0f),
                      1.5f);

        BeginVStack({.Width = Size::Fill(), .Height = Size::Fill()});
    }

    void EndSplitView()
    {
        SplitStack& stack = GetStack();
        const bool isOpen = stack.Depth > 0;
        CB_VERIFY(isOpen, "EndSplitView called without BeginSplitView");
        if (!isOpen)
            return;
        stack.Depth--;
        const bool isHorizontal =
            stack.Depth >= MaxSplitDepth || stack.Levels[stack.Depth].Direction == Axis::Horizontal;

        EndVStack();
        PopID();
        if (isHorizontal)
            EndHStack();
        else
            EndVStack();
    }
} // namespace Carbon
