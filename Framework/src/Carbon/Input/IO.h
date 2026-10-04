#pragma once

#include <string_view>
#include <vector>

#include "Carbon/Core/Vec2.h"
#include "Carbon/Input/InputEvent.h"
#include "Carbon/Input/Key.h"
#include "Carbon/Input/MouseButton.h"

namespace Carbon
{
    namespace Internal
    {
        struct InputState;
        struct InteractionState;
    } // namespace Internal

    /// The host's side of a context: display metrics, timing and input. The host sets the display size, content
    /// scale and delta time and queues input events before every NewFrame; Carbon applies them in NewFrame.
    class IO
    {
    public:
        /// Size of the area Carbon lays out and draws into, in logical points.
        void SetDisplaySize(float width, float height);
        Vec2 GetDisplaySize() const { return m_DisplaySize; }

        /// Pixels per point, e.g. 1.5 on a display scaled to 150 %. Text is rasterized at this scale.
        void SetContentScale(float scale);
        float GetContentScale() const { return m_ContentScale; }

        /// Seconds since the previous frame. Drives animations and key repeat.
        void SetDeltaTime(float seconds);
        float GetDeltaTime() const { return m_DeltaTime; }

        /// Queues a mouse move; the position is in points relative to the top-left of the display area.
        void AddMousePosEvent(float x, float y);
        /// Queues the mouse leaving the display area; nothing is hovered until the next position event.
        void AddMouseLeaveEvent();
        /// Queues a mouse button press or release.
        void AddMouseButtonEvent(MouseButton button, bool down);
        /// Queues a scroll; one unit is one wheel notch (a "line"). Positive y scrolls content down.
        void AddMouseWheelEvent(float x, float y);
        /// Queues a key press or release. Repeated presses of a held key are ignored; Carbon repeats itself.
        void AddKeyEvent(Key key, bool down);
        /// Queues one typed character.
        void AddInputCharacter(char32_t codepoint);
        /// Queues typed text encoded as UTF-8.
        void AddInputCharactersUTF8(std::string_view text);
        /// Queues the host window gaining or losing focus. Losing focus releases all keys and buttons.
        void AddFocusEvent(bool focused);

        /// The modifier used for shortcuts such as copy and paste. Ctrl by default; a host may choose Super.
        void SetShortcutModifier(KeyModifiers modifier) { m_ShortcutModifier = modifier; }
        KeyModifiers GetShortcutModifier() const { return m_ShortcutModifier; }

        /// True when Carbon is using the mouse (hovering or dragging a widget); the host should not also use it.
        bool WantsMouse() const { return m_WantsMouse; }
        /// True when a Carbon widget has keyboard focus.
        bool WantsKeyboard() const { return m_WantsKeyboard; }
        /// True when a text field is being edited; the host may show an on-screen keyboard or enable text input.
        bool WantsTextInput() const { return m_WantsTextInput; }

    private:
        friend struct Internal::InputState;
        friend struct Internal::InteractionState;

    private:
        Vec2 m_DisplaySize;
        float m_ContentScale = 1.0f;
        float m_DeltaTime = 1.0f / 60.0f;
        KeyModifiers m_ShortcutModifier = KeyModifiers::Ctrl;
        std::vector<InputEvent> m_Events;
        bool m_WantsMouse = false;
        bool m_WantsKeyboard = false;
        bool m_WantsTextInput = false;
    };

    /// Returns the IO object of the current context.
    IO& GetIO();
} // namespace Carbon
