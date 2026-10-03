#include "Carbon/Input/IO.h"

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/UTF8.h"

namespace Carbon
{
    void IO::SetDisplaySize(float width, float height)
    {
        CB_VERIFY(width >= 0.0f && height >= 0.0f, "Display size must not be negative ({} x {})", width, height);
        m_DisplaySize = Vec2(width > 0.0f ? width : 0.0f, height > 0.0f ? height : 0.0f);
    }

    void IO::SetContentScale(float scale)
    {
        CB_VERIFY(scale > 0.0f, "Content scale must be positive ({})", scale);
        m_ContentScale = scale > 0.0f ? scale : 1.0f;
    }

    void IO::SetDeltaTime(float seconds)
    {
        CB_VERIFY(seconds >= 0.0f, "Delta time must not be negative ({})", seconds);
        m_DeltaTime = seconds > 0.0f ? seconds : 0.0f;
    }

    void IO::AddMousePosEvent(float x, float y)
    {
        InputEvent event;
        event.Type = InputEventType::MousePos;
        event.Value = Vec2(x, y);
        m_Events.push_back(event);
    }

    void IO::AddMouseLeaveEvent()
    {
        InputEvent event;
        event.Type = InputEventType::MouseLeave;
        m_Events.push_back(event);
    }

    void IO::AddMouseButtonEvent(MouseButton button, bool down)
    {
        CB_VERIFY(button < MouseButton::Count, "Invalid mouse button {}", static_cast<int>(button));
        if (button >= MouseButton::Count)
            return;
        InputEvent event;
        event.Type = InputEventType::MouseButton;
        event.Button = button;
        event.Down = down;
        m_Events.push_back(event);
    }

    void IO::AddMouseWheelEvent(float x, float y)
    {
        InputEvent event;
        event.Type = InputEventType::MouseWheel;
        event.Value = Vec2(x, y);
        m_Events.push_back(event);
    }

    void IO::AddKeyEvent(Key key, bool down)
    {
        if (key == Key::None || key >= Key::Count)
            return;
        InputEvent event;
        event.Type = InputEventType::Key;
        event.KeyCode = key;
        event.Down = down;
        m_Events.push_back(event);
    }

    void IO::AddInputCharacter(char32_t codepoint)
    {
        // Control characters arrive as key events (Enter, Tab, Backspace); they are not text.
        if (codepoint < 0x20 || codepoint == 0x7F)
            return;
        InputEvent event;
        event.Type = InputEventType::Character;
        event.Character = codepoint;
        m_Events.push_back(event);
    }

    void IO::AddInputCharactersUTF8(std::string_view text)
    {
        size_t offset = 0;
        while (offset < text.size())
        {
            const UTF8Decoded decoded = DecodeUTF8(text, offset);
            AddInputCharacter(decoded.Codepoint);
            offset += decoded.Length;
        }
    }

    void IO::AddFocusEvent(bool focused)
    {
        InputEvent event;
        event.Type = InputEventType::Focus;
        event.Down = focused;
        m_Events.push_back(event);
    }

    IO& GetIO()
    {
        return Internal::GetContext().HostIO;
    }
} // namespace Carbon
