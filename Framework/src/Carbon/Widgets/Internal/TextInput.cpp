#include "Carbon/Widgets/Internal/TextInput.h"

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/UTF8.h"
#include "Carbon/Input/Input.h"
#include "Carbon/Widgets/Internal/TextEditor.h"

namespace Carbon::Internal
{
    bool ApplyTextInput(Context& context, TextEditor& editor, std::string& text, const TextInputOptions& options)
    {
        const InputState& input = context.Input;
        const KeyModifiers shortcut = context.HostIO.GetShortcutModifier();
        const bool isShiftHeld = HasModifiers(input.Modifiers, KeyModifiers::Shift);
        const bool byWord = HasModifiers(input.Modifiers, shortcut) || HasModifiers(input.Modifiers, KeyModifiers::Alt);
        bool changed = false;

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

        for (const char32_t character : input.Characters)
        {
            char encoded[4];
            const uint32_t length = EncodeUTF8(character, encoded);
            changed =
                editor.Insert(text, std::string_view(encoded, length), options.MaxLength, options.MaxBytes) || changed;
        }
        return changed;
    }
} // namespace Carbon::Internal
