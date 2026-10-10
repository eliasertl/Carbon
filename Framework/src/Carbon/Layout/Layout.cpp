#include "Carbon/Layout/Layout.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Animation/Easing.h"
#include "Carbon/Animation/Spring.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/Hash.h"
#include "Carbon/Core/Log.h"
#include "Carbon/Core/State.h"
#include "Carbon/Layout/LayoutInternal.h"
#include "Carbon/Layout/Stack.h"
#include "Carbon/Overlay/OverlayInternal.h"

namespace Carbon::Internal
{
    namespace
    {
        // The interface moves up on this spring for an on-screen keyboard, leaving this much room above it.
        constexpr float KeyboardPanResponse = 0.3f;
        constexpr float KeyboardPanMargin = 12.0f;

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
            /// Inside a grid row: takes the width of its cell. Its column then measures `FitWidth`, the width of its
            /// content, so that a column of such cells is as wide as its widest content.
            bool FillsCell = false;
            float FitWidth = 0.0f;
        };

        // Where a column starts, relative to the row's content area, from last frame's column widths.
        float GetColumnOffset(const GridFrame& grid, uint32_t column)
        {
            float offset = 0.0f;
            for (uint32_t i = 0; i < column && i < MaxGridColumns; i++)
                offset += grid.Record->ColumnWidths[i] + grid.HorizontalSpacing;
            return offset;
        }

        // The column the next cell of the open row goes to, and how many columns it covers.
        uint32_t GetCellColumn(const GridFrame& grid)
        {
            return std::min(grid.Column, MaxGridColumns - 1);
        }

        uint32_t GetCellSpan(const GridFrame& grid)
        {
            return std::clamp(grid.NextSpan, 1u, MaxGridColumns - GetCellColumn(grid));
        }

        float GetCellWidth(const GridFrame& grid)
        {
            const uint32_t column = GetCellColumn(grid);
            const uint32_t end = column + GetCellSpan(grid);
            return GetColumnOffset(grid, end) - GetColumnOffset(grid, column) - grid.HorizontalSpacing;
        }

        ItemFlow ResolveItem(const LayoutFrame& parent, const GridFrame* grid, Size width, Size height, Vec2 fitSize)
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
                        if (axis == parent.Axis && grid != nullptr)
                        {
                            flow.FillsCell = true;
                            flow.FitWidth = fit[i];
                            resolved[i] = std::max(GetCellWidth(*grid), 0.0f);
                        }
                        else if (axis == parent.Axis)
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

        // In a wrapping stack, starts a new line when an item of `size` does not fit on the current one.
        void BreakLine(LayoutFrame& parent, Vec2 size)
        {
            if (!parent.Wraps || parent.HasCursorOverride || parent.ItemCount == 0)
                return;
            if (parent.Cursor <= parent.ContentStart || parent.Cursor + size.X <= parent.Inner.GetRight() + 0.5f)
                return;
            parent.LineTop += parent.LineHeight + parent.Spacing;
            parent.Cursor = parent.ContentStart;
        }

        // Where the next item of `size` goes. Origins are snapped to whole pixels so edges and text stay crisp.
        Vec2 PlaceItem(const Context& context, const LayoutFrame& parent, const GridFrame* grid, Vec2 size)
        {
            if (parent.HasCursorOverride)
                return context.Scale.Snap(parent.CursorOverride);

            // A line of a wrapping stack is as tall as the stack's tallest item; items are aligned in it.
            if (parent.Wraps)
                return context.Scale.Snap(
                    Vec2(parent.Cursor, parent.LineTop + (parent.LineHeight - size.Y) * parent.CrossFactor));

            if (grid != nullptr)
            {
                // A grid cell: aligned inside its column(s) horizontally and inside the row vertically.
                const uint32_t column = GetCellColumn(*grid);
                const float horizontal = grid->NextAlignmentFactor.value_or(
                    column < grid->ColumnFactorCount ? grid->ColumnFactors[column] : grid->AlignmentFactor);
                const float vertical = grid->NextVerticalFactor.value_or(grid->RowVerticalFactor);
                const float x =
                    parent.Inner.X + GetColumnOffset(*grid, column) + (GetCellWidth(*grid) - size.X) * horizontal;
                const float y = parent.Inner.Y + (parent.Inner.Height - size.Y) * vertical;
                return context.Scale.Snap(Vec2(x, y));
            }

            const float crossStart = GetCross(parent.Inner.GetMin(), parent.Axis);
            const float crossSize = GetCross(parent.Inner.GetSize(), parent.Axis);
            const float cross = crossStart + (crossSize - GetCross(size, parent.Axis)) * parent.CrossFactor;
            return context.Scale.Snap(MakeVec(parent.Axis, parent.Cursor, cross));
        }

