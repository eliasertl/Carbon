#include "AndroidText.h"

#include <algorithm>

#include <game-activity/GameActivity.h>

#include <Carbon/Carbon.h>

namespace AndroidGallery
{
    namespace
    {
        bool IsContinuationByte(char byte)
        {
            return (static_cast<unsigned char>(byte) & 0xC0) == 0x80;
        }

        // The EditorInfo of the keyboard Carbon asks for: its kind, what its action key does, and that it never
        // covers the app with a full-screen editor in landscape.
        void SetEditorInfo(GameActivity* activity, const Carbon::TextInputState& state)
        {
            uint32_t type = TYPE_CLASS_TEXT;
            uint32_t action = IME_ACTION_DONE;
            uint32_t options = IME_FLAG_NO_FULLSCREEN | IME_FLAG_NO_EXTRACT_UI;
            switch (state.Keyboard)
            {
                case Carbon::KeyboardType::Text:
                    type = TYPE_CLASS_TEXT | TYPE_TEXT_FLAG_CAP_SENTENCES | TYPE_TEXT_FLAG_AUTO_CORRECT;
                    break;
                case Carbon::KeyboardType::Number:
                    type = TYPE_CLASS_NUMBER | TYPE_NUMBER_FLAG_DECIMAL | TYPE_NUMBER_FLAG_SIGNED;
                    break;
                case Carbon::KeyboardType::Email:
                    type = TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
                    break;
                case Carbon::KeyboardType::URL:
                    type = TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_URI;
                    action = IME_ACTION_GO;
                    break;
                case Carbon::KeyboardType::Search:
                    type = TYPE_CLASS_TEXT;
                    action = IME_ACTION_SEARCH;
                    break;
            }
            if (state.IsSecure)
            {
                type = TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_PASSWORD;
                options |= IME_FLAG_NO_PERSONALIZED_LEARNING;
            }
            if (state.IsMultiLine)
            {
                // Return inserts a line break instead of acting.
                type |= TYPE_TEXT_FLAG_MULTI_LINE;
                action = IME_ACTION_UNSPECIFIED;
                options |= IME_FLAG_NO_ENTER_ACTION;
            }
            // The action goes into the options too: that is where keyboards read it (EditorInfo.imeOptions).
            GameActivity_setImeEditorInfo(activity, static_cast<GameTextInputType>(type),
                                          static_cast<GameTextInputActionType>(action),
                                          static_cast<GameTextInputImeOptions>(options | action));
        }
    } // namespace

    void ModifiedUTF8ToUTF8(std::string_view modified, std::string& utf8)
    {
        utf8.clear();
        size_t i = 0;
        while (i < modified.size())
        {
            const unsigned char byte = static_cast<unsigned char>(modified[i]);
            // NUL is C0 80.
            if (byte == 0xC0 && i + 1 < modified.size() && static_cast<unsigned char>(modified[i + 1]) == 0x80)
            {
                utf8.push_back('\0');
                i += 2;
                continue;
            }
            // A high surrogate (ED A0..AF xx) followed by a low one (ED B0..BF xx) is one supplementary character.
            if (byte == 0xED && i + 5 < modified.size() &&
                (static_cast<unsigned char>(modified[i + 1]) & 0xF0) == 0xA0 &&
                static_cast<unsigned char>(modified[i + 3]) == 0xED &&
                (static_cast<unsigned char>(modified[i + 4]) & 0xF0) == 0xB0)
            {
                const uint32_t high = 0xD800u | ((static_cast<unsigned char>(modified[i + 1]) & 0x0Fu) << 6) |
                                      (static_cast<unsigned char>(modified[i + 2]) & 0x3Fu);
                const uint32_t low = 0xDC00u | ((static_cast<unsigned char>(modified[i + 4]) & 0x0Fu) << 6) |
                                     (static_cast<unsigned char>(modified[i + 5]) & 0x3Fu);
                Carbon::AppendUTF8(utf8, static_cast<char32_t>(0x10000u + ((high - 0xD800u) << 10) + (low - 0xDC00u)));
                i += 6;
                continue;
            }
            utf8.push_back(modified[i]);
            i++;
        }
    }

