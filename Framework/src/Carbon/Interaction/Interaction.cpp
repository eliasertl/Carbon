#include "Carbon/Interaction/Interaction.h"

#include <algorithm>
#include <cstdint>

#include "Carbon/Animation/Animation.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/State.h"
#include "Carbon/Interaction/InteractionInternal.h"
#include "Carbon/Overlay/OverlayInternal.h"

namespace Carbon
{
    namespace
    {
        using Internal::InputState;
        using Internal::InteractionState;

        constexpr size_t LeftButton = static_cast<size_t>(MouseButton::Left);

        // Extra distance the focus ring starts at before it settles onto the control.
        constexpr float FocusRingTravel = 4.0f;
        // Room kept between the text being edited and an on-screen keyboard, and for how many frames after the
        // keyboard appeared the text is brought into view.
        constexpr float KeyboardMargin = 12.0f;
        constexpr int KeyboardRevealFrameCount = 3;

        bool IsKeyPressedOnce(const InputState& input, Key key)
        {
            return input.KeyPressed[static_cast<size_t>(key)];
        }

        bool IsKeyPressedOrRepeated(const InputState& input, Key key)
        {
            const size_t index = static_cast<size_t>(key);
            return input.KeyPressed[index] || input.KeyRepeated[index];
        }

        void GiveFocus(Context& context, ID id, bool showRing)
        {
            InteractionState& state = context.Interaction;
            state.FocusedID = id;
            state.IsFocusVisible = showRing;
            state.FocusSetFrame = context.FrameCount;
        }

        // Moves focus to the next or previous of last frame's focusable items that belong to the overlay holding
        // the keyboard (or to no overlay, when none does).
        void MoveFocus(Context& context, bool isBackward)
        {
            InteractionState& state = context.Interaction;
            const ID scope = Internal::GetActiveFocusScope(context);
            size_t count = 0;
            size_t current = SIZE_MAX;
            for (const InteractionState::FocusEntry& entry : state.PreviousFocusOrder)
            {
                if (entry.Scope != scope)
                    continue;
                if (entry.Id == state.FocusedID)
                    current = count;
                count++;
            }
            if (count == 0)
                return;

            size_t target = isBackward ? count - 1 : 0;
            if (current != SIZE_MAX)
                target = isBackward ? (current + count - 1) % count : (current + 1) % count;
            for (const InteractionState::FocusEntry& entry : state.PreviousFocusOrder)
            {
                if (entry.Scope != scope)
                    continue;
                if (target == 0)
                {
                    GiveFocus(context, entry.Id, true);
                    state.DidFocusMove = true;
                    return;
                }
                target--;
            }
        }

        // Fires like a held key while the mouse holds a repeating button: after a delay, then at a steady rate.
        bool UpdateMouseRepeat(Context& context, ID id)
        {
            float& held = *GetState<float>(HashID("##repeat", id), StateLifetime::Transient);
            const float previous = held;
            held += context.DeltaTime;
            if (held < Internal::KeyRepeatDelay)
                return false;
            if (previous < Internal::KeyRepeatDelay)
                return true;
            const float interval = Internal::KeyRepeatInterval;
            return static_cast<int>((held - Internal::KeyRepeatDelay) / interval) >
                   static_cast<int>((previous - Internal::KeyRepeatDelay) / interval);
        }
    } // namespace

    namespace Internal
    {
        void InteractionState::PublishTo(IO& io) const
        {
            io.m_WantsMouse = HoverCandidate.IsValid() || ActiveID.IsValid();
            io.m_WantsKeyboard = FocusedID.IsValid();
            io.m_WantsTextInput = IsTextInputActive;
            io.m_CaretRect = IsTextInputActive ? TextInputCaretRect : Rect();
            io.m_WantsCompositionCancel = IsCompositionCancelRequested;
            // The state's text points into the IO object's own copy, which keeps its capacity.
            if (IsTextInputActive)
                io.m_TextInputText.assign(TextInputText);
            else
                io.m_TextInputText.clear();
            io.m_TextInputState = IsTextInputActive ? TextInput : TextInputState();
            io.m_TextInputState.Text = io.m_TextInputText;
            io.m_TextInputAreas.assign(TextInputAreas.begin(), TextInputAreas.end());
        }

