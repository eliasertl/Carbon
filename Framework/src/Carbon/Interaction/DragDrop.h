#pragma once

#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>
#include <type_traits>

#include "Carbon/Core/ID.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Core/Vec2.h"

namespace Carbon
{
    /// Drag and drop: an item becomes a drag source with a typed payload, other items accept drops of the payload
    /// types they understand, and the target under the pointer receives the payload when the button is released.
    ///
    ///     Carbon::BeginVStack({ .ID = "card" });               // a card: anything with a rectangle
    ///     Carbon::Text(task.Title);
    ///     Carbon::EndVStack();
    ///     if (Carbon::BeginDragSource(Carbon::GetID("card"), Carbon::GetLastItemRect()))
    ///     {
    ///         Carbon::SetDragPayload("Task", task.Id);
    ///         Carbon::Text(task.Title);                         // the preview, which follows the pointer
    ///         Carbon::EndDragSource();
    ///     }
    ///
    ///     Carbon::BeginVStack({ .ID = "done" });               // where tasks go
    ///     ...
    ///     Carbon::EndVStack();
    ///     const Carbon::Drop drop = Carbon::AcceptDrop(Carbon::GetID("done"), Carbon::GetLastItemRect(), "Task");
    ///     if (drop.IsDelivered)
    ///         MarkDone(drop.Payload.As<int>());
    ///
    /// Escape cancels a drag. A scroll view under the pointer scrolls while the pointer is near its edge. Files
    /// dragged in from the system arrive as a drag of FilesPayloadType, which the host forwards through the IO
    /// object (IO::AddFileDropEvent). See Docs/DragAndDrop.md.

    /// The payload type of files dragged into the window from outside the application; their paths are in
    /// DragPayload::Files.
    inline constexpr std::string_view FilesPayloadType = "Carbon.Files";

    /// What a drag carries.
    struct DragPayload
    {
        /// Names the kind of thing dragged, such as "Task" or "com.example.track". Targets accept drops by it.
        /// Empty when nothing is dragged.
        std::string_view Type = {};
        /// The bytes the drag source attached. Valid until the end of the frame.
        std::span<const std::byte> Data = {};
        /// For FilesPayloadType: the paths of the files, UTF-8. Valid until the end of the frame.
        std::span<const std::string_view> Files = {};

        /// The data as a value of T, for data attached with SetDragPayload<T>. A value-initialized T when the
        /// size does not match.
        template <typename T>
        T As() const
        {
            static_assert(std::is_trivially_copyable_v<T>, "Drag payloads are copied as bytes");
            T value{};
            if (Data.size() == sizeof(T))
                std::memcpy(&value, Data.data(), sizeof(T));
            return value;
        }
    };

    /// Per-call options of BeginDragSource. All fields are optional.
    struct DragSourceOptions
    {
        /// How far the pointer has to move with the button held before the drag begins, in points. Less is a
        /// click.
        float Threshold = 4.0f;
        /// Puts the preview on a rounded, translucent surface with a shadow. Turn it off for a preview that draws
        /// its own.
        bool HasPreviewBackground = true;
    };

    /// Makes the last item a drag source: an interactive item, which holds the pointer while it is pressed (a
    /// button, a list row). Returns true while the user drags it, from the moment the pointer has moved
    /// `Threshold` points with the button held. Then attach the payload with SetDragPayload, add the preview,
    /// which is laid out like in a VStack that follows the pointer, and call EndDragSource.
    bool BeginDragSource(const DragSourceOptions& options = {});
    /// Makes a rectangle a drag source, such as a stack of text and images (pass GetLastItemRect() after it).
    /// A press inside it that no interactive item inside took is taken by the drag source; an item that holds
    /// the pointer with the ID `id` (a row of a component) is dragged as well.
    bool BeginDragSource(ID id, const Rect& rect, const DragSourceOptions& options = {});
    /// Attaches the payload of the drag that is beginning: its type and a copy of `data`. Call it between
    /// BeginDragSource and EndDragSource; calling it every frame is fine.
    void SetDragPayload(std::string_view type, std::span<const std::byte> data = {});
    /// Attaches a value as the payload, copied as bytes.
    template <typename T>
    void SetDragPayload(std::string_view type, const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "Drag payloads are copied as bytes");
        SetDragPayload(type, std::span<const std::byte>(reinterpret_cast<const std::byte*>(&value), sizeof(T)));
    }
    void EndDragSource();

    /// Per-call options of AcceptDrop. All fields are optional.
    struct DropTargetOptions
    {
        /// Outlines the target in the accent color while a drag it accepts is over it. Components that show
        /// where the drop would go (an insertion line) turn it off.
        bool ShowsHighlight = true;
        float CornerRadius = 6.0f;
    };

    /// What a drop target sees of the drag this frame.
    struct Drop
    {
        /// A drag of the accepted type is over the target, and the target is the one that would receive it.
        bool IsHovered = false;
        /// The payload was dropped onto the target this frame: act on it.
        bool IsDelivered = false;
        /// Where the pointer is, in points.
        Vec2 Position;
        /// The drag's payload, while IsHovered.
        DragPayload Payload;
    };

    /// Makes the last interactive item a drop target for payloads of `type`. Where targets overlap, the one on the
    /// highest layer, and then the smallest one, gets the drag: an item inside a container that is a target too wins.
    Drop AcceptDrop(std::string_view type, const DropTargetOptions& options = {});
    /// The same for a rectangle with an ID of its own, for components that are not one item.
    Drop AcceptDrop(ID id, const Rect& rect, std::string_view type, const DropTargetOptions& options = {});

    /// True while something is dragged: from a drag source, or files from outside the application.
    bool IsDragging();
    /// The payload of the drag in progress; its Type is empty when nothing is dragged. Components use it to show
    /// that they would take a drop before it is over them.
    DragPayload GetDragPayload();
    /// The drag source being dragged; invalid when none is, or when the drag comes from outside.
    ID GetDragSourceID();
    /// Ends the drag in progress without a drop, as Escape does.
    void CancelDrag();
} // namespace Carbon
