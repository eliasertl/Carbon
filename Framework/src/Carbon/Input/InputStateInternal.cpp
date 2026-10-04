#include "Carbon/Input/InputStateInternal.h"

#include <cmath>

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

        bool mouseMoved = false;
        bool textEntered = false;
        bool actionKeyPressed = false;
        std::array<bool, MouseButtonCount> buttonChanged{};
        std::array<bool, KeyCount> keyChanged{};
        bool anyButtonChanged = false;

        std::vector<InputEvent>& events = io.m_Events;
        size_t consumed = 0;
        for (; consumed < events.size(); consumed++)
        {
            const InputEvent& event = events[consumed];
            bool defer = false;

            switch (event.Type)
            {
                case InputEventType::MousePos:
                case InputEventType::MouseLeave:
                {
                    // A move after a button change waits a frame, so the press or release is seen where it
                    // happened.
                    if (anyButtonChanged)
                    {
                        defer = true;
                        break;
                    }
                    HasMousePos = event.Type == InputEventType::MousePos;
                    if (HasMousePos)
                        MousePos = event.Value;
                    mouseMoved = true;
                    break;
                }
                case InputEventType::MouseButton:
                {
                    const size_t button = static_cast<size_t>(event.Button);
                    // A press right after a move waits a frame: hit testing knows the topmost item under the
                    // pointer from the previous frame, so the pointer has to be there for one frame first.
                    if (buttonChanged[button] || (event.Down && mouseMoved))
                    {
                        defer = true;
                        break;
                    }
                    if (MouseDown[button] == event.Down)
                        break;
                    MouseDown[button] = event.Down;
                    buttonChanged[button] = true;
                    anyButtonChanged = true;
                    if (event.Down)
                    {
                        const Vec2 travel = MousePos - MousePressedPos[button];
                        const bool isMultiClick = MouseLastClickCount[button] > 0 &&
                                                  time - MouseLastPressTime[button] <= MultiClickTime &&
                                                  travel.GetLengthSquared() <= MultiClickDistance * MultiClickDistance;
                        const uint8_t count = isMultiClick ? static_cast<uint8_t>(MouseLastClickCount[button] + 1) : 1;
                        MousePressed[button] = true;
                        MouseClickCount[button] = count;
                        MouseLastClickCount[button] = count;
                        MouseLastPressTime[button] = time;
                        MousePressedPos[button] = MousePos;
                    }
                    else
                    {
                        MouseReleased[button] = true;
                    }
                    break;
                }
                case InputEventType::MouseWheel:
                {
                    MouseWheel += event.Value;
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

        MouseDelta = (hadMousePos && HasMousePos) ? MousePos - previousMousePos : Vec2();

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
    }
} // namespace Carbon::Internal