    void UTF8ToModifiedUTF8(std::string_view utf8, std::string& modified)
    {
        modified.clear();
        size_t i = 0;
        while (i < utf8.size())
        {
            const unsigned char byte = static_cast<unsigned char>(utf8[i]);
            if (byte == 0)
            {
                modified.append("\xC0\x80");
                i++;
                continue;
            }
            if ((byte & 0xF8) == 0xF0 && i + 3 < utf8.size())
            {
                const uint32_t codepoint = ((byte & 0x07u) << 18) |
                                           ((static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu) << 12) |
                                           ((static_cast<unsigned char>(utf8[i + 2]) & 0x3Fu) << 6) |
                                           (static_cast<unsigned char>(utf8[i + 3]) & 0x3Fu);
                const uint32_t offset = codepoint - 0x10000u;
                for (const uint32_t surrogate : {0xD800u + (offset >> 10), 0xDC00u + (offset & 0x3FFu)})
                {
                    modified.push_back(static_cast<char>(0xE0u | (surrogate >> 12)));
                    modified.push_back(static_cast<char>(0x80u | ((surrogate >> 6) & 0x3Fu)));
                    modified.push_back(static_cast<char>(0x80u | (surrogate & 0x3Fu)));
                }
                i += 4;
                continue;
            }
            modified.push_back(utf8[i]);
            i++;
        }
    }

    size_t UTF16IndexToByteOffset(std::string_view text, size_t index)
    {
        size_t units = 0;
        size_t offset = 0;
        while (offset < text.size() && units < index)
        {
            const unsigned char byte = static_cast<unsigned char>(text[offset]);
            const size_t length = byte < 0x80 ? 1 : (byte & 0xE0) == 0xC0 ? 2 : (byte & 0xF0) == 0xE0 ? 3 : 4;
            units += length == 4 ? 2 : 1;
            offset = std::min(offset + length, text.size());
        }
        return offset;
    }

    size_t ByteOffsetToUTF16Index(std::string_view text, size_t offset)
    {
        offset = std::min(offset, text.size());
        size_t units = 0;
        for (size_t i = 0; i < offset; i++)
        {
            const unsigned char byte = static_cast<unsigned char>(text[i]);
            if (!IsContinuationByte(static_cast<char>(byte)))
                units += (byte & 0xF8) == 0xF0 ? 2 : 1;
        }
        return units;
    }

    void AndroidText::RequestKeyboard(bool visible)
    {
        m_HasRequest = true;
        m_IsRequestVisible = visible;
    }

    void AndroidText::ReadKeyboardState(const GameTextInputState& state)
    {
        ModifiedUTF8ToUTF8(std::string_view(state.text_UTF8, static_cast<size_t>(std::max(state.text_length, 0))),
                           m_KeyboardText);
        const size_t start =
            UTF16IndexToByteOffset(m_KeyboardText, static_cast<size_t>(std::max(state.selection.start, 0)));
        const size_t end =
            UTF16IndexToByteOffset(m_KeyboardText, static_cast<size_t>(std::max(state.selection.end, 0)));
        m_KeyboardSelectionStart = std::min(start, end);
        m_KeyboardSelectionEnd = std::max(start, end);
    }

