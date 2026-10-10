#include "Carbon/Interaction/DragDrop.h"

#include "Carbon/Animation/Animation.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Interaction/DragDropInternal.h"
#include "Carbon/Layout/LayoutInternal.h"

namespace Carbon
{
    namespace
    {
        using Internal::DragDropState;

        constexpr size_t LeftButton = static_cast<size_t>(MouseButton::Left);

        // The preview: translucent, on a small rounded surface, like the drag images of macOS.
        constexpr float PreviewOpacity = 0.85f;
        constexpr float PreviewRadius = 6.0f;
        constexpr EdgeInsets PreviewPadding(8.0f, 4.0f);
        constexpr float PreviewShadowBlur = 12.0f;
        constexpr Vec2 PreviewShadowOffset(0.0f, 4.0f);
        // The highlight of a drop target under the drag.
        constexpr float HighlightWidth = 2.0f;
        constexpr float HighlightFill = 0.12f;
        constexpr float HighlightFadeDuration = 0.15f;

        DragPayload MakePayload(const DragDropState& state)
        {
            DragPayload payload;
            payload.Type = state.Type;
            payload.Data = std::span<const std::byte>(state.Data);
            payload.Files = std::span<const std::string_view>(state.Files);
            return payload;
        }

        // Ends a drag that came from a source: the pointer is free again.
        void EndSourceDrag(Context& context)
        {
            if (context.Interaction.ActiveID == Internal::GetDragPointerID())
                context.Interaction.ActiveID = ID();
            context.DragDrop.Clear();
        }

        void OpenPreview(Context& context, const DragSourceOptions& options)
        {
            DragDropState& state = context.DragDrop;
            DrawList& drawList = context.Draw;
            drawList.PushLayer(DrawLayer::Tooltip);
            drawList.PushOpacity(PreviewOpacity, false);
            // The preview may show the same widgets as the source; its own scope keeps them apart.
            const ID previewID = HashID("##dragpreview", state.SourceID);
            context.IDStack.push_back(previewID);

            Internal::ContainerDescription description;
            description.Kind = Internal::ContainerKind::VStack;
            description.Axis = Axis::Vertical;
            description.Id = previewID;
            description.Padding = options.HasPreviewBackground ? PreviewPadding : EdgeInsets();
            description.Spacing = context.Style.GetVar(StyleVar::Spacing);
            description.IsFloating = true;
            description.FloatingOrigin = context.Input.MousePos - state.GrabOffset;
            Internal::BeginContainer(context, description);

            state.IsPreviewOpen = true;
            state.HasPreviewBackground = options.HasPreviewBackground;
            if (options.HasPreviewBackground)
            {
                state.PreviewShadow = drawList.AddDeferredShadow(context.Style.GetColor(StyleColor::Shadow),
                                                                 PreviewShadowBlur, PreviewShadowOffset);
                state.PreviewBackground =
                    drawList.AddDeferredSquircle(context.Style.GetColor(StyleColor::OverlayBackground));
                state.PreviewBorder = drawList.AddDeferredSquircleStroke(
                    context.Style.GetColor(StyleColor::OverlayBorder), context.Scale.GetPixelSize());
            }
        }

        bool BeginSource(Context& context, ID id, const Rect& rect, const DragSourceOptions& options, bool takesPress)
        {
            DragDropState& state = context.DragDrop;
            Internal::InteractionState& interaction = context.Interaction;
            const Internal::InputState& input = context.Input;
            CB_VERIFY(!state.IsPreviewOpen, "BeginDragSource was called inside the preview of another drag source");
            if (!id.IsValid() || state.IsPreviewOpen)
                return false;

            if (!state.IsActive)
            {
                if (interaction.DisabledDepth > 0)
                    return false;
                if (takesPress)
                {
                    // A press that nothing inside the rectangle took belongs to the drag source.
                    if (!interaction.ActiveID.IsValid() && input.MousePressed[LeftButton] && IsRectHovered(rect))
                        interaction.ActiveID = id;
                    if (interaction.ActiveID == id)
                    {
                        if (input.MouseDown[LeftButton])
                            interaction.IsActiveAlive = true;
                        else
                            interaction.ActiveID = ID();
                    }
                }
                if (interaction.ActiveID != id || !input.MouseDown[LeftButton])
                    return false;
                if (input.IsPointerTouch)
                {
                    // A finger lifts the item with a long press, as on iOS; moving before that scrolls instead.
                    if (!context.Gestures.IsLongPressed)
                        return false;
                }
                else
                {
                    const Vec2 travel = input.MousePos - input.MousePressedPos[LeftButton];
                    if (travel.GetLengthSquared() < options.Threshold * options.Threshold)
                        return false;
                }

                // The drag begins: it holds the pointer from now on, whether the source stays or not.
                state.Clear();
                state.IsActive = true;
                state.SourceID = id;
                state.GrabOffset = input.MousePressedPos[LeftButton] - rect.GetMin();
                interaction.ActiveID = Internal::GetDragPointerID();
                interaction.IsActiveAlive = true;
                interaction.IsActiveDrag = true;
            }
            if (state.IsFromHost || state.SourceID != id || state.IsDelivering)
                return false;
            OpenPreview(context, options);
            return true;
        }