        void PublishTextInput(Context& context, ID owner, std::string_view text, size_t selectionStart,
                              size_t selectionEnd, KeyboardType keyboard, bool isMultiLine, bool isSecure)
        {
            InteractionState& state = context.Interaction;
            state.TextInputOwner = owner;
            state.TextInputText.assign(text);
            state.TextInput.SelectionStart = std::min(selectionStart, text.size());
            state.TextInput.SelectionEnd = std::min(selectionEnd, text.size());
            state.TextInput.Keyboard = keyboard;
            state.TextInput.IsMultiLine = isMultiLine;
            state.TextInput.IsSecure = isSecure;
        }

        void AddTextInputArea(Context& context, const Rect& rect)
        {
            const Rect visible = GetHitRect(rect).GetIntersection(context.Draw.GetClipRect());
            if (visible.Width > 0.0f && visible.Height > 0.0f && !IsPointBlockedByOverlay(context, visible.GetCenter()))
                context.Interaction.TextInputAreas.push_back(visible);
        }

        void KeepAboveKeyboard(Context& context, const Rect& rect)
        {
            const Rect keyboard = context.HostIO.GetKeyboardRect();
            if (context.Interaction.KeyboardRevealFrames <= 0 || keyboard.Width <= 0.0f)
                return;
            RevealInScrollViews(context, rect, keyboard.Y - KeyboardMargin);
        }

        void BeginInteraction(Context& context)
        {
            InteractionState& state = context.Interaction;
            const InputState& input = context.Input;

            state.PreviousFocusOrder.swap(state.FocusOrder);
            state.FocusOrder.clear();
            state.HoverCandidate = ID();
            state.HoverCandidateLayer = 0;
            state.HoverCandidateDistance = 0.0f;
            state.HitRects.clear();
            state.IsActiveAlive = false;
            if (!state.ActiveID.IsValid())
                state.IsActiveDrag = false;
            state.IsFocusedAlive = false;
            state.DidFocusMove = false;
            state.DefaultButton = ID();
            state.IsEnterConsumed = false;
            state.DisabledStack.clear();
            state.DisabledDepth = 0;
            state.RequestedCursor = Cursor::Arrow;
            // Editing that moves to another control, or a keyboard that appears or grows, brings the control being
            // edited into view above the keyboard for a few frames, while the layout settles.
            const Rect keyboard = context.HostIO.GetKeyboardRect();
            const float keyboardTop = keyboard.Width > 0.0f ? keyboard.Y : context.DisplaySize.Y;
            if (keyboardTop < state.LastKeyboardTop - 0.5f)
                state.KeyboardRevealFrames = KeyboardRevealFrameCount;
            else if (state.KeyboardRevealFrames > 0)
                state.KeyboardRevealFrames--;
            state.LastKeyboardTop = keyboardTop;

            state.IsTextInputActive = false;
            state.TextInputCaretRect = Rect();
            state.IsCompositionCancelRequested = false;
            state.TextInputOwner = ID();
            state.TextInputAreas.clear();
            state.LastItem = InteractionState::LastItemData();

            // Using the keyboard while something has focus reveals the focus ring.
            if (state.FocusedID.IsValid())
            {
                for (size_t key = 0; key < InputState::KeyCount; key++)
                {
                    const Key code = static_cast<Key>(key);
                    const bool isModifier = code >= Key::LeftCtrl && code <= Key::RightSuper;
                    if (input.KeyPressed[key] && !isModifier)
                    {
                        state.IsFocusVisible = true;
                        break;
                    }
                }
            }

            // Tab and Shift+Tab move focus through last frame's focusable items, wrapping around. FocusNext and
            // FocusPrevious request the same step from code.
            const KeyModifiers blocking = KeyModifiers::Ctrl | KeyModifiers::Alt | KeyModifiers::Super;
            const bool hasBlockingModifier = (input.Modifiers & blocking) != KeyModifiers::None;
            int step = state.PendingFocusMove;
            state.PendingFocusMove = 0;
            const bool isShiftHeld = HasModifiers(input.Modifiers, KeyModifiers::Shift);
            if (state.TabTaker.IsValid() && state.TabTaker == state.FocusedID)
            {
                // The focused item uses Tab; as in macOS text views, Ctrl+Tab leaves it, and Shift+Tab goes back.
                const bool isCtrlHeld = HasModifiers(input.Modifiers, KeyModifiers::Ctrl);
                if (IsKeyPressedOrRepeated(input, Key::Tab) && (isCtrlHeld || isShiftHeld))
                    step = isShiftHeld ? -1 : 1;
            }
            else if (IsKeyPressedOrRepeated(input, Key::Tab) && !hasBlockingModifier)
            {
                step = isShiftHeld ? -1 : 1;
            }
            if (step != 0)
                MoveFocus(context, step < 0);
        }

