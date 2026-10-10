#include "Carbon/Widgets/Internal/TextInput.h"

#include <algorithm>

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/UTF8.h"
#include "Carbon/Input/Input.h"
#include "Carbon/Widgets/Internal/TextEditor.h"

namespace Carbon::Internal
{
    namespace
    {
        // Underlines of pre-edit text, in points: clauses get a thin line and the active clause a thick one, each
        // inset at both ends so that neighbouring clauses stay apart.
        constexpr float ClauseUnderline = 1.0f;
        constexpr float ActiveClauseUnderline = 2.0f;
        constexpr float ClauseInset = 1.0f;
    } // namespace

    bool ApplyTextInput(Context& context, TextEditor& editor, std::string& text, const TextInputOptions& options)
    {
        const InputState& input = context.Input;
        const KeyModifiers shortcut = context.HostIO.GetShortcutModifier();
        const bool isShiftHeld = HasModifiers(input.Modifiers, KeyModifiers::Shift);
        const bool byWord = HasModifiers(input.Modifiers, shortcut) || HasModifiers(input.Modifiers, KeyModifiers::Alt);
        bool changed = false;

        // While an input method composes, the keys are its business; a host forwards them only by mistake.
        if (!IsComposing(context))
        {
            if (IsKeyPressed(Key::LeftArrow))
                editor.MoveLeft(text, isShiftHeld, byWord);
            if (IsKeyPressed(Key::RightArrow))
                editor.MoveRight(text, isShiftHeld, byWord);
            if (IsKeyPressed(Key::Backspace))
                changed = editor.DeleteBackward(text, byWord) || changed;
            if (IsKeyPressed(Key::Delete))
                changed = editor.DeleteForward(text, byWord) || changed;

            if (IsShortcutPressed(Key::A))
                editor.SelectAll(text);
            const bool canCopy = editor.HasSelection() && options.CanCopy && context.HostCallbacks.SetClipboardText;
            if (IsShortcutPressed(Key::C) && canCopy)
                context.HostCallbacks.SetClipboardText(editor.GetSelectedText(text));
            if (IsShortcutPressed(Key::X) && canCopy)
            {
                context.HostCallbacks.SetClipboardText(editor.GetSelectedText(text));
                changed = editor.DeleteSelection(text) || changed;
            }
            if (IsShortcutPressed(Key::V) && context.HostCallbacks.GetClipboardText)
            {
                const std::string pasted = context.HostCallbacks.GetClipboardText();
                changed = editor.Insert(text, pasted, options.MaxLength, options.MaxBytes) || changed;
            }
            if (IsShortcutPressed(Key::Z))
                changed = editor.Undo(text) || changed;
            if (IsShortcutPressed(Key::Z, KeyModifiers::Shift) || IsShortcutPressed(Key::Y))
                changed = editor.Redo(text) || changed;
        }

        // Edits of an on-screen keyboard: autocorrection, a suggestion, a deletion. Offsets are those of the text
        // the host was told about; outside it or inside a character, they move to the nearest boundary.
        for (const TextReplacement& replacement : input.Replacements)
        {
            const auto toBoundary = [&text](size_t offset)
            {
                offset = std::min(offset, text.size());
                while (offset > 0 && offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xC0) == 0x80)
                    offset--;
                return offset;
            };
            const size_t end = toBoundary(replacement.End);
            const size_t start = std::min(toBoundary(replacement.Start), end);
            editor.SetCaret(text, start, false);
            editor.SetCaret(text, end, true);
            const std::string_view inserted =
                std::string_view(input.ReplacementText).substr(replacement.TextStart, replacement.TextLength);
            if (inserted.empty())
                changed = editor.DeleteSelection(text) || changed;
            else
                changed = editor.Insert(text, inserted, options.MaxLength, options.MaxBytes) || changed;
        }

        // Typed characters, and the text of compositions committed this frame: a Korean input method commits a
        // syllable and starts composing the next one in the same frame.
        for (const char32_t character : input.Characters)
        {
            char encoded[4];
            const uint32_t length = EncodeUTF8(character, encoded);
            changed =
                editor.Insert(text, std::string_view(encoded, length), options.MaxLength, options.MaxBytes) || changed;
        }

