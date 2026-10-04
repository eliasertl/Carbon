#include "Carbon/Widgets/Internal/TextEditor.h"

#include <algorithm>

#include "Carbon/Core/UTF8.h"

namespace Carbon::Internal
{
    namespace
    {
        constexpr size_t MaxUndoSteps = 100;

        enum class CharacterClass
        {
            Space,
            Punctuation,
            Word
        };

        CharacterClass Classify(char32_t codepoint)
        {
            if (codepoint == U' ' || codepoint == U'\t' || codepoint == 0xA0)
                return CharacterClass::Space;
            if (codepoint < 0x80)
            {
                const bool isAlphanumeric = (codepoint >= U'0' && codepoint <= U'9') ||
                                            (codepoint >= U'A' && codepoint <= U'Z') ||
                                            (codepoint >= U'a' && codepoint <= U'z') || codepoint == U'_';
                return isAlphanumeric ? CharacterClass::Word : CharacterClass::Punctuation;
            }
            // Everything outside ASCII counts as part of a word.
            return CharacterClass::Word;
        }

        CharacterClass ClassifyAt(const std::string& text, size_t offset)
        {
            return Classify(DecodeUTF8(text, offset).Codepoint);
        }
    } // namespace

    void TextEditor::Reset(const std::string& text)
    {
        m_Caret = text.size();
        m_Anchor = text.size();
        m_UndoStack.clear();
        m_RedoStack.clear();
        m_LastEdit = EditKind::None;
    }

    void TextEditor::ClampTo(const std::string& text)
    {
        if (m_Caret > text.size() || m_Anchor > text.size())
        {
            m_Caret = std::min(m_Caret, text.size());
            m_Anchor = std::min(m_Anchor, text.size());
            // The text changed behind the editor's back: the history no longer applies.
            m_UndoStack.clear();
            m_RedoStack.clear();
            m_LastEdit = EditKind::None;
        }
    }

    std::string_view TextEditor::GetSelectedText(const std::string& text) const
    {
        return std::string_view(text).substr(GetSelectionStart(), GetSelectionEnd() - GetSelectionStart());
    }

    void TextEditor::SetCaret(const std::string& text, size_t offset, bool extendSelection)
    {
        m_Caret = std::min(offset, text.size());
        if (!extendSelection)
            m_Anchor = m_Caret;
        m_LastEdit = EditKind::None;
    }

    void TextEditor::SelectAll(const std::string& text)
    {
        m_Anchor = 0;
        m_Caret = text.size();
        m_LastEdit = EditKind::None;
    }

    void TextEditor::SelectWordAt(const std::string& text, size_t offset)
    {
        offset = std::min(offset, text.size());
        // A click at the very end selects the last word.
        if (offset == text.size() && offset > 0)
            offset = PreviousCodepointOffset(text, offset);
        m_Anchor = FindWordStart(text, offset);
        m_Caret = FindWordEnd(text, offset);
        m_LastEdit = EditKind::None;
    }

    void TextEditor::MoveLeft(const std::string& text, bool extendSelection, bool byWord)
    {
        if (HasSelection() && !extendSelection)
        {
            SetCaret(text, GetSelectionStart(), false);
            return;
        }
        size_t target = PreviousCodepointOffset(text, m_Caret);
        if (byWord)
        {
            // Skip spaces, then go to the start of the word before them.
            while (target > 0 && ClassifyAt(text, target) == CharacterClass::Space)
                target = PreviousCodepointOffset(text, target);
            target = FindWordStart(text, target);
        }
        SetCaret(text, target, extendSelection);
    }

    void TextEditor::MoveRight(const std::string& text, bool extendSelection, bool byWord)
    {
        if (HasSelection() && !extendSelection)
        {
            SetCaret(text, GetSelectionEnd(), false);
            return;
        }
        size_t target = NextCodepointOffset(text, m_Caret);
        if (byWord)
        {
            target = m_Caret;
            while (target < text.size() && ClassifyAt(text, target) == CharacterClass::Space)
                target = NextCodepointOffset(text, target);
            if (target < text.size())
                target = FindWordEnd(text, target);
        }
        SetCaret(text, target, extendSelection);
    }

    void TextEditor::MoveToStart(const std::string& text, bool extendSelection)
    {
        SetCaret(text, 0, extendSelection);
    }

    void TextEditor::MoveToEnd(const std::string& text, bool extendSelection)
    {
        SetCaret(text, text.size(), extendSelection);
    }