        void EndInteraction(Context& context)
        {
            InteractionState& state = context.Interaction;
            const InputState& input = context.Input;

            CB_VERIFY(state.DisabledStack.empty(), "Unbalanced PushDisabled: {} call(s) without PopDisabled",
                      state.DisabledStack.size());
            while (!state.DisabledStack.empty())
                PopDisabled();

            // A composition belongs to the text field being edited. With none edited this frame, nothing shows it
            // or can take its text: it is dropped, and the host's input method is asked to drop it too.
            CompositionState& composition = context.Input.Composition;
            if (composition.IsActive && !state.IsTextInputActive)
            {
                composition.Clear();
                state.IsCompositionCancelRequested = true;
            }

            state.HoveredID = state.HoverCandidate;
            state.TabTaker = state.TabTakerThisFrame;
            state.TabTakerThisFrame = ID();

            // An item that held the pointer and was not submitted this frame lets go of it.
            if (state.ActiveID.IsValid() && !state.IsActiveAlive)
                state.ActiveID = ID();

            // Pressing on empty space takes focus away.
            bool isAnyButtonPressed = false;
            for (const bool pressed : input.MousePressed)
                isAnyButtonPressed = isAnyButtonPressed || pressed;
            if (isAnyButtonPressed && !state.ActiveID.IsValid())
                state.FocusedID = ID();

            // Focus on an item that no longer exists is dropped. An item focused programmatically gets one
            // frame to show up.
            if (state.FocusedID.IsValid() && !state.IsFocusedAlive)
            {
                const bool isRegistered = std::any_of(state.FocusOrder.begin(), state.FocusOrder.end(),
                                                      [&state](const InteractionState::FocusEntry& entry)
                                                      { return entry.Id == state.FocusedID; });
                const bool isRecent = context.FrameCount <= state.FocusSetFrame + 1;
                if (!isRegistered && !isRecent)
                    state.FocusedID = ID();
            }

            // Enter that no focused control used goes to the default button, on the next frame.
            const bool isEnterPressed =
                IsKeyPressedOnce(input, Key::Enter) || IsKeyPressedOnce(input, Key::KeypadEnter);
            state.PendingDefaultActivation = (isEnterPressed && !state.IsEnterConsumed) ? state.DefaultButton : ID();

            if (state.RequestedCursor != state.ShownCursor)
            {
                state.ShownCursor = state.RequestedCursor;
                if (context.HostCallbacks.SetCursor)
                    context.HostCallbacks.SetCursor(state.ShownCursor);
            }

            state.PublishTo(context.HostIO);

            // The host's on-screen keyboard follows the control being edited: shown when editing starts or moves to
            // another control, hidden when it ends.
            const ID keyboardOwner = state.IsTextInputActive ? state.TextInputOwner : ID();
            if (keyboardOwner != state.KeyboardOwner)
            {
                state.KeyboardOwner = keyboardOwner;
                state.KeyboardRevealFrames = KeyboardRevealFrameCount;
                if (context.HostCallbacks.SetKeyboardVisible)
                    context.HostCallbacks.SetKeyboardVisible(keyboardOwner.IsValid());
            }
        }

        void TakeTabKey(Context& context, ID id)
        {
            context.Interaction.TabTakerThisFrame = id;
        }

