#include "Carbon/Layout/Grid.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/State.h"
#include "Carbon/Layout/LayoutInternal.h"

namespace Carbon
{
    namespace
    {
        // Column widths that differ by less than this are the same; the grid has settled.
        constexpr float SettleTolerance = 0.01f;

        // Widens the columns under cells that span several of them and are wider than those columns together.
        // The extra width is shared equally, as SwiftUI's Grid does.
        void DistributeSpans(const Internal::GridFrame& grid, float* widths)
        {
            for (uint32_t i = 0; i < grid.SpanCount; i++)
            {
                const Internal::GridSpan& span = grid.Spans[i];
                float covered = grid.HorizontalSpacing * float(span.Count - 1);
                for (uint32_t column = span.Column; column < span.Column + span.Count; column++)
                    covered += widths[column];
                if (span.Width <= covered)
                    continue;
                const float extra = (span.Width - covered) / float(span.Count);
                for (uint32_t column = span.Column; column < span.Column + span.Count; column++)
                    widths[column] += extra;
            }
        }
    } // namespace

    void BeginGrid(const GridOptions& options, const std::source_location& location)
    {
        Context& context = Internal::GetFrameContext();
        const float spacing = context.Style.GetVar(StyleVar::Spacing);

        Internal::ContainerDescription description;
        description.Kind = Internal::ContainerKind::Grid;
        description.Axis = Axis::Vertical;
        description.Id = options.ID.empty() ? Internal::GetCallSiteID(context, location.file_name(), location.line(),
                                                                      location.column())
                                            : GetID(options.ID);
        description.Width = options.Width;
        description.Height = options.Height;
        description.Padding = options.Padding;
        description.Spacing = options.VerticalSpacing.value_or(spacing);
        Internal::LayoutFrame& frame = Internal::BeginContainer(context, description);
        if (options.Background.has_value())
        {
            frame.Background = context.Draw.AddDeferredSquircle(*options.Background);
            frame.BackgroundRadius = options.CornerRadius.value_or(context.Style.GetVar(StyleVar::GroupCornerRadius));
        }

        CB_VERIFY(options.ColumnAlignments.size() <= Internal::MaxGridColumns,
                  "A grid has at most {} columns; {} column alignments were given", Internal::MaxGridColumns,
                  options.ColumnAlignments.size());

        Internal::GridFrame grid;
        grid.FrameIndex = context.Layout.Frames.size() - 1;
        grid.Record = GetState<Internal::GridRecord>(frame.Id, StateLifetime::Transient);
        grid.HorizontalSpacing = options.HorizontalSpacing.value_or(spacing);
        grid.AlignmentFactor = Internal::GetAlignmentFactor(options.Alignment);
        grid.VerticalFactor = Internal::GetAlignmentFactor(options.VerticalAlignment);
        grid.ColumnFactorCount = uint32_t(std::min<size_t>(options.ColumnAlignments.size(), Internal::MaxGridColumns));
        for (uint32_t i = 0; i < grid.ColumnFactorCount; i++)
            grid.ColumnFactors[i] = Internal::GetAlignmentFactor(options.ColumnAlignments[i]);
        context.Layout.GridFrames.push_back(grid);
    }

    void EndGrid()
    {
        Context& context = Internal::GetFrameContext();
        Internal::LayoutState& layout = context.Layout;
        const bool isGrid = !layout.GridFrames.empty() &&
                            layout.GridFrames.back().FrameIndex + 1 == layout.Frames.size() &&
                            layout.Frames.back().Kind == Internal::ContainerKind::Grid;
        if (isGrid)
        {
            const Internal::GridFrame& grid = layout.GridFrames.back();
            float widths[Internal::MaxGridColumns] = {};
            std::copy(std::begin(grid.Widths), std::end(grid.Widths), widths);
            DistributeSpans(grid, widths);

            // The next frame places cells against these widths; if they moved, it must be drawn.
            Internal::GridRecord& record = *grid.Record;
            bool changed = record.ColumnCount != grid.ColumnCount;
            for (uint32_t i = 0; i < Internal::MaxGridColumns; i++)
            {
                changed = changed || std::abs(record.ColumnWidths[i] - widths[i]) > SettleTolerance;
                record.ColumnWidths[i] = widths[i];
            }
            record.ColumnCount = grid.ColumnCount;
            if (changed)
                context.IsAnimatingThisFrame = true;

            layout.GridFrames.pop_back();
        }
        Internal::EndContainer(context, Internal::ContainerKind::Grid);
    }

    void BeginGridRow(const GridRowOptions& options, const std::source_location& location)
    {
        Context& context = Internal::GetFrameContext();
        Internal::LayoutState& layout = context.Layout;
        const bool isInGrid = !layout.GridFrames.empty() &&
                              layout.GridFrames.back().FrameIndex + 1 == layout.Frames.size() &&
                              layout.Frames.back().Kind == Internal::ContainerKind::Grid;
        CB_VERIFY(isInGrid, "BeginGridRow must be called directly inside BeginGrid");

        Internal::ContainerDescription description;
        description.Kind = Internal::ContainerKind::GridRow;
        description.Axis = Axis::Horizontal;
        description.Id = Internal::GetCallSiteID(context, location.file_name(), location.line(), location.column());
        description.Spacing =
            isInGrid ? layout.GridFrames.back().HorizontalSpacing : context.Style.GetVar(StyleVar::Spacing);
        description.CrossFactor = 0.5f;
        Internal::BeginContainer(context, description);

        if (isInGrid)
        {
            Internal::GridFrame& grid = layout.GridFrames.back();
            grid.Column = 0;
            grid.RowVerticalFactor =
                options.Alignment.has_value() ? Internal::GetAlignmentFactor(*options.Alignment) : grid.VerticalFactor;
            grid.NextSpan = 1;
            grid.NextAlignmentFactor.reset();
            grid.NextVerticalFactor.reset();
        }
    }

    void EndGridRow()
    {
        Internal::EndContainer(Internal::GetFrameContext(), Internal::ContainerKind::GridRow);
    }

    void SetNextGridCell(const GridCellOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        Internal::GridFrame* grid = Internal::FindGrid(context.Layout, context.Layout.Frames.back());
        CB_VERIFY(grid != nullptr, "SetNextGridCell must be called inside a grid row");
        if (grid == nullptr)
            return;

        grid->NextSpan = uint32_t(std::max(options.ColumnSpan, 1));
        if (options.Alignment.has_value())
            grid->NextAlignmentFactor = Internal::GetAlignmentFactor(*options.Alignment);
        if (options.VerticalAlignment.has_value())
            grid->NextVerticalFactor = Internal::GetAlignmentFactor(*options.VerticalAlignment);
    }
} // namespace Carbon