        Drop AcceptAt(Context& context, ID id, const Rect& rect, std::string_view type,
                      const DropTargetOptions& options)
        {
            Drop result;
            DragDropState& state = context.DragDrop;
            if (!state.IsActive || state.Type != type || !id.IsValid() || context.Interaction.DisabledDepth > 0)
                return result;

            // Competing for the next frame: the highest layer, then the smallest visible area.
            DrawList& drawList = context.Draw;
            if (IsRectHovered(rect))
            {
                const uint32_t layer = drawList.GetLayerOrder();
                const Rect visible = rect.GetIntersection(drawList.GetClipRect());
                const float area = visible.Width * visible.Height;
                if (!state.TargetCandidate.IsValid() || layer > state.CandidateLayer ||
                    (layer == state.CandidateLayer && area <= state.CandidateArea))
                {
                    state.TargetCandidate = id;
                    state.CandidateLayer = layer;
                    state.CandidateArea = area;
                }
                result.IsHovered = state.HoveredTarget == id;
            }

            // The highlight fades in when the drag arrives and out when it leaves, while the drag goes on; a drop
            // ends it at once, as in macOS.
            if (options.ShowsHighlight)
            {
                const bool isShown = result.IsHovered && !state.IsDelivering;
                const float opacity = Animate(HashID("##drophighlight", id), isShown ? 1.0f : 0.0f,
                                              AnimationSpec::Fade(HighlightFadeDuration));
                if (opacity > 0.0f && !state.IsDelivering)
                {
                    const Color accent = context.Style.GetColor(StyleColor::Accent);
                    const float smoothing = context.Style.GetVar(StyleVar::CornerSmoothing);
                    drawList.AddSquircle(rect, accent.WithOpacity(HighlightFill * opacity), options.CornerRadius,
                                         smoothing);
                    drawList.AddSquircleStroke(rect.Inset(EdgeInsets(HighlightWidth * 0.5f)),
                                               accent.WithOpacity(opacity), options.CornerRadius, HighlightWidth,
                                               smoothing);
                }
            }

            if (!result.IsHovered)
                return result;
            result.IsDelivered = state.IsDelivering;
            result.Position = context.Input.MousePos;
            result.Payload = MakePayload(state);
            return result;
        }
    } // namespace

    namespace Internal
    {
        void DragDropState::Clear()
        {
            IsActive = false;
            IsFromHost = false;
            SourceID = ID();
            GrabOffset = Vec2();
            Type.clear();
            Data.clear();
            Files.clear();
            HoveredTarget = ID();
            IsDelivering = false;
            IsDropPending = false;
        }

        ID GetDragPointerID()
        {
            return HashID("Carbon.DragPointer");
        }

        void BeginDragDrop(Context& context)
        {
            DragDropState& state = context.DragDrop;
            const InputState& input = context.Input;
            state.TargetCandidate = ID();
            state.CandidateLayer = 0;
            state.CandidateArea = 0.0f;
            state.IsDelivering = false;

            // Files from outside: a drag of files while they are over the display. A drop is delivered in the
            // frame after it arrived, when the target under it is known.
            const FileDragState& files = input.FileDrag;
            if (state.IsActive && state.IsFromHost && state.IsDropPending)
            {
                state.IsDelivering = true;
                state.IsDropPending = false;
                return;
            }
            if (files.IsOver || files.IsDropped)
            {
                if (!state.IsFromHost)
                {
                    if (state.IsActive)
                        EndSourceDrag(context);
                    state.Clear();
                    state.IsActive = true;
                    state.IsFromHost = true;
                    state.Type.assign(FilesPayloadType);
                }
                state.Files.assign(files.Files.begin(), files.Files.end());
                state.IsDropPending = files.IsDropped;
                return;
            }
            if (state.IsActive && state.IsFromHost)
            {
                state.Clear();
                return;
            }
            if (!state.IsActive)
                return;

            // A drag from a source ends with the button: a drop where it was released. Escape, or the window
            // losing focus, cancels it.
            if (input.KeyPressed[static_cast<size_t>(Key::Escape)])
            {
                context.Overlays.IsEscapeConsumed = true;
                EndSourceDrag(context);
                return;
            }
            if (!input.MouseDown[LeftButton])
            {
                const bool isDropped = input.MouseReleased[LeftButton] && input.Focused && !input.IsPointerCancelled;
                if (context.Interaction.ActiveID == GetDragPointerID())
                    context.Interaction.ActiveID = ID();
                if (isDropped)
                    state.IsDelivering = true;
                else
                    state.Clear();
                return;
            }
            context.Interaction.ActiveID = GetDragPointerID();
            context.Interaction.IsActiveDrag = true;
        }