        bool UpdateHover(Context& context, ID id, const Rect& rect)
        {
            InteractionState& state = context.Interaction;
            const Rect hit = GetHitRect(rect);
            if (state.IsRecordingHitRects)
                state.HitRects.push_back({id, hit});
            if (!IsRectHovered(hit))
                return false;
            if (state.ActiveID.IsValid() && state.ActiveID != id)
                return false;

            // Claim the pointer for the next frame. Later items are drawn on top and win, except against an
            // item on a higher layer. Where the enlarged hit areas of touch mode overlap, the item the finger is
            // closest to wins; inside an item the distance is zero, so this changes nothing for exact areas.
            const uint32_t layer = context.Draw.GetLayerOrder();
            const Vec2 pointer = context.Input.MousePos;
            const Vec2 nearest = Max(rect.GetMin(), Min(pointer, rect.GetMax()));
            const float distance = (pointer - nearest).GetLengthSquared();
            if (!state.HoverCandidate.IsValid() || layer > state.HoverCandidateLayer ||
                (layer == state.HoverCandidateLayer && distance <= state.HoverCandidateDistance))
            {
                state.HoverCandidate = id;
                state.HoverCandidateLayer = layer;
                state.HoverCandidateDistance = distance;
            }
            return state.HoveredID == id;
        }
    } // namespace Internal

    Interaction ButtonBehavior(ID id, const Rect& rect, const ButtonBehaviorOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        InteractionState& state = context.Interaction;
        const InputState& input = context.Input;
        Interaction result;

        if (options.Disabled || state.DisabledDepth > 0)
        {
            if (state.ActiveID == id)
                state.ActiveID = ID();
            if (state.FocusedID == id)
                state.FocusedID = ID();
            SetLastItem(id, rect, result);
            return result;
        }

        if (options.Focusable)
            RegisterFocusable(id, rect);

        result.Hovered = Internal::UpdateHover(context, id, rect);
        // With a finger, what activates on press with a mouse activates when the finger lifts: a touch that becomes
        // a scroll must not have picked anything. Repeating buttons keep reacting at once.
        const bool activatesOnPress = options.ActivateOnPress && (!input.IsPointerTouch || options.Repeat);

        if (result.Hovered && input.MousePressed[LeftButton] && !state.ActiveID.IsValid())
        {
            state.ActiveID = id;
            // Clicking moves focus to the control without showing the ring, so that keyboard navigation
            // continues from where the user clicked.
            if (options.Focusable)
                GiveFocus(context, id, false);
            result.DoubleClicked = input.MouseClickCount[LeftButton] == 2;
            if (activatesOnPress)
                result.Clicked = true;
            if (options.Repeat)
                *GetState<float>(HashID("##repeat", id), StateLifetime::Transient) = 0.0f;
        }

        if (state.ActiveID == id)
        {
            state.IsActiveAlive = true;
            const bool isOver = IsRectHovered(GetHitRect(rect));
            if (input.MouseDown[LeftButton])
            {
                result.Pressed = isOver;
                if (options.Repeat && isOver && UpdateMouseRepeat(context, id))
                    result.Clicked = true;
            }
            else
            {
                // Released. Losing the host window's focus, or the system cancelling a touch, also releases the
                // button, but must not activate.
                if (isOver && !activatesOnPress && input.MouseReleased[LeftButton] && input.Focused &&
                    !input.IsPointerCancelled)
                    result.Clicked = true;
                state.ActiveID = ID();
            }
        }

        result.Focused = state.FocusedID == id;
        result.FocusVisible = result.Focused && state.IsFocusVisible && input.Focused;
        if (result.Focused)
        {
            const bool isSpace =
                options.Repeat ? IsKeyPressedOrRepeated(input, Key::Space) : IsKeyPressedOnce(input, Key::Space);
            const bool isEnter = IsKeyPressedOnce(input, Key::Enter) || IsKeyPressedOnce(input, Key::KeypadEnter);
            if (isSpace || isEnter)
                result.Clicked = true;
            if (isEnter)
                state.IsEnterConsumed = true;
            if (input.KeyDown[static_cast<size_t>(Key::Space)])
                result.Pressed = true;
        }

        // A default button under an overlay that holds the keyboard does not get Enter.
        if (options.IsDefault && Internal::IsInActiveFocusScope(context))
        {
            state.DefaultButton = id;
            if (state.PendingDefaultActivation == id)
            {
                result.Clicked = true;
                state.PendingDefaultActivation = ID();
            }
        }

        // A finger does not hover: the pointer exists only while it touches, and pressing shows instead.
        if (input.IsPointerTouch)
            result.Hovered = false;
        SetLastItem(id, rect, result);
        return result;
    }

