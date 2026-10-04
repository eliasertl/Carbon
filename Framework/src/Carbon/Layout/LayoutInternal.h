#pragma once

#include <cstdint>
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
    };

    enum class ContainerKind : uint8_t
    {
        Root,
        VStack,
        HStack,
        ScrollView
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

    /// The layout state of one context: the stack of open containers and scroll bookkeeping.
    struct LayoutState
    {
        std::vector<LayoutFrame> Frames;
        std::vector<ScrollFrame> ScrollFrames;
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

    /// Identifies a container by its call site, the enclosing container and the ID scope.
    ID GetCallSiteID(Context& context, const char* file, uint32_t line, uint32_t column);
} // namespace Carbon::Internal