        // Records a grid cell in its grid's column measurements and moves the row on to the next column.
        void CommitCell(LayoutFrame& row, GridFrame& grid, Vec2 origin, const ItemFlow& flow)
        {
            const uint32_t column = GetCellColumn(grid);
            const uint32_t span = GetCellSpan(grid);
            const float measured = flow.FillsCell ? flow.FitWidth : flow.Size.X;
            if (span == 1)
                grid.Widths[column] = std::max(grid.Widths[column], measured);
            else if (grid.SpanCount < MaxGridSpans)
                grid.Spans[grid.SpanCount++] = GridSpan{column, span, measured};
            grid.ColumnCount = std::max(grid.ColumnCount, column + span);

            const float end = GetColumnOffset(grid, column + span);
            row.MainExtent = std::max(row.MainExtent, end - grid.HorizontalSpacing);
            if (!flow.FillsCross)
                row.CrossExtent = std::max(row.CrossExtent, flow.Size.Y);
            row.Cursor = row.Inner.X + end;
            row.ItemCount++;
            row.LastItem = Rect(origin, flow.Size);

            grid.Column = column + span;
            grid.NextSpan = 1;
            grid.NextAlignmentFactor.reset();
            grid.NextVerticalFactor.reset();
        }

        bool FitsMain(const LayoutFrame& frame)
        {
            return frame.Axis == Axis::Horizontal ? frame.FitsWidth : frame.FitsHeight;
        }

        bool FitsCross(const LayoutFrame& frame)
        {
            return frame.Axis == Axis::Horizontal ? frame.FitsHeight : frame.FitsWidth;
        }

        // ---- The first frame of a new container ----------------------------------------------------------------
        //
        // A new container has no measurements, so whatever depends on them is laid out against zero: alignment
        // across an axis that fits the content, items that fill such an axis, and flexible items along an axis of
        // given length. Instead of hiding every new container for a frame, the container is drawn and the
        // placements of its items are checked; only when one was wrong is the container hidden at End, and it
        // fades in from the next frame. Across a fitting axis the container's extent grows with each item, so a
        // stack whose largest item comes first is right at once.

        void GrowCross(LayoutFrame& frame, float cross)
        {
            if (frame.Axis == Axis::Horizontal)
                frame.Inner.Height = std::max(frame.Inner.Height, cross);
            else
                frame.Inner.Width = std::max(frame.Inner.Width, cross);
        }

        // Called before an item is placed in a container whose first frame is checked. `isDrawn` is false for items
        // that draw nothing (spacers) or that are hidden in this frame anyway.
        void CheckFirstFramePlacement(LayoutFrame& parent, const GridFrame* grid, const ItemFlow& flow, bool isDrawn)
        {
            if (!parent.ChecksFirstFrame || parent.HasCursorOverride || grid != nullptr)
                return;
            // Along an axis of given length, flexible items share free space computed from measurements.
            if (flow.IsFlexible && !FitsMain(parent))
                parent.IsProvisional = true;
            if (!isDrawn || !FitsCross(parent))
                return;
            if (!flow.FillsCross)
                GrowCross(parent, GetCross(flow.Size, parent.Axis));
            if (flow.FillsCross || parent.CrossFactor != 0.0f)
                parent.MinUsedCross = std::min(parent.MinUsedCross, GetCross(parent.Inner.GetSize(), parent.Axis));
        }

