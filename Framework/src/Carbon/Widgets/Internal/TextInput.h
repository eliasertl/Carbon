#pragma once

#include <cstddef>
#include <string>

#include "Carbon/Core/Color.h"
#include "Carbon/Core/FunctionRef.h"
#include "Carbon/Core/ID.h"

namespace Carbon
{
    struct Context;
    class DrawList;
} // namespace Carbon

namespace Carbon::Internal
{
    class TextEditor;

    /// Limits and permissions of the text being edited.
    struct TextInputOptions
    {
        /// Longest text in characters, and largest in bytes; 0 means unlimited.
        size_t MaxLength = 0;
        size_t MaxBytes = 0;
        /// Copy and cut reach the clipboard (false for secure fields).
        bool CanCopy = true;
    };

    /// Applies the editing input that every text control shares, in this order: Left and Right (with Shift to
    /// select, with the shortcut modifier or Alt by word), Backspace and Delete (by word with the same modifiers),
    /// select all, copy, cut, paste, undo, redo, and the typed characters. Keys whose meaning differs between
    /// controls (Up, Down, Home, End, Enter, Tab, Escape) are left to the caller. Returns true when the text
    /// changed.
    ///
    /// While an input method composes (IsComposing), the keys belong to it: only the typed characters (text it
    /// committed) are applied, and a selection is deleted once there is pre-edit text to take its place.
    bool ApplyTextInput(Context& context, TextEditor& editor, std::string& text, const TextInputOptions& options);

    /// True while an input method composes text for the control being edited. The control ignores editing keys
    /// and shows the pre-edit text at its caret.
    bool IsComposing(const Context& context);

    /// Inserts the pre-edit text at the caret as it stands and ends the composition, for one the user interrupted
    /// by clicking or by moving the focus. The host is asked to cancel its input method's composition (see
    /// IO::WantsCompositionCancel). Returns true when the text changed.
    bool CommitComposition(Context& context, TextEditor& editor, std::string& text, const TextInputOptions& options);

    /// Ends the composition without inserting it, and asks the host to cancel its input method's.
    void CancelComposition(Context& context);

    /// Called by a text control that starts editing while another one was edited: a composition in progress
    /// belongs to that other control, and is committed into it at its caret on its next call (see
    /// ApplyHandedOverComposition). The host is asked to cancel its input method's composition.
    void HandOverComposition(Context& context, const TextEditor& previousEditor);

    /// Inserts pre-edit text handed over to the control `id` when it lost the focus to another control, if there
    /// is any. Returns true when the text changed.
    bool ApplyHandedOverComposition(Context& context, ID id, std::string& text, const TextInputOptions& options,
                                    bool isMultiLine);

    /// Where the input method's candidate window belongs, as a byte offset into the pre-edit text: the start of
    /// the active clause, or the caret.
    size_t GetCompositionAnchor(const Context& context);

    /// Underlines the part [from, to) of the pre-edit text shown at offset `start` of the displayed text: a thin
    /// line under each clause, a thicker one under the active clause, with a gap between clauses. `getX` gives
    /// the horizontal position of an offset of the displayed text; `bottom` is the bottom of the line.
    void DrawCompositionUnderline(Context& context, DrawList& drawList, size_t start, size_t from, size_t to,
                                  FunctionRef<float(size_t)> getX, float bottom, Color color);
} // namespace Carbon::Internal
