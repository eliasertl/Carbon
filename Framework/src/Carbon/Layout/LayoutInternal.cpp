#include "Carbon/Layout/LayoutInternal.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Animation/Easing.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/Hash.h"
#include "Carbon/Core/State.h"
#include "Carbon/Layout/Layout.h"
#include "Carbon/Layout/Stack.h"
#include "Carbon/Overlay/OverlayInternal.h"

namespace Carbon::Internal
{
    namespace
    {
        // Measurements that differ by less than this are the same; layout has settled.
        constexpr float SettleTolerance = 0.01f;

        float GetMain(Vec2 value, Axis axis)
        {
            return axis == Axis::Horizontal ? value.X : value.Y;
        }

        float GetCross(Vec2 value, Axis axis)
        {
            return axis == Axis::Horizontal ? value.Y : value.X;
        }

        Vec2 MakeVec(Axis axis, float main, float cross)
        {
            return axis == Axis::Horizontal ? Vec2(main, cross) : Vec2(cross, main);
        }

        // How an item takes part in its container's flow.
        struct ItemFlow
        {
            Vec2 Size;
            /// Stretches along the container's axis; counts with `FlexMinimum` when the content is measured.
            bool IsFlexible = false;
            float FlexWeight = 0.0f;
            float FlexMinimum = 0.0f;
            /// Takes the container's full extent across the axis, so it must not define that extent.
            bool FillsCross = false;
        };

        ItemFlow ResolveItem(const LayoutFrame& parent, Size width, Size height, Vec2 fitSize)
        {
            ItemFlow flow;
            const Size modes[2] = {width, height};
            const float fit[2] = {fitSize.X, fitSize.Y};
            float resolved[2] = {0.0f, 0.0f};
            for (int i = 0; i < 2; i++)
            {
                const Axis axis = i == 0 ? Axis::Horizontal : Axis::Vertical;
                switch (modes[i].Mode)
                {
                    case SizeMode::Fit:
                        resolved[i] = fit[i];
                        break;
                    case SizeMode::Fixed:
                        resolved[i] = std::max(modes[i].Value, 0.0f);
                        break;
                    case SizeMode::Fill:
                        if (axis == parent.Axis)
                        {
                            flow.IsFlexible = true;
                            flow.FlexWeight = std::max(modes[i].Value, 0.0f);
                            resolved[i] = parent.FlexUnit * flow.FlexWeight;
                        }
                        else
                        {
                            flow.FillsCross = true;
                            resolved[i] = GetCross(parent.Inner.GetSize(), parent.Axis);
                        }
                        break;
                }
            }
            flow.Size = Vec2(resolved[0], resolved[1]);
            return flow;
        }

        // Where the next item of `size` goes. Origins are snapped to whole pixels so edges and text stay crisp.
        Vec2 PlaceItem(const Context& context, const LayoutFrame& parent, Vec2 size)
        {
            if (parent.HasCursorOverride)
                return context.Scale.Snap(parent.CursorOverride);

            const float crossStart = GetCross(parent.Inner.GetMin(), parent.Axis);
            const float crossSize = GetCross(parent.Inner.GetSize(), parent.Axis);
            const float cross = crossStart + (crossSize - GetCross(size, parent.Axis)) * parent.CrossFactor;
            return context.Scale.Snap(MakeVec(parent.Axis, parent.Cursor, cross));
        }

        // Advances the cursor past an item and adds it to the container's measurements.
        void CommitItem(LayoutFrame& parent, Vec2 origin, const ItemFlow& flow)
        {
            const float mainSize = GetMain(flow.Size, parent.Axis);
            const float measuredMain = flow.IsFlexible ? flow.FlexMinimum : mainSize;
            if (parent.HasCursorOverride)
            {
                // An absolutely placed item extends the content to wherever it ends.
                const float crossStart = GetCross(parent.Inner.GetMin(), parent.Axis);
                parent.MainExtent =
                    std::max(parent.MainExtent, GetMain(origin, parent.Axis) - parent.ContentStart + measuredMain);
                if (!flow.FillsCross)
                {
                    parent.CrossExtent = std::max(parent.CrossExtent, GetCross(origin, parent.Axis) - crossStart +
                                                                          GetCross(flow.Size, parent.Axis));
                }
                parent.HasCursorOverride = false;
            }
            else
            {
                if (parent.ItemCount > 0)
                    parent.MainExtent += parent.Spacing;
                parent.MainExtent += measuredMain;
                if (!flow.FillsCross)
                    parent.CrossExtent = std::max(parent.CrossExtent, GetCross(flow.Size, parent.Axis));
            }

            parent.Cursor = GetMain(origin, parent.Axis) + mainSize + parent.Spacing;
            parent.FlexWeight += flow.FlexWeight;
            parent.ItemCount++;
            parent.LastItem = Rect(origin, flow.Size);
        }