        // A new container whose own layout is known to need measurements before anything is placed in it: it is
        // hidden for its first frame, as before. `flow` is how it is placed in `parent`.
        bool NeedsMeasurementsToDraw(const ContainerDescription& description, const LayoutFrame& parent,
                                     const GridFrame* grid, const ItemFlow& flow)
        {
            // Overlays are placed from their measured size; grid columns are measured.
            if (description.IsFloating || description.Kind == ContainerKind::Grid ||
                description.Kind == ContainerKind::GridRow || grid != nullptr)
                return true;
            // Justified content is offset by free space computed from the content's measured length.
            const bool fitsMain =
                (description.Axis == Axis::Horizontal ? description.Width : description.Height).Mode == SizeMode::Fit;
            if (!fitsMain && !description.IsScrolling && description.JustifyFactor != 0.0f)
                return true;
            // Aligned across its parent's axis by its own size, which is not known before End.
            const Size crossSize = parent.Axis == Axis::Horizontal ? description.Height : description.Width;
            return !parent.HasCursorOverride && parent.CrossFactor != 0.0f && !flow.FillsCross &&
                   crossSize.Mode == SizeMode::Fit;
        }

        // Advances the cursor past an item and adds it to the container's measurements.
        void CommitItem(LayoutFrame& parent, GridFrame* grid, Vec2 origin, const ItemFlow& flow)
        {
            if (grid != nullptr && !parent.HasCursorOverride)
            {
                CommitCell(parent, *grid, origin, flow);
                return;
            }

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
            else if (parent.Wraps)
            {
                // The content is as wide as its longest line and as tall as its lines.
                parent.MainExtent = std::max(parent.MainExtent, origin.X + measuredMain - parent.ContentStart);
                if (!flow.FillsCross)
                    parent.LineHeight = std::max(parent.LineHeight, flow.Size.Y);
                parent.CrossExtent = std::max(parent.CrossExtent, parent.LineTop - parent.Inner.Y + parent.LineHeight);
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

        std::string_view GetBeginFunctionName(ContainerKind kind)
        {
            switch (kind)
            {
                case ContainerKind::HStack:
                    return "BeginHStack";
                case ContainerKind::Grid:
                    return "BeginGrid";
                case ContainerKind::GridRow:
                    return "BeginGridRow";
                default:
                    return "BeginVStack";
            }
        }

        // Several containers begun from one call site inside the same container and ID scope during one frame share
        // an identity and are told apart only by their order: what a helper function or a loop without PushID
        // does. Their measurements then move to another container whenever one before it disappears. Reported
        // once per call site, with the fix.
        void ReportRepeatedCallSite(Context& context, ContainerKind kind, const std::source_location& location)
        {
            const uint64_t key = HashCombine(reinterpret_cast<uintptr_t>(location.file_name()),
                                             (static_cast<uint64_t>(location.line()) << 32) | location.column());
            std::vector<uint64_t>& reported = context.Layout.ReportedCallSites;
            if (std::find(reported.begin(), reported.end(), key) != reported.end())
                return;
            reported.push_back(key);
            const std::string_view function = GetBeginFunctionName(kind);
            CB_LOG_WARNING("Layout",
                           "{}:{}: {} was called more than once from this line inside the same container during one "
                           "frame, from a loop or from a function called several times. These containers share an "
                           "identity and are told apart only by their order, so their layout moves to another one "
                           "when one before it disappears. Give each its own identity: put PushID(key) / PopID() "
                           "around each call, or pass a unique .ID in the options of {}.",
                           location.file_name(), location.line(), function, function);
        }

        bool StoreMeasurements(const LayoutFrame& frame, ContainerRecord& record)
        {
            const Vec2 content = MakeVec(frame.Axis, frame.MainExtent, frame.CrossExtent);
            const bool changed = std::abs(content.X - record.ContentSize.X) > SettleTolerance ||
                                 std::abs(content.Y - record.ContentSize.Y) > SettleTolerance ||
                                 std::abs(frame.FlexWeight - record.FlexWeight) > SettleTolerance ||
                                 std::abs(frame.LineHeight - record.LineHeight) > SettleTolerance;
            record.ContentSize = content;
            record.FlexWeight = frame.FlexWeight;
            record.LineHeight = frame.LineHeight;
            return changed;
        }
    } // namespace

    void BeginLayout(Context& context)
    {
        LayoutState& layout = context.Layout;
        layout.Frames.clear();
        layout.ScrollFrames.clear();
        layout.GridFrames.clear();

        LayoutFrame root;
        root.Kind = ContainerKind::Root;
        root.Axis = Axis::Vertical;
        root.Id = HashID("Carbon.Root");
        root.Record = GetState<ContainerRecord>(root.Id, StateLifetime::Transient);
        root.Record->LastFrame = context.FrameCount;
        root.ResolvedSize = context.DisplaySize;
        root.Spacing = context.Style.GetVar(StyleVar::Spacing);
        // Content stays inside the safe area; the interface moves up while an on-screen keyboard would cover the
        // text being edited.
        root.Padding = context.HostIO.GetSafeAreaInsets();
        SpringState pan;
        pan.Value = layout.KeyboardPanShown;
        pan.Velocity = layout.KeyboardPanVelocity;
        if (context.ReduceMotion)
            pan = SpringState{layout.KeyboardPan, 0.0f};
        else
            pan = AdvanceSpring(pan, layout.KeyboardPan, KeyboardPanResponse, 1.0f, context.DeltaTime);
        if (IsSpringAtRest(pan, layout.KeyboardPan))
            pan = SpringState{layout.KeyboardPan, 0.0f};
        else
            context.IsAnimatingThisFrame = true;
        layout.KeyboardPanShown = pan.Value;
        layout.KeyboardPanVelocity = pan.Velocity;
        root.Origin = Vec2(0.0f, -context.Scale.Snap(pan.Value));
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
        layout.GridFrames.clear();

        if (!layout.Frames.empty())
        {
            LayoutFrame& root = layout.Frames.back();
            if (StoreMeasurements(root, *root.Record))
                context.IsAnimatingThisFrame = true;
        }

        // The interface moves up just enough for the caret of the text being edited to clear an on-screen keyboard,
        // once the scroll views have had their chance to bring it into view.
        const Rect keyboard = context.HostIO.GetKeyboardRect();
        const InteractionState& interaction = context.Interaction;
        float panTarget = 0.0f;
        if (interaction.IsTextInputActive && keyboard.Width > 0.0f)
        {
            const float caretBottom = interaction.TextInputCaretRect.GetBottom() + layout.KeyboardPanShown;
            panTarget = std::clamp(caretBottom + KeyboardPanMargin - keyboard.Y, 0.0f, keyboard.Height);
            if (interaction.KeyboardRevealFrames > 0)
                panTarget = std::min(panTarget, layout.KeyboardPan);
        }
        if (panTarget != layout.KeyboardPan)
        {
            layout.KeyboardPan = panTarget;
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
        // ID derived from its position in that sequence. The first one's record counts them.
        ID id = description.Id;
        bool created = false;
        ContainerRecord* record = GetState<ContainerRecord>(id, StateLifetime::Transient, &created);
        if (record->LastFrame == context.FrameCount)
        {
            // The second container from the call site is the one to report; later ones need not look again.
            if (description.CallSite != nullptr && record->Occurrences == 1)
                ReportRepeatedCallSite(context, description.Kind, *description.CallSite);
            const uint64_t occurrence = record->Occurrences++;
            id = ID{HashCombine(description.Id.Value, occurrence)};
            record = GetState<ContainerRecord>(id, StateLifetime::Transient, &created);
        }
        else
        {
            record->Occurrences = 1;
        }
        record->LastFrame = context.FrameCount;

        LayoutFrame& parent = layout.Frames.back();
        const GridFrame* grid = FindGrid(layout, parent);
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
            flow = ResolveItem(parent, grid, description.Width, description.Height, record->ContentSize + padding);
        }

        // Whether a new container can be drawn in its first frame (see CheckFirstFramePlacement).
        const bool isParentAppearing = parent.IsAppearing && !description.IsFloating;
        const bool hidesFirstFrame =
            created && !isParentAppearing && NeedsMeasurementsToDraw(description, parent, grid, flow);
        if (!description.IsFloating)
        {
            CheckFirstFramePlacement(parent, grid, flow, !hidesFirstFrame);
            BreakLine(parent, flow.Size);
        }

        LayoutFrame frame;
        frame.Kind = description.Kind;
        frame.Axis = description.Axis;
        frame.Id = id;
        frame.Record = record;
        frame.Origin = description.IsFloating ? context.Scale.Snap(description.FloatingOrigin)
                                              : PlaceItem(context, parent, grid, flow.Size);
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
        frame.FillsParentCell = flow.FillsCell;
        StartFlow(frame, *record, description.JustifyFactor, description.IsScrolling, description.ScrollOffset);
        // Wrapping needs a width to wrap at; a stack that fits its content has none.
        frame.Wraps = description.Wraps && description.Axis == Axis::Horizontal && !frame.FitsWidth;
        frame.LineTop = frame.Inner.Y;
        frame.LineHeight = frame.Wraps ? record->LineHeight : 0.0f;

        // A container seen for the first time lays out with empty measurements. When that is known to put its
        // content in the wrong place, it is hidden for that frame and fades in afterwards; otherwise it is drawn
        // and checked (see CheckFirstFramePlacement). Nested new containers fade together with the outermost one;
        // a floating container has left its parent's opacity behind, so it always fades in on its own.
        record->AppearTime = created ? 0.0f : std::min(record->AppearTime + context.DeltaTime, AppearFadeDuration);
        frame.IsAppearing = isParentAppearing;
        if (created && !hidesFirstFrame && !isParentAppearing)
        {
            frame.ChecksFirstFrame = true;
            frame.FirstVertex = context.Draw.GetVertices().size();
        }
        else if (!isParentAppearing && record->AppearTime < AppearFadeDuration)
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
            // A square background that reaches the safe area's edge continues to the display's edge, under the
            // system's bars and around a notch.
            const Rect background = frame.BackgroundRadius <= 0.0f ? ExtendToDisplayEdges(rect) : rect;
            context.Draw.ResolveDeferredSquircle(frame.Background, background, frame.BackgroundRadius,
                                                 context.Style.GetVar(StyleVar::CornerSmoothing));
        }
        if (frame.HasOpacity)
            context.Draw.PopOpacity();

        // A new container that was drawn stays visible only if everything in it was placed right.
        if (frame.ChecksFirstFrame)
        {
            const bool isProvisional = frame.IsProvisional || frame.MinUsedCross < frame.CrossExtent - SettleTolerance;
            if (isProvisional)
            {
                context.Draw.HideSince(frame.FirstVertex);
                context.IsAnimatingThisFrame = true;
            }
            else
            {
                frame.Record->AppearTime = AppearFadeDuration;
            }
        }

        if (frame.IsFloating)
            return rect;

        ItemFlow flow;
        flow.Size = size;
        flow.IsFlexible = frame.IsFlexible;
        flow.FlexWeight = frame.ParentFlexWeight;
        flow.FillsCross = frame.FillsParentCross;
        flow.FillsCell = frame.FillsParentCell;
        flow.FitWidth = frame.Record->ContentSize.X + frame.Padding.GetHorizontal();
        GridFrame* grid = FindGrid(layout, parent);
        // Items after it are aligned against its final size.
        if (parent.ChecksFirstFrame && !parent.HasCursorOverride && grid == nullptr && FitsCross(parent) &&
            !flow.FillsCross)
            GrowCross(parent, GetCross(size, parent.Axis));
        CommitItem(parent, grid, frame.Origin, flow);
        return rect;
    }

