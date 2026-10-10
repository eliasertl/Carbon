#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Carbon/Core/ID.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Core/Vec2.h"
#include "Carbon/Draw/DrawList.h"

namespace Carbon
{
    struct Context;
}

namespace Carbon::Internal
{
    /// The drag in progress in one context, and the drop targets competing for it.
    struct DragDropState
    {
        bool IsActive = false;
        /// The drag brings files from outside the application; it has no source item and ends with the host's
        /// drop or leave event.
        bool IsFromHost = false;
        ID SourceID;
        /// Where the source was pressed, from the corner of its rectangle: the preview keeps that distance.
        Vec2 GrabOffset;
        /// The payload. Its storage is kept between drags, so dragging does not allocate once it has grown.
        std::string Type;
        std::vector<std::byte> Data;
        std::vector<std::string_view> Files;

        /// The target that gets the drag: found during a frame among the targets under the pointer, used by the
        /// next frame, like the hovered item.
        ID HoveredTarget;
        ID TargetCandidate;
        uint32_t CandidateLayer = 0;
        float CandidateArea = 0.0f;
        /// The payload is delivered to the hovered target during this frame; the drag ends with it.
        bool IsDelivering = false;
        /// Files were dropped: they are delivered during the next frame, once a target has been found for them.
        bool IsDropPending = false;

        /// The preview of the source, between BeginDragSource and EndDragSource.
        bool IsPreviewOpen = false;
        DeferredShape PreviewShadow;
        DeferredShape PreviewBackground;
        DeferredShape PreviewBorder;
        bool HasPreviewBackground = false;

        /// Ends the drag without touching the pointer; the payload's storage is kept.
        void Clear();
    };

    /// The ID that holds the pointer during a drag, so that nothing else reacts to it.
    ID GetDragPointerID();

    /// Called by NewFrame after input and interaction: starts drags of files from the host, decides whether this
    /// frame delivers the drop, and cancels a drag on Escape or when the window loses focus.
    void BeginDragDrop(Context& context);
    /// Called by EndFrame before interaction: publishes the target found during the frame and ends a delivered
    /// drag.
    void EndDragDrop(Context& context);
    /// True while something is dragged. Scroll views scroll near their edges meanwhile.
    bool IsDragActive(const Context& context);
} // namespace Carbon::Internal
