#include "Carbon/Input/IO.h"

#include <algorithm>

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

    void IO::SetSafeAreaInsets(const EdgeInsets& insets)
    {
        m_SafeAreaInsets = EdgeInsets(std::max(insets.Left, 0.0f), std::max(insets.Top, 0.0f),
                                      std::max(insets.Right, 0.0f), std::max(insets.Bottom, 0.0f));
    }

    void IO::SetTextScale(float scale)
    {
        CB_VERIFY(scale > 0.0f, "Text scale must be positive ({})", scale);
        m_TextScale = scale > 0.0f ? std::clamp(scale, 0.5f, 3.0f) : 1.0f;
    }

    void IO::SetKeyboardRect(const Rect& rect)
    {
        m_KeyboardRect = rect.Width > 0.0f && rect.Height > 0.0f ? rect : Rect();
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

    void IO::AddTouchEvent(TouchPhase phase, uint64_t id, float x, float y, PointerType type)
    {
        CB_VERIFY(type != PointerType::Mouse, "Touch events come from a finger or a pen, not a mouse");
        InputEvent event;
        event.Type = InputEventType::Touch;
        event.Value = Vec2(x, y);
        event.TouchId = id;
        event.Phase = phase;
        event.Pointer = type == PointerType::Mouse ? PointerType::Touch : type;
        m_Events.push_back(event);
    }

    namespace
    {
        // Moves an offset into the text and back onto the start of a character.
        size_t ToCharacterBoundary(std::string_view text, size_t offset)
        {
            offset = std::min(offset, text.size());
            while (offset > 0 && offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xC0) == 0x80)
                offset--;
            return offset;
        }
    } // namespace

    void IO::AddCompositionStartEvent()
    {
        InputEvent event;
        event.Type = InputEventType::CompositionStart;
        m_Events.push_back(event);
    }

    void IO::AddCompositionUpdateEvent(std::string_view text, size_t caret, std::span<const CompositionClause> clauses)
    {
        CB_VERIFY(caret <= text.size(), "Composition caret {} is outside its text of {} bytes", caret, text.size());
        InputEvent event;
        event.Type = InputEventType::CompositionUpdate;
        event.TextStart = static_cast<uint32_t>(m_EventText.size());
        event.TextLength = static_cast<uint32_t>(text.size());
        event.Caret = static_cast<uint32_t>(ToCharacterBoundary(text, caret));
        event.ClauseStart = static_cast<uint32_t>(m_EventClauses.size());
        m_EventText.append(text);

        // Clauses are kept in order, inside the text and on character boundaries; empty ones are dropped.
        size_t previousEnd = 0;
        for (const CompositionClause& clause : clauses)
        {
            CB_VERIFY(clause.Start <= clause.End && clause.End <= text.size(),
                      "Composition clause [{}, {}) is outside its text of {} bytes", clause.Start, clause.End,
                      text.size());
            CompositionClause stored;
            stored.Start = std::max(ToCharacterBoundary(text, clause.Start), previousEnd);
            stored.End = ToCharacterBoundary(text, clause.End);
            stored.IsActive = clause.IsActive;
            if (stored.End <= stored.Start)
                continue;
            m_EventClauses.push_back(stored);
            previousEnd = stored.End;
        }
        event.ClauseCount = static_cast<uint32_t>(m_EventClauses.size()) - event.ClauseStart;
        m_Events.push_back(event);
    }

    void IO::AddCompositionCommitEvent(std::string_view text)
    {
        InputEvent event;
        event.Type = InputEventType::CompositionCommit;
        event.TextStart = static_cast<uint32_t>(m_EventText.size());
        event.TextLength = static_cast<uint32_t>(text.size());
        m_EventText.append(text);
        m_Events.push_back(event);
    }

    void IO::AddCompositionCancelEvent()
    {
        InputEvent event;
        event.Type = InputEventType::CompositionCancel;
        m_Events.push_back(event);
    }

    void IO::AddTextReplaceEvent(size_t start, size_t end, std::string_view text)
    {
        CB_VERIFY(start <= end, "A text replacement must not end before it starts ({} > {})", start, end);
        InputEvent event;
        event.Type = InputEventType::TextReplace;
        event.ReplaceStart = static_cast<uint32_t>(std::min(start, end));
        event.ReplaceEnd = static_cast<uint32_t>(end);
        event.TextStart = static_cast<uint32_t>(m_EventText.size());
        event.TextLength = static_cast<uint32_t>(text.size());
        m_EventText.append(text);
        m_Events.push_back(event);
    }

    void IO::AddFileDragEvent(float x, float y, std::span<const std::string_view> paths)
    {
        AddFileEvent(InputEventType::FileDrag, Vec2(x, y), paths);
    }

    void IO::AddFileDragLeaveEvent()
    {
        InputEvent event;
        event.Type = InputEventType::FileDragLeave;
        m_Events.push_back(event);
    }

    void IO::AddFileDropEvent(float x, float y, std::span<const std::string_view> paths)
    {
        AddFileEvent(InputEventType::FileDrop, Vec2(x, y), paths);
    }

    void IO::AddFileEvent(InputEventType type, Vec2 position, std::span<const std::string_view> paths)
    {
        InputEvent event;
        event.Type = type;
        event.Value = position;
        event.TextStart = static_cast<uint32_t>(m_EventText.size());
        for (const std::string_view path : paths)
        {
            m_EventText.append(path);
            m_EventText.push_back('\0');
        }
        event.TextLength = static_cast<uint32_t>(m_EventText.size()) - event.TextStart;
        event.ClauseCount = static_cast<uint32_t>(paths.size());
        m_Events.push_back(event);
    }

    IO& GetIO()
    {
        return Internal::GetContext().HostIO;
    }
} // namespace Carbon