    GridFrame* FindGrid(LayoutState& layout, const LayoutFrame& parent)
    {
        if (parent.Kind != ContainerKind::GridRow || layout.GridFrames.empty())
            return nullptr;
        // Only a row directly inside the innermost grid places cells; anywhere else it is a plain row.
        GridFrame& grid = layout.GridFrames.back();
        const size_t rowIndex = grid.FrameIndex + 1;
        if (rowIndex >= layout.Frames.size() || &layout.Frames[rowIndex] != &parent)
            return nullptr;
        return &grid;
    }

    float GetAlignmentFactor(Alignment alignment)
    {
        switch (alignment)
        {
            case Alignment::Leading:
                return 0.0f;
            case Alignment::Center:
                return 0.5f;
            case Alignment::Trailing:
                return 1.0f;
        }
        return 0.0f;
    }

    float GetAlignmentFactor(VerticalAlignment alignment)
    {
        switch (alignment)
        {
            case VerticalAlignment::Top:
                return 0.0f;
            case VerticalAlignment::Center:
                return 0.5f;
            case VerticalAlignment::Bottom:
                return 1.0f;
        }
        return 0.0f;
    }

    void SetCallSiteID(Context& context, ContainerDescription& description, const std::source_location& location)
    {
        uint64_t hash = HashCombine(context.Layout.Frames.back().Id.Value, context.IDStack.back().Value);
        hash = HashCombine(hash, reinterpret_cast<uintptr_t>(location.file_name()));
        hash = HashCombine(hash, (static_cast<uint64_t>(location.line()) << 32) | location.column());
        description.Id = ID{hash == 0 ? 1 : hash};
        description.CallSite = &location;
    }
} // namespace Carbon::Internal