        // Computes the content area and the distribution of free space from last frame's measurements.
        void StartFlow(LayoutFrame& frame, const ContainerRecord& record, float justifyFactor, bool isScrolling,
                       Vec2 scrollOffset)
        {
            frame.Inner = Rect(frame.Origin, frame.ResolvedSize).Inset(frame.Padding);

            const bool fitsMain = frame.Axis == Axis::Horizontal ? frame.FitsWidth : frame.FitsHeight;
            const float innerMain = GetMain(frame.Inner.GetSize(), frame.Axis);
            const float contentMain = GetMain(record.ContentSize, frame.Axis);
            // Free space exists only when the container's length is decided from outside.
            const float freeSpace = (fitsMain || isScrolling) ? 0.0f : std::max(0.0f, innerMain - contentMain);

            frame.FlexUnit = record.FlexWeight > 0.0f ? freeSpace / record.FlexWeight : 0.0f;
            const float justifyOffset = record.FlexWeight > 0.0f ? 0.0f : freeSpace * justifyFactor;
            frame.ContentStart =
                GetMain(frame.Inner.GetMin(), frame.Axis) + justifyOffset - GetMain(scrollOffset, frame.Axis);
            frame.Cursor = frame.ContentStart;
        }

        bool StoreMeasurements(const LayoutFrame& frame, ContainerRecord& record)
        {
            const Vec2 content = MakeVec(frame.Axis, frame.MainExtent, frame.CrossExtent);
            const bool changed = std::abs(content.X - record.ContentSize.X) > SettleTolerance ||
                                 std::abs(content.Y - record.ContentSize.Y) > SettleTolerance ||
                                 std::abs(frame.FlexWeight - record.FlexWeight) > SettleTolerance;
            record.ContentSize = content;
            record.FlexWeight = frame.FlexWeight;
            return changed;
        }
    } // namespace

    void BeginLayout(Context& context)
    {
        LayoutState& layout = context.Layout;
        layout.Frames.clear();
        layout.ScrollFrames.clear();

        LayoutFrame root;
        root.Kind = ContainerKind::Root;
        root.Axis = Axis::Vertical;
        root.Id = HashID("Carbon.Root");
        root.Record = GetState<ContainerRecord>(root.Id, StateLifetime::Transient);
        root.Record->LastFrame = context.FrameCount;
        root.ResolvedSize = context.DisplaySize;
        root.Spacing = context.Style.GetVar(StyleVar::Spacing);
        StartFlow(root, *root.Record, 0.0f, false, Vec2());
        layout.Frames.push_back(root);
    }

    void EndLayout(Context& context)
    {
        LayoutState& layout = context.Layout;
        CB_VERIFY(layout.Frames.size() == 1, "Unbalanced layout: {} container(s) were begun but not ended",
                  layout.Frames.size() - 1);
        // Unwind what was left open, so one mistake is reported once and the other stacks stay balanced.
        while (layout.Frames.size() > 1)
        {
            if (layout.Frames.back().Kind == ContainerKind::ScrollView)
            {
                context.Draw.PopClipRect();
                if (context.IDStack.size() > 1)
                    context.IDStack.pop_back();
            }
            if (layout.Frames.back().HasOpacity)
                context.Draw.PopOpacity();
            if (layout.Frames.back().Kind == ContainerKind::Overlay)
                AbandonOverlay(context);
            layout.Frames.pop_back();
        }
        layout.ScrollFrames.clear();

        if (!layout.Frames.empty())
        {
            LayoutFrame& root = layout.Frames.back();
            if (StoreMeasurements(root, *root.Record))
                context.IsAnimatingThisFrame = true;
        }

        layout.HoveredScrollView = layout.HoveredScrollViewCandidate;
        layout.KeyboardScrollView =
            layout.HoveredScrollView.IsValid() ? layout.HoveredScrollView : layout.FirstScrollViewCandidate;
        layout.HoveredScrollViewCandidate = ID();
        layout.FirstScrollViewCandidate = ID();
    }

