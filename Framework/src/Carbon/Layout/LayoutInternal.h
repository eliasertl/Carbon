#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "Carbon/Core/EdgeInsets.h"
#include "Carbon/Core/ID.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Core/Vec2.h"
#include "Carbon/Draw/DrawList.h"
#include "Carbon/Layout/Size.h"

namespace Carbon
{
    struct Context;
}

namespace Carbon::Internal
{
    /// Seconds a new container takes to fade in after its first, hidden frame.
    inline constexpr float AppearFadeDuration = 0.12f;

    /// What a container remembers from the previous frame. Layout is single-pass: anything that depends on a size
    /// not known yet (alignment, free space, fitting the content) uses these measurements.
    struct ContainerRecord
    {
        /// Size of the content without padding. Flexible items count with their minimum length.
        Vec2 ContentSize;
        /// Total weight of flexible items along the main axis.
        float FlexWeight;
        /// Seconds since the container appeared; drives the fade-in.
        float AppearTime;
        /// The last frame that used this record; detects several containers from one call site.
        uint64_t LastFrame;
        /// How many containers have been opened from this record's call site during LastFrame. The next one
        /// derives its ID from this count, so a loop of containers costs one more lookup each, not one per
        /// container before it.
        uint32_t Occurrences;
    };

    /// The most columns a grid can have.
    inline constexpr uint32_t MaxGridColumns = 32;
    /// The most cells spanning several columns that one grid measures per frame.
    inline constexpr uint32_t MaxGridSpans = 64;

    /// What a grid remembers from the previous frame: the width of each column. Cells are placed against these,
    /// so a column lines up across rows even though its widest cell may come later in the frame.
    struct GridRecord
    {
        float ColumnWidths[MaxGridColumns];
        uint32_t ColumnCount;
    };

    enum class ContainerKind : uint8_t
    {
        Root,
        VStack,
        HStack,
        ScrollView,
        Overlay,
        Grid,
        GridRow
    };

    /// Parameters shared by every kind of container.
    struct ContainerDescription
    {
        ContainerKind Kind = ContainerKind::VStack;
        Carbon::Axis Axis = Carbon::Axis::Vertical;
        ID Id;
        Size Width = Size::Fit();
        Size Height = Size::Fit();
        EdgeInsets Padding;
        float Spacing = 0.0f;
        /// 0, 0.5 or 1: where items sit across the axis.
        float CrossFactor = 0.0f;
        /// 0, 0.5 or 1: where the content sits along the axis when there is free space and nothing flexible.
        float JustifyFactor = 0.0f;
        /// The content may be larger than the container along the axis and is shifted by this offset.
        bool IsScrolling = false;
        Vec2 ScrollOffset;
        /// The container is placed at FloatingOrigin instead of in its parent's flow, and takes no space there.
        bool IsFloating = false;
        Vec2 FloatingOrigin;
    };

    /// A container that is open during the frame.
    struct LayoutFrame
    {
        ContainerKind Kind = ContainerKind::Root;
        Carbon::Axis Axis = Carbon::Axis::Vertical;
        ID Id;
        ContainerRecord* Record = nullptr;

        /// Top-left corner, and the size where it is known up front (Fixed and Fill axes).
        Vec2 Origin;
        Vec2 ResolvedSize;
        bool FitsWidth = false;
        bool FitsHeight = false;
        EdgeInsets Padding;
        /// The content area: this frame's for known axes, last frame's measurement for fitting axes.
        Rect Inner;

        float Spacing = 0.0f;
        float CrossFactor = 0.0f;
        /// Points of free space per unit of weight.
        float FlexUnit = 0.0f;
        /// Main-axis position of the next item, and where the content started.
        float Cursor = 0.0f;
        float ContentStart = 0.0f;

        // Measured while the frame is open.
        float MainExtent = 0.0f;
        float CrossExtent = 0.0f;
        float FlexWeight = 0.0f;
        uint32_t ItemCount = 0;
        Rect LastItem;