        // Pre-edit text takes the place of the selection, as typing would.
        if (IsComposing(context) && !input.Composition.Text.empty() && editor.HasSelection())
            changed = editor.DeleteSelection(text) || changed;
        return changed;
    }

    bool IsComposing(const Context& context)
    {
        return context.Input.Composition.IsActive;
    }

    bool CommitComposition(Context& context, TextEditor& editor, std::string& text, const TextInputOptions& options)
    {
        CompositionState& composition = context.Input.Composition;
        if (!composition.IsActive)
            return false;
        const bool changed = editor.Insert(text, composition.Text, options.MaxLength, options.MaxBytes);
        CancelComposition(context);
        return changed;
    }

    void CancelComposition(Context& context)
    {
        if (!context.Input.Composition.IsActive)
            return;
        context.Input.Composition.Clear();
        context.Interaction.IsCompositionCancelRequested = true;
    }

    void HandOverComposition(Context& context, const TextEditor& previousEditor)
    {
        TextEditState& edit = context.TextEdit;
        CompositionState& composition = context.Input.Composition;
        if (!composition.IsActive || !edit.Owner.IsValid())
            return;
        edit.PendingCommitOwner = edit.Owner;
        edit.PendingCommitFrame = context.FrameCount;
        edit.PendingCommitCaret = previousEditor.GetCaret();
        edit.PendingCommitText.assign(composition.Text);
        CancelComposition(context);
    }

    bool ApplyHandedOverComposition(Context& context, ID id, std::string& text, const TextInputOptions& options,
                                    bool isMultiLine)
    {
        TextEditState& edit = context.TextEdit;
        if (edit.PendingCommitOwner != id)
            return false;
        edit.PendingCommitOwner = ID();
        if (context.FrameCount > edit.PendingCommitFrame + 1 || edit.PendingCommitText.empty())
            return false;

        // The control is not being edited, so the shared editor belongs to another one. A rare event, worth an
        // editor of its own.
        TextEditor editor;
        editor.SetMultiLine(isMultiLine);
        editor.Reset(text);
        // The text may have changed since; the caret stays inside it and on a character boundary.
        size_t caret = std::min(edit.PendingCommitCaret, text.size());
        while (caret > 0 && caret < text.size() && (static_cast<unsigned char>(text[caret]) & 0xC0) == 0x80)
            caret--;
        editor.SetCaret(text, caret, false);
        return editor.Insert(text, edit.PendingCommitText, options.MaxLength, options.MaxBytes);
    }

    size_t GetCompositionAnchor(const Context& context)
    {
        const CompositionState& composition = context.Input.Composition;
        for (const CompositionClause& clause : composition.Clauses)
        {
            if (clause.IsActive)
                return clause.Start;
        }
        return composition.Caret;
    }

    void DrawCompositionUnderline(Context& context, DrawList& drawList, size_t start, size_t from, size_t to,
                                  FunctionRef<float(size_t)> getX, float bottom, Color color)
    {
        const CompositionState& composition = context.Input.Composition;
        const auto underline = [&](size_t clauseStart, size_t clauseEnd, bool isActive)
        {
            // The part of the clause on this line, in offsets of the displayed text.
            const size_t first = std::max(start + clauseStart, from);
            const size_t last = std::min(start + clauseEnd, to);
            if (first >= last)
                return;
            // Insets only where the clause itself begins or ends, not where a line breaks it.
            const float left = getX(first) + (first == start + clauseStart ? ClauseInset : 0.0f);
            const float right = getX(last) - (last == start + clauseEnd ? ClauseInset : 0.0f);
            if (right <= left)
                return;
            const float thickness = isActive ? ActiveClauseUnderline : ClauseUnderline;
            const Rect line(left, bottom - thickness, right - left, thickness);
            drawList.AddRect(context.Scale.Snap(line), color);
        };

        if (composition.Clauses.empty())
        {
            underline(0, composition.Text.size(), false);
            return;
        }
        for (const CompositionClause& clause : composition.Clauses)
            underline(clause.Start, clause.End, clause.IsActive);
    }
} // namespace Carbon::Internal