        void EndDragDrop(Context& context)
        {
            DragDropState& state = context.DragDrop;
            if (state.IsPreviewOpen)
            {
                CB_VERIFY(false, "BeginDragSource returned true, but EndDragSource was not called");
                context.IDStack.pop_back();
                context.Draw.PopOpacity();
                context.Draw.PopLayer();
                state.IsPreviewOpen = false;
            }
            state.HoveredTarget = state.IsActive ? state.TargetCandidate : ID();
            if (state.IsDelivering)
            {
                state.Clear();
                return;
            }
            if (state.IsActive && !state.IsFromHost && context.Interaction.ActiveID == GetDragPointerID())
                context.Interaction.IsActiveAlive = true;
            // Dropped files need the next frame to reach their target.
            if (state.IsDropPending)
                context.IsAnimatingThisFrame = true;
        }

        bool IsDragActive(const Context& context)
        {
            return context.DragDrop.IsActive;
        }
    } // namespace Internal

    bool BeginDragSource(const DragSourceOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        const Internal::InteractionState::LastItemData& item = context.Interaction.LastItem;
        return BeginSource(context, item.Id, item.Bounds, options, false);
    }

    bool BeginDragSource(ID id, const Rect& rect, const DragSourceOptions& options)
    {
        return BeginSource(Internal::GetFrameContext(), id, rect, options, true);
    }

    void SetDragPayload(std::string_view type, std::span<const std::byte> data)
    {
        DragDropState& state = Internal::GetFrameContext().DragDrop;
        CB_VERIFY(state.IsPreviewOpen, "SetDragPayload must be called between BeginDragSource and EndDragSource");
        if (!state.IsPreviewOpen)
            return;
        state.Type.assign(type);
        state.Data.assign(data.begin(), data.end());
    }

    void EndDragSource()
    {
        Context& context = Internal::GetFrameContext();
        DragDropState& state = context.DragDrop;
        const bool isOpen = state.IsPreviewOpen && context.Layout.Frames.size() > 1 &&
                            context.Layout.Frames.back().IsFloating &&
                            context.Layout.Frames.back().Kind == Internal::ContainerKind::VStack;
        CB_VERIFY(isOpen, "EndDragSource does not match a BeginDragSource that returned true");
        if (!isOpen)
            return;

        const Rect rect = Internal::EndContainer(context, Internal::ContainerKind::VStack);
        DrawList& drawList = context.Draw;
        if (state.HasPreviewBackground)
        {
            const float smoothing = context.Style.GetVar(StyleVar::CornerSmoothing);
            drawList.ResolveDeferredSquircle(state.PreviewShadow, rect, PreviewRadius, smoothing);
            drawList.ResolveDeferredSquircle(state.PreviewBackground, rect, PreviewRadius, smoothing);
            drawList.ResolveDeferredSquircle(state.PreviewBorder, rect, PreviewRadius, smoothing);
        }
        context.IDStack.pop_back();
        drawList.PopOpacity();
        drawList.PopLayer();
        state.IsPreviewOpen = false;
    }

    Drop AcceptDrop(std::string_view type, const DropTargetOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        const Internal::InteractionState::LastItemData& item = context.Interaction.LastItem;
        return AcceptAt(context, item.Id, item.Bounds, type, options);
    }

    Drop AcceptDrop(ID id, const Rect& rect, std::string_view type, const DropTargetOptions& options)
    {
        return AcceptAt(Internal::GetFrameContext(), id, rect, type, options);
    }

    bool IsDragging()
    {
        return Internal::GetContext().DragDrop.IsActive;
    }

    DragPayload GetDragPayload()
    {
        const DragDropState& state = Internal::GetContext().DragDrop;
        return state.IsActive ? MakePayload(state) : DragPayload();
    }

    ID GetDragSourceID()
    {
        const DragDropState& state = Internal::GetContext().DragDrop;
        return state.IsActive && !state.IsFromHost ? state.SourceID : ID();
    }

    void CancelDrag()
    {
        Context& context = Internal::GetContext();
        if (context.DragDrop.IsActive && !context.DragDrop.IsFromHost)
            EndSourceDrag(context);
    }
} // namespace Carbon
