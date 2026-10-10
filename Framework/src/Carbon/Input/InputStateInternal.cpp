#include "Carbon/Input/InputStateInternal.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Core/UTF8.h"

namespace Carbon::Internal
{
    namespace
    {
        // Keys that produce text when typed. Other keys (Enter, arrows, Backspace, ...) act on the text, so their
        // order relative to typed characters must be preserved across frames.
        bool IsTextKey(Key key)
        {
            return key == Key::Space || (key >= Key::D0 && key <= Key::Z) ||
                   (key >= Key::Apostrophe && key <= Key::GraveAccent);
        }

        bool IsModifierKey(Key key)
        {
            return key >= Key::LeftCtrl && key <= Key::RightSuper;
        }

        constexpr size_t LeftButton = static_cast<size_t>(MouseButton::Left);

        // How much of a new velocity sample replaces the smoothed one, and how long the pointer may rest before
        // its velocity counts as zero.
        constexpr float VelocitySmoothing = 0.6f;
        constexpr float VelocityRestTime = 0.06f;
    } // namespace

    void InputState::Update(IO& io, double time)
    {
        const float deltaTime = io.m_DeltaTime;
        const bool hadMousePos = HasMousePos;
        const Vec2 previousMousePos = MousePos;

        MousePressed.fill(false);
        MouseReleased.fill(false);
        MouseClickCount.fill(0);
        MouseWheel = Vec2();
        KeyPressed.fill(false);
        KeyRepeated.fill(false);
        KeyReleased.fill(false);
        Characters.clear();
        CompositionChanged = false;
        FileDrag.IsDropped = false;
        IsPointerCancelled = false;

        bool mouseMoved = false;
        bool textEntered = false;
        bool actionKeyPressed = false;
        std::array<bool, MouseButtonCount> buttonChanged{};
        std::array<bool, KeyCount> keyChanged{};
        bool anyButtonChanged = false;

        // Changes the state of a button; a press also counts clicks that follow each other closely.
        const auto setButton = [&](size_t button, bool down, float multiClickDistance)
        {
            MouseDown[button] = down;
            buttonChanged[button] = true;
            anyButtonChanged = true;
            if (!down)
            {
                MouseReleased[button] = true;
                return;
            }
            const Vec2 travel = MousePos - MousePressedPos[button];
            const bool isMultiClick = MouseLastClickCount[button] > 0 &&
                                      time - MouseLastPressTime[button] <= MultiClickTime &&
                                      travel.GetLengthSquared() <= multiClickDistance * multiClickDistance;
            const uint8_t count = isMultiClick ? static_cast<uint8_t>(MouseLastClickCount[button] + 1) : 1;
            MousePressed[button] = true;
            MouseClickCount[button] = count;
            MouseLastClickCount[button] = count;
            MouseLastPressTime[button] = time;
            MousePressedPos[button] = MousePos;
        };

        // The first finger down drives the pointer like a mouse that exists only while the finger is down. Its
        // beginning and its end take two frames each, for the reason a press waits after a move: the pointer
        // arrives, then presses; the button is released where the finger lifts, then the pointer leaves.
        // Returns true when the event has to wait for the next frame.
        const auto applyTouch = [&](InputEvent& event) -> bool
        {
            const bool isEnd = event.Phase == TouchPhase::Ended || event.Phase == TouchPhase::Cancelled;
            const bool isPrimary = (HasPrimaryTouch && event.TouchId == PrimaryTouchId) || (isEnd && event.Down) ||
                                   (!HasPrimaryTouch && event.Phase == TouchPhase::Began &&
                                    FindTouch(event.TouchId) == nullptr && !MouseDown[LeftButton]);
            if (!isPrimary)
            {
                // Further fingers only feed gestures, and never wait.
                LastPointerType = event.Pointer;
                HasPointerInput = true;
                UpdateTouch(event, time);
                return false;
            }

            switch (event.Phase)
            {
                case TouchPhase::Began:
                {
                    if (!event.Down)
                    {
                        if (anyButtonChanged)
                            return true;
                        LastPointerType = event.Pointer;
                        HasPointerInput = true;
                        IsPointerTouch = true;
                        HasPrimaryTouch = true;
                        PrimaryTouchId = event.TouchId;
                        PointerVelocity = Vec2();
                        UpdateTouch(event, time);
                        HasMousePos = true;
                        MousePos = event.Value;
                        mouseMoved = true;
                        // The press follows in the next frame, once the item under the finger is known.
                        event.Down = true;
                        return true;
                    }
                    if (buttonChanged[LeftButton] || mouseMoved)
                        return true;
                    if (!MouseDown[LeftButton])
                        setButton(LeftButton, true, MultiTapDistance);
                    return false;
                }
                case TouchPhase::Moved:
                {
                    // A move right after the press waits, so the press is seen where it happened.
                    if (anyButtonChanged)
                        return true;
                    LastPointerType = event.Pointer;
                    HasPointerInput = true;
                    UpdateTouch(event, time);
                    MousePos = event.Value;
                    mouseMoved = true;
                    return false;
                }
                case TouchPhase::Ended:
                case TouchPhase::Cancelled:
                {
                    if (!event.Down)
                    {
                        if (anyButtonChanged)
                            return true;
                        LastPointerType = event.Pointer;
                        HasPointerInput = true;
                        UpdateTouch(event, time);
                        HasMousePos = true;
                        MousePos = event.Value;
                        mouseMoved = true;
                        HasPrimaryTouch = false;
                        if (MouseDown[LeftButton])
                        {
                            setButton(LeftButton, false, MultiTapDistance);
                            IsPointerCancelled = event.Phase == TouchPhase::Cancelled;
                        }
                        // The pointer leaves in the next frame: there is no hover without a finger.
                        event.Down = true;
                        return true;
                    }
                    if (anyButtonChanged)
                        return true;
                    if (!HasPrimaryTouch)
                        HasMousePos = false;
                    mouseMoved = true;
                    return false;
                }
            }
            return false;
        };

        std::vector<InputEvent>& events = io.m_Events;
        size_t consumed = 0;
        for (; consumed < events.size(); consumed++)
        {
            InputEvent& event = events[consumed];
            bool defer = false;

            switch (event.Type)
            {
                case InputEventType::MousePos:
                case InputEventType::MouseLeave:
                {
                    // A finger drives the pointer: a mouse the host emulates from it must not move it elsewhere.
                    if (HasPrimaryTouch)
                        break;
                    // A move after a button change waits a frame, so the press or release is seen where it
                    // happened.
                    if (anyButtonChanged)
                    {
                        defer = true;
                        break;
                    }
                    HasMousePos = event.Type == InputEventType::MousePos;
                    if (HasMousePos)
                    {
                        MousePos = event.Value;
                        LastPointerType = PointerType::Mouse;
                        HasPointerInput = true;
                        IsPointerTouch = false;
                    }
                    mouseMoved = true;
                    break;
                }
                case InputEventType::MouseButton:
                {
                    const size_t button = static_cast<size_t>(event.Button);
                    if (HasPrimaryTouch && button == LeftButton)
                        break;
                    // A press right after a move waits a frame: hit testing knows the topmost item under the
                    // pointer from the previous frame, so the pointer has to be there for one frame first.
                    if (buttonChanged[button] || (event.Down && mouseMoved))
                    {
                        defer = true;
                        break;
                    }
                    if (MouseDown[button] == event.Down)
                        break;
                    LastPointerType = PointerType::Mouse;
                    HasPointerInput = true;
                    IsPointerTouch = false;
                    setButton(button, event.Down, MultiClickDistance);
                    break;
                }
                case InputEventType::MouseWheel:
                {
                    MouseWheel += event.Value;
                    LastPointerType = PointerType::Mouse;
                    HasPointerInput = true;
                    break;
                }
                case InputEventType::Touch:
                {
                    defer = applyTouch(event);
                    break;
                }
                case InputEventType::Key:
                {
                    const size_t key = static_cast<size_t>(event.KeyCode);
                    const bool isActionKey = !IsTextKey(event.KeyCode) && !IsModifierKey(event.KeyCode);
                    if (keyChanged[key] || (textEntered && isActionKey))
                    {
                        defer = true;
                        break;
                    }
                    if (KeyDown[key] == event.Down)
                        break; // Host-side key repeat; Carbon generates repeats itself.
                    KeyDown[key] = event.Down;
                    keyChanged[key] = true;
                    if (event.Down)
                    {
                        KeyPressed[key] = true;
                        KeyDownDuration[key] = 0.0f;
                        actionKeyPressed = actionKeyPressed || isActionKey;
                    }
                    else
                    {
                        KeyReleased[key] = true;
                    }
                    break;
                }
                case InputEventType::Character:
                {
                    if (actionKeyPressed)
                    {
                        defer = true;
                        break;
                    }
                    Characters.push_back(event.Character);
                    textEntered = true;
                    break;
                }
                case InputEventType::CompositionStart:
                case InputEventType::CompositionUpdate:
                case InputEventType::CompositionCommit:
                case InputEventType::CompositionCancel:
                {
                    // Compositions are text: they keep their order relative to editing keys, as characters do.
                    if (actionKeyPressed)
                    {
                        defer = true;
                        break;
                    }
                    ApplyComposition(io, event);
                    CompositionChanged = true;
                    textEntered = true;
                    break;
                }
                case InputEventType::FileDrag:
                case InputEventType::FileDrop:
                {
                    // Files over the display bring the pointer with them: the host gets no mouse events meanwhile.
                    HasMousePos = true;
                    MousePos = event.Value;
                    mouseMoved = true;
                    FileDrag.IsOver = event.Type == InputEventType::FileDrag;
                    FileDrag.IsDropped = event.Type == InputEventType::FileDrop;
                    FileDrag.SetPaths(std::string_view(io.m_EventText).substr(event.TextStart, event.TextLength),
                                      event.ClauseCount);
                    break;
                }
                case InputEventType::FileDragLeave:
                {
                    FileDrag.IsOver = false;
                    break;
                }
                case InputEventType::Focus:
                {
                    Focused = event.Down;
                    if (!Focused)
                        ReleaseAll();
                    break;
                }
            }

            if (defer)
                break;
        }
        events.erase(events.begin(), events.begin() + static_cast<std::ptrdiff_t>(consumed));
        // The text of composition events is stored next to the queue; it is no longer needed once the queue is
        // empty. Deferred events keep their offsets until then.
        if (events.empty())
        {
            io.m_EventText.clear();
            io.m_EventClauses.clear();
        }

        MouseDelta = (hadMousePos && HasMousePos) ? MousePos - previousMousePos : Vec2();

        // The velocity a pan keeps when the finger is lifted. A finger that rests for a moment first throws
        // nothing.
        if (IsPointerTouch && HasMousePos && deltaTime > 0.0f)
        {
            if (MouseDelta != Vec2())
            {
                PointerVelocity += (MouseDelta / deltaTime - PointerVelocity) * VelocitySmoothing;
                PointerRestTime = 0.0f;
            }
            else
            {
                PointerRestTime += deltaTime;
                if (PointerRestTime >= VelocityRestTime)
                    PointerVelocity = Vec2();
            }
        }

        for (size_t key = 0; key < KeyCount; key++)
        {
            if (!KeyDown[key] || KeyPressed[key])
                continue;
            const float previous = KeyDownDuration[key];
            const float current = previous + deltaTime;
            KeyDownDuration[key] = current;
            if (current < KeyRepeatDelay)
                continue;
            const float previousTicks =
                previous < KeyRepeatDelay ? -1.0f : std::floor((previous - KeyRepeatDelay) / KeyRepeatInterval);
            const float currentTicks = std::floor((current - KeyRepeatDelay) / KeyRepeatInterval);
            KeyRepeated[key] = currentTicks > previousTicks;
        }

        Modifiers = KeyModifiers::None;
        if (KeyDown[static_cast<size_t>(Key::LeftCtrl)] || KeyDown[static_cast<size_t>(Key::RightCtrl)])
            Modifiers = Modifiers | KeyModifiers::Ctrl;
        if (KeyDown[static_cast<size_t>(Key::LeftShift)] || KeyDown[static_cast<size_t>(Key::RightShift)])
            Modifiers = Modifiers | KeyModifiers::Shift;
        if (KeyDown[static_cast<size_t>(Key::LeftAlt)] || KeyDown[static_cast<size_t>(Key::RightAlt)])
            Modifiers = Modifiers | KeyModifiers::Alt;
        if (KeyDown[static_cast<size_t>(Key::LeftSuper)] || KeyDown[static_cast<size_t>(Key::RightSuper)])
            Modifiers = Modifiers | KeyModifiers::Super;

        // Shift turns a plain wheel sideways, as macOS does for every application.
        if (HasModifiers(Modifiers, KeyModifiers::Shift) && MouseWheel.X == 0.0f)
            MouseWheel = Vec2(MouseWheel.Y, 0.0f);
    }