    bool TextEditor::Insert(std::string& text, std::string_view inserted, size_t maxLength)
    {
        // Sanitize: a single-line field has no line breaks or control characters.
        std::string clean;
        clean.reserve(inserted.size());
        size_t offset = 0;
        while (offset < inserted.size())
        {
            const UTF8Decoded decoded = DecodeUTF8(inserted, offset);
            offset += decoded.Length;
            if (decoded.Codepoint == U'\n' || decoded.Codepoint == U'\t')
                clean.push_back(' ');
            else if (decoded.Codepoint >= 0x20 && decoded.Codepoint != 0x7F)
                AppendUTF8(clean, decoded.Codepoint);
        }

        if (maxLength > 0)
        {
            const size_t selected = CountCodepoints(GetSelectedText(text));
            const size_t current = CountCodepoints(text) - selected;
            const size_t room = current < maxLength ? maxLength - current : 0;
            size_t end = 0;
            for (size_t i = 0; i < room && end < clean.size(); i++)
                end = NextCodepointOffset(clean, end);
            clean.resize(end);
        }

        if (clean.empty() && !HasSelection())
            return false;

        RecordUndo(text, HasSelection() ? EditKind::Other : EditKind::Typing);
        EraseSelection(text);
        text.insert(m_Caret, clean);
        m_Caret += clean.size();
        m_Anchor = m_Caret;
        return true;
    }

    bool TextEditor::DeleteBackward(std::string& text, bool byWord)
    {
        if (HasSelection())
            return DeleteSelection(text);
        if (m_Caret == 0)
            return false;

        RecordUndo(text, EditKind::Deleting);
        const size_t caret = m_Caret;
        MoveLeft(text, false, byWord);
        text.erase(m_Caret, caret - m_Caret);
        m_Anchor = m_Caret;
        m_LastEdit = EditKind::Deleting;
        return true;
    }

    bool TextEditor::DeleteForward(std::string& text, bool byWord)
    {
        if (HasSelection())
            return DeleteSelection(text);
        if (m_Caret >= text.size())
            return false;

        RecordUndo(text, EditKind::Deleting);
        const size_t caret = m_Caret;
        MoveRight(text, false, byWord);
        text.erase(caret, m_Caret - caret);
        m_Caret = caret;
        m_Anchor = caret;
        m_LastEdit = EditKind::Deleting;
        return true;
    }

    bool TextEditor::DeleteSelection(std::string& text)
    {
        if (!HasSelection())
            return false;
        RecordUndo(text, EditKind::Other);
        EraseSelection(text);
        return true;
    }

    bool TextEditor::Undo(std::string& text)
    {
        if (m_UndoStack.empty())
            return false;
        m_RedoStack.push_back(Snapshot{text, m_Caret, m_Anchor});
        Snapshot& snapshot = m_UndoStack.back();
        text = std::move(snapshot.Text);
        m_Caret = snapshot.Caret;
        m_Anchor = snapshot.Anchor;
        m_UndoStack.pop_back();
        m_LastEdit = EditKind::None;
        return true;
    }

    bool TextEditor::Redo(std::string& text)
    {
        if (m_RedoStack.empty())
            return false;
        m_UndoStack.push_back(Snapshot{text, m_Caret, m_Anchor});
        Snapshot& snapshot = m_RedoStack.back();
        text = std::move(snapshot.Text);
        m_Caret = snapshot.Caret;
        m_Anchor = snapshot.Anchor;
        m_RedoStack.pop_back();
        m_LastEdit = EditKind::None;
        return true;
    }

    void TextEditor::RecordUndo(const std::string& text, EditKind kind)
    {
        // A run of typing, or a run of deleting, is one undo step.
        const bool continues = kind == m_LastEdit && (kind == EditKind::Typing || kind == EditKind::Deleting);
        m_LastEdit = kind;
        m_RedoStack.clear();
        if (continues)
            return;
        if (m_UndoStack.size() >= MaxUndoSteps)
            m_UndoStack.erase(m_UndoStack.begin());
        m_UndoStack.push_back(Snapshot{text, m_Caret, m_Anchor});
    }

    void TextEditor::EraseSelection(std::string& text)
    {
        const size_t start = GetSelectionStart();
        text.erase(start, GetSelectionEnd() - start);
        m_Caret = start;
        m_Anchor = start;
    }

    size_t TextEditor::FindWordStart(const std::string& text, size_t offset)
    {
        if (offset >= text.size())
            return text.size();
        const CharacterClass kind = ClassifyAt(text, offset);
        while (offset > 0)
        {
            const size_t previous = PreviousCodepointOffset(text, offset);
            if (ClassifyAt(text, previous) != kind)
                break;
            offset = previous;
        }
        return offset;
    }

    size_t TextEditor::FindWordEnd(const std::string& text, size_t offset)
    {
        if (offset >= text.size())
            return text.size();
        const CharacterClass kind = ClassifyAt(text, offset);
        while (offset < text.size() && ClassifyAt(text, offset) == kind)
            offset = NextCodepointOffset(text, offset);
        return offset;
    }
} // namespace Carbon::Internal