namespace Carbon
{
    using Internal::LayoutFrame;

    Rect AllocateItem(Vec2 size, const ItemOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        LayoutFrame& frame = context.Layout.Frames.back();
        Internal::GridFrame* grid = Internal::FindGrid(context.Layout, frame);
        const Internal::ItemFlow flow = Internal::ResolveItem(frame, grid, options.Width, options.Height, size);
        Internal::CheckFirstFramePlacement(frame, grid, flow, true);
        Internal::BreakLine(frame, flow.Size);
        const Vec2 origin = Internal::PlaceItem(context, frame, grid, flow.Size);
        Internal::CommitItem(frame, grid, origin, flow);
        return Rect(origin, flow.Size);
    }

    Vec2 ResolveItemSize(Vec2 size, const ItemOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        const LayoutFrame& frame = context.Layout.Frames.back();
        const Internal::GridFrame* grid = Internal::FindGrid(context.Layout, frame);
        return Internal::ResolveItem(frame, grid, options.Width, options.Height, size).Size;
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

    EdgeInsets GetSafeAreaInsets()
    {
        return Internal::GetContext().HostIO.GetSafeAreaInsets();
    }

    Rect ExtendToDisplayEdges(const Rect& rect)
    {
        const Context& context = Internal::GetContext();
        const Rect display(Vec2(), context.DisplaySize);
        const Rect safe = display.Inset(context.HostIO.GetSafeAreaInsets());
        const float tolerance = 0.5f;
        float left = rect.X;
        float top = rect.Y;
        float right = rect.GetRight();
        float bottom = rect.GetBottom();
        if (left <= safe.X + tolerance)
            left = std::min(left, display.X);
        if (top <= safe.Y + tolerance + context.Layout.KeyboardPanShown)
            top = std::min(top, display.Y);
        if (right >= safe.GetRight() - tolerance)
            right = std::max(right, display.GetRight());
        if (bottom >= safe.GetBottom() - tolerance - context.Layout.KeyboardPanShown)
            bottom = std::max(bottom, display.GetBottom());
        return Rect::FromMinMax(Vec2(left, top), Vec2(right, bottom));
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
        Internal::GridFrame* grid = Internal::FindGrid(context.Layout, frame);
        Internal::CheckFirstFramePlacement(frame, grid, flow, false);
        const Vec2 origin = Internal::PlaceItem(context, frame, grid, flow.Size);
        Internal::CommitItem(frame, grid, origin, flow);
    }
} // namespace Carbon