    TouchPoint* InputState::FindTouch(uint64_t id)
    {
        for (size_t i = 0; i < TouchCount; i++)
        {
            if (Touches[i].Id == id)
                return &Touches[i];
        }
        return nullptr;
    }

    void InputState::UpdateTouch(const InputEvent& event, double time)
    {
        TouchPoint* touch = FindTouch(event.TouchId);
        switch (event.Phase)
        {
            case TouchPhase::Began:
            case TouchPhase::Moved:
            {
                if (touch == nullptr)
                {
                    // A move of a finger that never began is taken as its beginning.
                    if (TouchCount == MaxTouches)
                        return;
                    touch = &Touches[TouchCount++];
                    touch->Id = event.TouchId;
                    touch->StartPosition = event.Value;
                    touch->StartTime = time;
                }
                touch->Type = event.Pointer;
                touch->Position = event.Value;
                break;
            }
            case TouchPhase::Ended:
            case TouchPhase::Cancelled:
            {
                if (touch == nullptr)
                    return;
                // The other fingers keep their order.
                const size_t index = static_cast<size_t>(touch - Touches.data());
                for (size_t i = index; i + 1 < TouchCount; i++)
                    Touches[i] = Touches[i + 1];
                TouchCount--;
                break;
            }
        }
    }