    DragInteraction DragBehavior(ID id, const Rect& rect, const DragBehaviorOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        InteractionState& state = context.Interaction;
        const InputState& input = context.Input;
        DragInteraction result;
        result.Position = input.MousePos;

        if (options.Disabled || state.DisabledDepth > 0)
        {
            if (state.ActiveID == id)
                state.ActiveID = ID();
            if (state.FocusedID == id)
                state.FocusedID = ID();
            SetLastItem(id, rect, Interaction());
            return result;
        }

        if (options.Focusable)
            RegisterFocusable(id, rect);

        result.Hovered = Internal::UpdateHover(context, id, rect);

        if (result.Hovered && input.MousePressed[LeftButton] && !state.ActiveID.IsValid())
        {
            state.ActiveID = id;
            // A finger that has to rest first holds the item without dragging it, and may still scroll.
            state.IsActiveDrag = !(options.WaitsForLongPress && input.IsPointerTouch);
            if (options.Focusable)
                GiveFocus(context, id, false);
            result.Started = state.IsActiveDrag;
        }

        if (state.ActiveID == id && !state.IsActiveDrag)
        {
            state.IsActiveAlive = true;
            result.Total = input.MousePos - input.MousePressedPos[LeftButton];
            if (!input.MouseDown[LeftButton])
            {
                result.Ended = true;
                state.ActiveID = ID();
            }
            else if (context.Gestures.IsLongPressed)
            {
                state.IsActiveDrag = true;
                result.Started = true;
                result.Active = true;
            }
        }
        else if (state.ActiveID == id)
        {
            state.IsActiveAlive = true;
            result.Total = input.MousePos - input.MousePressedPos[LeftButton];
            if (input.MouseDown[LeftButton])
            {
                result.Active = true;
                result.Delta = result.Started ? Vec2() : input.MouseDelta;
            }
            else
            {
                result.Ended = true;
                state.ActiveID = ID();
            }
        }

        result.Focused = state.FocusedID == id;
        result.FocusVisible = result.Focused && state.IsFocusVisible && input.Focused;
        if (input.IsPointerTouch)
            result.Hovered = false;

        Interaction summary;
        summary.Hovered = result.Hovered;
        summary.Pressed = result.Active;
        summary.Focused = result.Focused;
        summary.FocusVisible = result.FocusVisible;
        SetLastItem(id, rect, summary);
        return result;
    }

    bool IsRectHovered(const Rect& rect)
    {
        const Context& context = Internal::GetFrameContext();
        const InputState& input = context.Input;
        return input.HasMousePos && rect.Contains(input.MousePos) &&
               context.Draw.GetClipRect().Contains(input.MousePos) && !Internal::IsPointerBlockedByOverlay(context);
    }

    Rect GetHitRect(const Rect& rect)
    {
        const Context& context = Internal::GetContext();
        if (!context.IsTouchMode)
            return rect;
        const float growX = std::max(0.0f, MinimumTouchTarget - rect.Width) * 0.5f;
        const float growY = std::max(0.0f, MinimumTouchTarget - rect.Height) * 0.5f;
        return Rect(rect.X - growX, rect.Y - growY, rect.Width + growX * 2.0f, rect.Height + growY * 2.0f);
    }

    void RegisterFocusable(ID id, const Rect& rect)
    {
        Context& context = Internal::GetFrameContext();
        InteractionState& state = context.Interaction;
        if (state.DisabledDepth > 0)
            return;
        state.FocusOrder.push_back({id, Internal::GetCurrentFocusScope(context)});
        if (state.FocusedID == id)
        {
            state.IsFocusedAlive = true;
            if (state.DidFocusMove)
                Internal::RevealInScrollViews(context, rect);
        }
    }

    bool IsFocused(ID id)
    {
        return id.IsValid() && Internal::GetContext().Interaction.FocusedID == id;
    }

    bool IsFocusVisible(ID id)
    {
        const Context& context = Internal::GetContext();
        return IsFocused(id) && context.Interaction.IsFocusVisible && context.Input.Focused;
    }

    void SetFocus(ID id, bool showRing)
    {
        GiveFocus(Internal::GetContext(), id, showRing);
    }

    void ClearFocus()
    {
        Internal::GetContext().Interaction.FocusedID = ID();
    }

    void FocusNext()
    {
        Internal::GetContext().Interaction.PendingFocusMove = 1;
    }