    void AndroidText::ApplyKeyboardChanges(GameActivity* activity, bool hasChanged)
    {
        if (!hasChanged || !m_IsKeyboardShown)
            return;
        GameActivity_getTextInputState(
            activity, [](void* context, const GameTextInputState* state)
            { static_cast<AndroidText*>(context)->ReadKeyboardState(*state); }, this);

        Carbon::IO& io = Carbon::GetIO();
        if (!io.WantsTextInput())
            return;
        const Carbon::TextInputState& carbon = io.GetTextInputState();
        const std::string_view before = carbon.Text;
        const std::string_view after = m_KeyboardText;

        // The changed part: what lies between the longest common start and the longest common end, both cut back
        // to the start of a character.
        size_t caret = carbon.SelectionStart == carbon.SelectionEnd ? carbon.SelectionEnd : std::string_view::npos;
        if (before != after)
        {
            const size_t shorter = std::min(before.size(), after.size());
            size_t prefix = 0;
            while (prefix < shorter && before[prefix] == after[prefix])
                prefix++;
            while (prefix > 0 && ((prefix < before.size() && IsContinuationByte(before[prefix])) ||
                                  (prefix < after.size() && IsContinuationByte(after[prefix]))))
                prefix--;
            size_t suffix = 0;
            while (suffix < shorter - prefix && before[before.size() - 1 - suffix] == after[after.size() - 1 - suffix])
                suffix++;
            while (suffix > 0 && (IsContinuationByte(before[before.size() - suffix]) ||
                                  IsContinuationByte(after[after.size() - suffix])))
                suffix--;
            const std::string_view inserted = after.substr(prefix, after.size() - suffix - prefix);
            io.AddTextReplaceEvent(prefix, before.size() - suffix, inserted);
            caret = prefix + inserted.size();
        }
        // A caret the keyboard moved (its space bar slides it): an empty replacement there moves Carbon's.
        if (m_KeyboardSelectionStart == m_KeyboardSelectionEnd && m_KeyboardSelectionEnd != caret)
            io.AddTextReplaceEvent(m_KeyboardSelectionEnd, m_KeyboardSelectionEnd, {});
    }

    void AndroidText::SendCarbonState(GameActivity* activity)
    {
        const Carbon::TextInputState& carbon = Carbon::GetIO().GetTextInputState();
        m_KeyboardText.assign(carbon.Text);
        m_KeyboardSelectionStart = std::min(carbon.SelectionStart, carbon.Text.size());
        m_KeyboardSelectionEnd = std::min(carbon.SelectionEnd, carbon.Text.size());

        UTF8ToModifiedUTF8(m_KeyboardText, m_Scratch);
        GameTextInputState state = {};
        state.text_UTF8 = m_Scratch.c_str();
        state.text_length = static_cast<int32_t>(m_Scratch.size());
        state.selection.start = static_cast<int32_t>(ByteOffsetToUTF16Index(m_KeyboardText, m_KeyboardSelectionStart));
        state.selection.end = static_cast<int32_t>(ByteOffsetToUTF16Index(m_KeyboardText, m_KeyboardSelectionEnd));
        state.composingRegion = {SPAN_UNDEFINED, SPAN_UNDEFINED};
        GameActivity_setTextInputState(activity, &state);
    }

    void AndroidText::Update(GameActivity* activity)
    {
        const Carbon::IO& io = Carbon::GetIO();
        if (m_HasRequest)
        {
            m_HasRequest = false;
            if (m_IsRequestVisible && io.WantsTextInput())
            {
                SetEditorInfo(activity, io.GetTextInputState());
                SendCarbonState(activity);
                GameActivity_showSoftInput(activity, 0);
                m_IsKeyboardShown = true;
            }
            else
            {
                GameActivity_hideSoftInput(activity, 0);
                m_IsKeyboardShown = false;
            }
            return;
        }
        if (!m_IsKeyboardShown || !io.WantsTextInput())
            return;
        // Carbon's text changed by other means than the keyboard: the keyboard continues from Carbon's.
        const Carbon::TextInputState& carbon = io.GetTextInputState();
        if (carbon.Text != m_KeyboardText || carbon.SelectionStart != m_KeyboardSelectionStart ||
            carbon.SelectionEnd != m_KeyboardSelectionEnd)
            SendCarbonState(activity);
    }
} // namespace AndroidGallery