    LayoutFrame& BeginContainer(Context& context, const ContainerDescription& description)
    {
        LayoutState& layout = context.Layout;

        // A call site inside a loop opens several containers with the same ID; each one after the first gets an
        // ID derived from its position in that sequence.
        ID id = description.Id;
        bool created = false;
        ContainerRecord* record = GetState<ContainerRecord>(id, StateLifetime::Transient, &created);
        for (uint64_t occurrence = 1; record->LastFrame == context.FrameCount; occurrence++)
        {
            id = ID{HashCombine(description.Id.Value, occurrence)};
            record = GetState<ContainerRecord>(id, StateLifetime::Transient, &created);
        }
        record->LastFrame = context.FrameCount;

        const LayoutFrame& parent = layout.Frames.back();
        const Vec2 padding(description.Padding.GetHorizontal(), description.Padding.GetVertical());
        ItemFlow flow;
        if (description.IsFloating)
        {
            // Outside of any flow there is nothing to share: Fill takes the whole display.
            const Vec2 fit = record->ContentSize + padding;
            const auto resolve = [](Size size, float fitLength, float fillLength)
            {
                switch (size.Mode)
                {
                    case SizeMode::Fixed:
                        return std::max(size.Value, 0.0f);
                    case SizeMode::Fill:
                        return fillLength;
                    default:
                        return fitLength;
                }
            };
            flow.Size = Vec2(resolve(description.Width, fit.X, context.DisplaySize.X),
                             resolve(description.Height, fit.Y, context.DisplaySize.Y));
        }
        else
        {
            flow = ResolveItem(parent, description.Width, description.Height, record->ContentSize + padding);
        }

        LayoutFrame frame;
        frame.Kind = description.Kind;
        frame.Axis = description.Axis;
        frame.Id = id;
        frame.Record = record;
        frame.Origin = description.IsFloating ? context.Scale.Snap(description.FloatingOrigin)
                                              : PlaceItem(context, parent, flow.Size);
        frame.IsFloating = description.IsFloating;
        frame.ResolvedSize = flow.Size;
        frame.FitsWidth = description.Width.Mode == SizeMode::Fit;
        frame.FitsHeight = description.Height.Mode == SizeMode::Fit;
        frame.Padding = description.Padding;
        frame.Spacing = description.Spacing;
        frame.CrossFactor = description.CrossFactor;
        frame.IsFlexible = flow.IsFlexible;
        frame.ParentFlexWeight = flow.FlexWeight;
        frame.FillsParentCross = flow.FillsCross;
        StartFlow(frame, *record, description.JustifyFactor, description.IsScrolling, description.ScrollOffset);

        // A container seen for the first time lays out with empty measurements, so it is hidden for that frame
        // and fades in afterwards. Nested new containers fade together with the outermost one.
        record->AppearTime = created ? 0.0f : std::min(record->AppearTime + context.DeltaTime, AppearFadeDuration);
        // A floating container has left its parent's opacity behind, so it always fades in on its own.
        const bool isParentAppearing = parent.IsAppearing && !description.IsFloating;
        frame.IsAppearing = isParentAppearing;
        if (!isParentAppearing && record->AppearTime < AppearFadeDuration)
        {
            const float opacity = created ? 0.0f : Ease(Easing::EaseOut, record->AppearTime / AppearFadeDuration);
            context.Draw.PushOpacity(opacity);
            frame.HasOpacity = true;
            frame.IsAppearing = true;
            context.IsAnimatingThisFrame = true;
        }

        layout.Frames.push_back(frame);
        return layout.Frames.back();
    }

