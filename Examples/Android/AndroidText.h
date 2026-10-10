#pragma once

#include <cstddef>
#include <string>
#include <string_view>

struct GameActivity;
struct GameTextInputState;

namespace AndroidGallery
{
    /// Keeps Android's soft keyboard and the text control Carbon edits in step.
    ///
    /// The keyboard edits a copy of the text (GameTextInput, behind an InputConnection), so that suggestions,
    /// autocorrection, gesture typing and voice input work. Its changes reach Carbon as one AddTextReplaceEvent per
    /// frame, the difference between the text Carbon reported and the keyboard's; a moved caret follows as an empty
    /// replacement. When Carbon's text or caret changes by other means (a hardware key, a paste, another field),
    /// the keyboard gets Carbon's state. GameTextInput holds modified UTF-8 and counts the selection in UTF-16 code
    /// units, as Java does; Carbon uses UTF-8 and byte offsets, so both are converted.
    class AndroidText
    {
    public:
        /// Carbon's SetKeyboardVisible callback: remembers the request until the end of the frame.
        void RequestKeyboard(bool visible);
        /// Before NewFrame: reads the keyboard's state when it changed and queues the difference to Carbon.
        void ApplyKeyboardChanges(GameActivity* activity, bool hasChanged);
        /// After EndFrame: shows, hides and configures the keyboard, and gives it Carbon's text when it differs.
        void Update(GameActivity* activity);
        /// True while the keyboard is shown for a text control.
        bool IsKeyboardShown() const { return m_IsKeyboardShown; }

    private:
        void ReadKeyboardState(const GameTextInputState& state);
        void SendCarbonState(GameActivity* activity);

    private:
        /// What the keyboard has, as UTF-8 with byte offsets.
        std::string m_KeyboardText;
        size_t m_KeyboardSelectionStart = 0;
        size_t m_KeyboardSelectionEnd = 0;
        /// Storage for converting to and from GameTextInput's encoding; reused.
        std::string m_Scratch;
        bool m_HasRequest = false;
        bool m_IsRequestVisible = false;
        bool m_IsKeyboardShown = false;
    };

    /// Converts Java's modified UTF-8 (surrogate pairs encoded one by one, NUL as two bytes) to UTF-8.
    void ModifiedUTF8ToUTF8(std::string_view modified, std::string& utf8);
    /// Converts UTF-8 to Java's modified UTF-8.
    void UTF8ToModifiedUTF8(std::string_view utf8, std::string& modified);
    /// The byte offset in UTF-8 `text` of the UTF-16 code unit `index`; clamped to the text.
    size_t UTF16IndexToByteOffset(std::string_view text, size_t index);
    /// The UTF-16 code unit index of the byte offset `offset` in UTF-8 `text`.
    size_t ByteOffsetToUTF16Index(std::string_view text, size_t offset);
} // namespace AndroidGallery