    void FileDragState::SetPaths(std::string_view text, size_t count)
    {
        Paths.assign(text);
        Files.clear();
        size_t start = 0;
        for (size_t i = 0; i < count && start <= Paths.size(); i++)
        {
            const size_t end = std::min(Paths.find('\0', start), Paths.size());
            Files.emplace_back(Paths.data() + start, end - start);
            start = end + 1;
        }
    }

    void CompositionState::Clear()
    {
        IsActive = false;
        Text.clear();
        Caret = 0;
        Clauses.clear();
    }

    void InputState::ApplyComposition(const IO& io, const InputEvent& event)
    {
        const std::string_view text = std::string_view(io.m_EventText).substr(event.TextStart, event.TextLength);
        switch (event.Type)
        {
            case InputEventType::CompositionStart:
            {
                Composition.Clear();
                Composition.IsActive = true;
                break;
            }
            case InputEventType::CompositionUpdate:
            {
                // An input method that is told to cancel may still send an empty update; it starts nothing.
                if (!Composition.IsActive && text.empty())
                    break;
                Composition.IsActive = true;
                Composition.Text.assign(text);
                Composition.Caret = event.Caret;
                const CompositionClause* clauses = io.m_EventClauses.data() + event.ClauseStart;
                Composition.Clauses.assign(clauses, clauses + event.ClauseCount);
                break;
            }
            case InputEventType::CompositionCommit:
            {
                // Committed text is typed text; control characters are dropped as AddInputCharacter does.
                size_t offset = 0;
                while (offset < text.size())
                {
                    const UTF8Decoded decoded = DecodeUTF8(text, offset);
                    if (decoded.Codepoint >= 0x20 && decoded.Codepoint != 0x7F)
                        Characters.push_back(decoded.Codepoint);
                    offset += decoded.Length;
                }
                Composition.Clear();
                break;
            }
            case InputEventType::CompositionCancel:
            {
                Composition.Clear();
                break;
            }
            default:
                break;
        }
    }

    void InputState::ReleaseAll()
    {
        for (size_t button = 0; button < MouseButtonCount; button++)
        {
            if (MouseDown[button])
            {
                MouseDown[button] = false;
                MouseReleased[button] = true;
            }
        }
        for (size_t key = 0; key < KeyCount; key++)
        {
            if (KeyDown[key])
            {
                KeyDown[key] = false;
                KeyReleased[key] = true;
            }
        }
        // Fingers are lost too; one that drove the pointer takes the pointer with it and activates nothing.
        TouchCount = 0;
        if (HasPrimaryTouch)
        {
            HasPrimaryTouch = false;
            HasMousePos = false;
            IsPointerCancelled = true;
        }
    }
} // namespace Carbon::Internal