    Rect EndContainer(Context& context, ContainerKind kind)
    {
        LayoutState& layout = context.Layout;
        const bool isOpen = layout.Frames.size() > 1;
        CB_VERIFY(isOpen, "A container was ended that was never begun");
        if (!isOpen)
            return Rect();
        CB_VERIFY(layout.Frames.back().Kind == kind,
                  "Mismatched container: the End call does not match the innermost open Begin call");

        const LayoutFrame frame = layout.Frames.back();
        layout.Frames.pop_back();
        LayoutFrame& parent = layout.Frames.back();

        if (StoreMeasurements(frame, *frame.Record))
            context.IsAnimatingThisFrame = true; // another frame is needed for the layout to settle

        // Axes that fit their content take this frame's measurement; the others were decided at Begin.
        Vec2 size = frame.ResolvedSize;
        if (frame.FitsWidth)
            size.X = frame.Record->ContentSize.X + frame.Padding.GetHorizontal();
        if (frame.FitsHeight)
            size.Y = frame.Record->ContentSize.Y + frame.Padding.GetVertical();
        const Rect rect(frame.Origin, size);

        if (frame.Background.IsValid)
        {
            context.Draw.ResolveDeferredSquircle(frame.Background, rect, frame.BackgroundRadius,
                                                 context.Style.GetVar(StyleVar::CornerSmoothing));
        }
        if (frame.HasOpacity)
            context.Draw.PopOpacity();

        if (frame.IsFloating)
            return rect;

        ItemFlow flow;
        flow.Size = size;
        flow.IsFlexible = frame.IsFlexible;
        flow.FlexWeight = frame.ParentFlexWeight;
        flow.FillsCross = frame.FillsParentCross;
        CommitItem(parent, frame.Origin, flow);
        return rect;
    }

    ID GetCallSiteID(Context& context, const char* file, uint32_t line, uint32_t column)
    {
        uint64_t hash = HashCombine(context.Layout.Frames.back().Id.Value, context.IDStack.back().Value);
        hash = HashCombine(hash, reinterpret_cast<uintptr_t>(file));
        hash = HashCombine(hash, (static_cast<uint64_t>(line) << 32) | column);
        return ID{hash == 0 ? 1 : hash};
    }
} // namespace Carbon::Internal

namespace Carbon
{
    using Internal::LayoutFrame;

    Rect AllocateItem(Vec2 size, const ItemOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        LayoutFrame& frame = context.Layout.Frames.back();
        const Internal::ItemFlow flow = Internal::ResolveItem(frame, options.Width, options.Height, size);
        const Vec2 origin = Internal::PlaceItem(context, frame, flow.Size);
        Internal::CommitItem(frame, origin, flow);
        return Rect(origin, flow.Size);
    }

    Vec2 ResolveItemSize(Vec2 size, const ItemOptions& options)
    {
        const Context& context = Internal::GetFrameContext();
        return Internal::ResolveItem(context.Layout.Frames.back(), options.Width, options.Height, size).Size;
    }

    Axis GetLayoutAxis()
    {
        return Internal::GetFrameContext().Layout.Frames.back().Axis;
    }

    Vec2 GetCursorPos()
    {
        const Context& context = Internal::GetFrameContext();
        const LayoutFrame& frame = context.Layout.Frames.back();
        if (frame.HasCursorOverride)
            return frame.CursorOverride;
        return Internal::MakeVec(frame.Axis, frame.Cursor, Internal::GetCross(frame.Inner.GetMin(), frame.Axis));
    }

    void SetCursorPos(Vec2 position)
    {
        Context& context = Internal::GetFrameContext();
        LayoutFrame& frame = context.Layout.Frames.back();
        frame.HasCursorOverride = true;
        frame.CursorOverride = position;
    }

    Rect GetContentRect()
    {
        return Internal::GetFrameContext().Layout.Frames.back().Inner;
    }

    Rect GetLastItemRect()
    {
        return Internal::GetFrameContext().Layout.Frames.back().LastItem;
    }

    // Declared in Layout/Stack.h.
    void Spacer(const SpacerOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        LayoutFrame& frame = context.Layout.Frames.back();

        Internal::ItemFlow flow;
        // A spacer has no extent across the axis and must not influence it.
        flow.FillsCross = true;
        if (options.Length.has_value())
        {
            flow.Size = Internal::MakeVec(frame.Axis, std::max(*options.Length, 0.0f), 0.0f);
        }
        else
        {
            flow.IsFlexible = true;
            flow.FlexWeight = std::max(options.Weight, 0.0f);
            flow.FlexMinimum = std::max(options.MinLength, 0.0f);
            flow.Size = Internal::MakeVec(frame.Axis, flow.FlexMinimum + frame.FlexUnit * flow.FlexWeight, 0.0f);
        }
        const Vec2 origin = Internal::PlaceItem(context, frame, flow.Size);
        Internal::CommitItem(frame, origin, flow);
    }
} // namespace Carbon
