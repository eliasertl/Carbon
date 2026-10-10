#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Carbon/Core/Rect.h"
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

        /// Queues the start of an input method composition (Japanese, Chinese, Korean and similar input): the
        /// text the user composes is shown at the caret of the text being edited until it is committed. Optional:
        /// an update starts a composition by itself.
        void AddCompositionStartEvent();
        /// Queues the composition's current pre-edit text (UTF-8), the caret inside it as a byte offset, and
        /// optionally its clauses. Replaces the previous pre-edit text; it is not inserted into any text yet.
        void AddCompositionUpdateEvent(std::string_view text, size_t caret,
                                       std::span<const CompositionClause> clauses = {});
        /// Queues the end of a composition with the text it produced (UTF-8), which is inserted like typed text.
        /// Also inserts text when no composition is in progress.
        void AddCompositionCommitEvent(std::string_view text);
        /// Queues the end of a composition without any text: the user abandoned it.
        void AddCompositionCancelEvent();

        /// Queues files dragged over the display area from outside the application (from the system's file
        /// manager), at a position in points: call it whenever the drag moves. Carbon treats it as a drag of
        /// FilesPayloadType (Carbon/Interaction/DragDrop.h), so that drop targets for files highlight under it.
        /// Optional: a host that learns about files only when they are dropped calls AddFileDropEvent alone.
        void AddFileDragEvent(float x, float y, std::span<const std::string_view> paths);
        /// Queues the end of a drag of files without a drop: they left the display area, or the user cancelled.
        void AddFileDragLeaveEvent();
        /// Queues files dropped onto the display area at a position in points (UTF-8 paths). The drop target for
        /// files under that position receives them during the frame after the next.
        void AddFileDropEvent(float x, float y, std::span<const std::string_view> paths);

        /// The modifier used for shortcuts such as copy and paste. Ctrl by default; a host may choose Super.
        void SetShortcutModifier(KeyModifiers modifier) { m_ShortcutModifier = modifier; }
        KeyModifiers GetShortcutModifier() const { return m_ShortcutModifier; }

        /// True when Carbon is using the mouse (hovering or dragging a widget); the host should not also use it.
        bool WantsMouse() const { return m_WantsMouse; }
        /// True when a Carbon widget has keyboard focus.
        bool WantsKeyboard() const { return m_WantsKeyboard; }
        /// True when a text field is being edited; the host may show an on-screen keyboard or enable text input
        /// and its input method.
        bool WantsTextInput() const { return m_WantsTextInput; }
        /// The caret of the text being edited, in points, while WantsTextInput() is true; an empty rectangle
        /// otherwise. During a composition, the start of the clause being converted, or the caret inside the
        /// pre-edit text. The host places the input method's candidate window next to it.
        Rect GetCaretRect() const { return m_CaretRect; }
        /// True after a frame in which Carbon ended a composition itself, because the user clicked or moved the
        /// focus: the pre-edit text was inserted as it stood. The host cancels the input method's composition so
        /// that it does not commit the same text again.
        bool WantsCompositionCancel() const { return m_WantsCompositionCancel; }

    private:
        void AddFileEvent(InputEventType type, Vec2 position, std::span<const std::string_view> paths);

    private:
        friend struct Internal::InputState;
        friend struct Internal::InteractionState;

    private:
        Vec2 m_DisplaySize;
        float m_ContentScale = 1.0f;
        float m_DeltaTime = 1.0f / 60.0f;
        KeyModifiers m_ShortcutModifier = KeyModifiers::Ctrl;
        std::vector<InputEvent> m_Events;
        /// The text and clauses of queued composition events; emptied whenever the queue is.
        std::string m_EventText;
        std::vector<CompositionClause> m_EventClauses;
        bool m_WantsMouse = false;
        bool m_WantsKeyboard = false;
        bool m_WantsTextInput = false;
        bool m_WantsCompositionCancel = false;
        Rect m_CaretRect;
    };

    /// Returns the IO object of the current context.
    IO& GetIO();
} // namespace Carbon