        bool HasCursorOverride = false;
        Vec2 CursorOverride;

        /// How this container takes part in its parent's flow.
        bool IsFlexible = false;
        float ParentFlexWeight = 0.0f;
        bool FillsParentCross = false;
        /// Inside a grid row: takes the width of its cell, and its column measures its content instead.
        bool FillsParentCell = false;
        bool IsFloating = false;

        DeferredShape Background;
        float BackgroundRadius = 0.0f;
        bool HasOpacity = false;
        bool IsAppearing = false;
    };

    /// A scroll view that is open during the frame; parallel to its LayoutFrame.
    struct ScrollFrame
    {
        ID Id;
        Rect Viewport;
        Carbon::Axis Axis = Carbon::Axis::Vertical;
        bool ShowsIndicator = true;
        Vec2 DisplayedOffset;
    };

    /// A cell that spans several columns, measured during the frame and distributed over its columns at the end.
    struct GridSpan
    {
        uint32_t Column = 0;
        uint32_t Count = 0;
        float Width = 0.0f;
    };

    /// A grid that is open during the frame; parallel to its LayoutFrame. Holds this frame's measurements and the
    /// cursor of the row that is open.
    struct GridFrame
    {
        /// Index of the grid's LayoutFrame in LayoutState::Frames.
        size_t FrameIndex = 0;
        GridRecord* Record = nullptr;
        float HorizontalSpacing = 0.0f;
        float AlignmentFactor = 0.0f;
        float VerticalFactor = 0.5f;
        float ColumnFactors[MaxGridColumns] = {};
        uint32_t ColumnFactorCount = 0;

        // Measured while the grid is open.
        float Widths[MaxGridColumns] = {};
        GridSpan Spans[MaxGridSpans] = {};
        uint32_t SpanCount = 0;
        uint32_t ColumnCount = 0;

        // The open row.
        uint32_t Column = 0;
        float RowVerticalFactor = 0.5f;

        // Set by SetNextGridCell for the next cell only.
        uint32_t NextSpan = 1;
        std::optional<float> NextAlignmentFactor;
        std::optional<float> NextVerticalFactor;
    };

    /// The layout state of one context: the stack of open containers and scroll bookkeeping.
    struct LayoutState
    {
        std::vector<LayoutFrame> Frames;
        std::vector<ScrollFrame> ScrollFrames;
        std::vector<GridFrame> GridFrames;
        /// The scroll view under the pointer: found during a frame, used by the next one.
        ID HoveredScrollView;
        ID HoveredScrollViewCandidate;
        /// The scroll view that Page Up and Page Down act on: the hovered one, or else the first (outermost) one.
        ID KeyboardScrollView;
        ID FirstScrollViewCandidate;
    };

    /// Called by NewFrame: opens the root container, which covers the display.
    void BeginLayout(Context& context);
    /// Called by EndFrame: closes the root container and reports containers left open.
    void EndLayout(Context& context);

    /// Opens a container as an item of the current one.
    LayoutFrame& BeginContainer(Context& context, const ContainerDescription& description);
    /// Closes the current container, which must be of `kind`. Returns its final rectangle.
    Rect EndContainer(Context& context, ContainerKind kind);

    /// Scrolls the open scroll views so that a rectangle becomes visible. Used when keyboard focus moves to an
    /// item.
    void RevealInScrollViews(Context& context, const Rect& rect);

    /// The grid whose cells are the items of `parent`, or null when `parent` is not a row directly inside the
    /// innermost open grid.
    GridFrame* FindGrid(LayoutState& layout, const LayoutFrame& parent);

    /// 0, 0.5 or 1: the position of an item in the free space for an alignment.
    float GetAlignmentFactor(Alignment alignment);
    float GetAlignmentFactor(VerticalAlignment alignment);

    /// Identifies a container by its call site, the enclosing container and the ID scope.
    ID GetCallSiteID(Context& context, const char* file, uint32_t line, uint32_t column);
} // namespace Carbon::Internal