    void FocusPrevious()
    {
        Internal::GetContext().Interaction.PendingFocusMove = -1;
    }

    ID GetFocusedID()
    {
        return Internal::GetContext().Interaction.FocusedID;
    }

    ID GetActiveID()
    {
        return Internal::GetContext().Interaction.ActiveID;
    }

    ID GetHoveredID()
    {
        return Internal::GetContext().Interaction.HoveredID;
    }

    void DrawFocusRing(ID id, const Rect& rect, float cornerRadius, bool alwaysWhenFocused)
    {
        Context& context = Internal::GetFrameContext();
        InteractionState& state = context.Interaction;
        const bool isVisible = alwaysWhenFocused ? (IsFocused(id) && context.Input.Focused) : IsFocusVisible(id);
        const ID animationID = HashID("##focusring", id);
        if (isVisible && state.FocusRingID != id)
        {
            // The ring appears: start its animation from nothing rather than fully shown.
            state.FocusRingID = id;
            SetAnimationValue(animationID, 0.0f);
        }
        // Only the item that shows the ring, or showed it last and is fading out, needs animation state.
        if (state.FocusRingID != id)
            return;

        const float progress = Animate(animationID, isVisible ? 1.0f : 0.0f, AnimationSpec::Fade(0.18f));
        if (progress <= 0.001f)
            return;

        // The ring fades in while it settles onto the control from slightly further out.
        const float travel = context.ReduceMotion ? 0.0f : (1.0f - progress) * FocusRingTravel;
        const Color color = context.Style.GetColor(StyleColor::FocusRing).WithOpacity(progress);
        context.Draw.AddFocusRing(rect, color, cornerRadius, context.Style.GetVar(StyleVar::FocusRingWidth),
                                  context.Style.GetVar(StyleVar::FocusRingOffset) + travel,
                                  context.Style.GetVar(StyleVar::CornerSmoothing));
    }

    void PushDisabled(bool disabled)
    {
        Context& context = Internal::GetFrameContext();
        InteractionState& state = context.Interaction;
        state.DisabledStack.push_back(disabled);
        if (!disabled)
            return;
        // Entering the outermost disabled scope dims everything drawn inside it.
        if (state.DisabledDepth == 0)
            context.Draw.PushOpacity(context.Style.GetVar(StyleVar::DisabledOpacity));
        state.DisabledDepth++;
    }

    void PopDisabled()
    {
        Context& context = Internal::GetFrameContext();
        InteractionState& state = context.Interaction;
        CB_VERIFY(!state.DisabledStack.empty(), "PopDisabled called without a matching PushDisabled");
        if (state.DisabledStack.empty())
            return;
        const bool wasDisabled = state.DisabledStack.back();
        state.DisabledStack.pop_back();
        if (!wasDisabled)
            return;
        state.DisabledDepth--;
        if (state.DisabledDepth == 0)
            context.Draw.PopOpacity();
    }

    bool IsDisabled()
    {
        return Internal::GetContext().Interaction.DisabledDepth > 0;
    }

    void SetCursor(Cursor cursor)
    {
        Internal::GetContext().Interaction.RequestedCursor = cursor;
    }

    bool IsItemHovered()
    {
        return Internal::GetContext().Interaction.LastItem.Hovered;
    }

    bool IsItemFocused()
    {
        return Internal::GetContext().Interaction.LastItem.Focused;
    }

    bool IsItemActive()
    {
        return Internal::GetContext().Interaction.LastItem.Active;
    }

    Rect GetItemRect()
    {
        return Internal::GetContext().Interaction.LastItem.Bounds;
    }

    ID GetItemID()
    {
        return Internal::GetContext().Interaction.LastItem.Id;
    }

    bool IsItemSubmitted()
    {
        return Internal::GetContext().Interaction.LastItem.Submitted;
    }

    void SetLastItem(ID id, const Rect& rect, const Interaction& interaction)
    {
        InteractionState::LastItemData& item = Internal::GetContext().Interaction.LastItem;
        item.Id = id;
        item.Bounds = rect;
        item.Hovered = interaction.Hovered;
        item.Focused = interaction.Focused;
        item.Active = interaction.Pressed;
        item.Submitted = false;
    }

    void SetItemSubmitted()
    {
        Internal::GetContext().Interaction.LastItem.Submitted = true;
    }
} // namespace Carbon
