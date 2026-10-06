#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Carbon/Core/ID.h"

namespace Carbon::Internal
{
    /// The editing logic of a single-line text field: caret, selection, insertion, deletion and undo, on UTF-8
    /// text. It knows nothing about drawing or input devices, which keeps it easy to test.
    ///
    /// Offsets are byte offsets that always sit on character boundaries. The selection runs from the anchor to
    /// the caret; when they are equal there is no selection.
    class TextEditor
    {
    public:
        /// Starts editing a text: caret at the end, no selection, empty undo history.
        void Reset(const std::string& text);

        /// Brings caret and anchor back inside the text after it was changed from outside.
        void ClampTo(const std::string& text);

        size_t GetCaret() const { return m_Caret; }
        size_t GetAnchor() const { return m_Anchor; }
        bool HasSelection() const { return m_Caret != m_Anchor; }
        size_t GetSelectionStart() const { return m_Caret < m_Anchor ? m_Caret : m_Anchor; }
        size_t GetSelectionEnd() const { return m_Caret < m_Anchor ? m_Anchor : m_Caret; }
        std::string_view GetSelectedText(const std::string& text) const;

        /// Moves the caret; with `extendSelection` the anchor stays where it is.
        void SetCaret(const std::string& text, size_t offset, bool extendSelection);
        void SelectAll(const std::string& text);
        /// Selects the word (or run of spaces or punctuation) around `offset`.
        void SelectWordAt(const std::string& text, size_t offset);

        /// Arrow keys. Without `extendSelection`, a selection collapses to its near edge instead of moving.
        void MoveLeft(const std::string& text, bool extendSelection, bool byWord);
        void MoveRight(const std::string& text, bool extendSelection, bool byWord);
        void MoveToStart(const std::string& text, bool extendSelection);
        void MoveToEnd(const std::string& text, bool extendSelection);

        /// A multi-line editor keeps line breaks (a "\r\n" or a lone '\r' becomes '\n') and tabs; a single-line
        /// one turns them into spaces. Off by default.
        void SetMultiLine(bool isMultiLine) { m_IsMultiLine = isMultiLine; }

        /// Replaces the selection with `inserted`. Line breaks and tabs become spaces (or stay, in a multi-line
        /// editor) and other control characters are dropped. `maxLength` limits the text's length in characters and
        /// `maxBytes` its size in bytes (0 = unlimited); what does not fit is cut off at a character boundary. Returns
        /// true when the text changed.
        bool Insert(std::string& text, std::string_view inserted, size_t maxLength = 0, size_t maxBytes = 0);
        /// Backspace and Delete: remove the selection, or the character (or word) next to the caret.
        bool DeleteBackward(std::string& text, bool byWord);
        bool DeleteForward(std::string& text, bool byWord);
        bool DeleteSelection(std::string& text);

        /// Undo and redo. Consecutive typing counts as one step. Return true when the text changed.
        bool Undo(std::string& text);
        bool Redo(std::string& text);

    private:
        enum class EditKind
        {
            None,
            Typing,
            Deleting,
            Other
        };

        struct Snapshot
        {
            std::string Text;
            size_t Caret = 0;
            size_t Anchor = 0;
        };

        void RecordUndo(const std::string& text, EditKind kind);
        void EraseSelection(std::string& text);
        static size_t FindWordStart(const std::string& text, size_t offset);
        static size_t FindWordEnd(const std::string& text, size_t offset);

    private:
        size_t m_Caret = 0;
        size_t m_Anchor = 0;
        std::vector<Snapshot> m_UndoStack;
        std::vector<Snapshot> m_RedoStack;
        EditKind m_LastEdit = EditKind::None;
        bool m_IsMultiLine = false;
        /// The sanitized text of the current Insert; kept so that typing does not allocate.
        std::string m_Inserted;
    };

    /// A visual line of a text area: the bytes [Start, End) of its text, without the line break.
    struct TextAreaLine
    {
        size_t Start = 0;
        size_t End = 0;
        /// Horizontal extent of the line, including spaces where it wrapped, in points.
        float Width = 0.0f;
        /// The last line of a paragraph: End is a line break or the end of the text, and the caret may sit there.
        /// Otherwise the line wrapped and End is the Start of the next line, where the caret is shown instead.
        bool EndsParagraph = false;
    };

    /// The editing session of a context. Only one text field is edited at a time: the one with keyboard focus.
    struct TextEditState
    {
        /// The text field being edited; invalid when none is.
        ID Owner;
        TextEditor Editor;
        /// How far the text is scrolled to keep the caret inside the field, in points.
        float ScrollX = 0.0f;
        /// Seconds since the caret last moved; drives the blink.
        float BlinkTime = 0.0f;
        /// The current mouse press started as a single click, so dragging extends the selection.
        bool IsDragSelecting = false;
        /// The owner's text was replaced from outside: reset the editor on its next call. See ReloadTextField.
        bool IsReloadPending = false;
        /// A selection for the field PendingSelectionOwner, set during PendingSelectionFrame and applied on the
        /// field's next call once it is being edited. See SetTextFieldSelection.
        ID PendingSelectionOwner;
        uint64_t PendingSelectionFrame = 0;
        size_t PendingAnchor = 0;
        size_t PendingCaret = 0;
        /// The text of a field bound to a fixed buffer or to a callback, while that field runs. Its capacity is
        /// kept, so those fields do not allocate either.
        std::string BoundText;
        /// Scratch storage reused between frames.
        std::vector<float> CaretPositions;
        std::string SecureText;
        /// The layout of the text area being drawn: its visual lines, and the caret's horizontal position before
        /// each byte, relative to the start of the byte's line. Reused by every text area in turn.
        std::vector<TextAreaLine> AreaLines;
        std::vector<float> AreaCaretX;
        std::string AreaParagraph;
    };
} // namespace Carbon::Internal
