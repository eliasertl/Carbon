#include "Carbon/Extensions/SearchField.h"

namespace Carbon
{
    bool SearchField(std::string_view label, std::string* text, const SearchFieldOptions& options)
    {
        CB_VERIFY(text != nullptr, "SearchField needs a string to edit");
        if (text == nullptr)
            return false;

        // Escape cancels the search. The text field itself only gives up focus on Escape, so the field's state
        // before the call decides.
        const bool cancels = IsFocused(GetID(label)) && IsKeyPressed(Key::Escape, false) && !text->empty();

        TextFieldOptions field;
        field.Keyboard = KeyboardType::Search;
        field.Placeholder = options.Placeholder;
        field.Icon = Icons::MagnifyingGlass;
        field.ShowsClearButton = true;
        field.Width = options.Width;
        field.ControlSize = options.ControlSize;
        field.Disabled = options.Disabled;
        bool changed = TextField(label, text, field);

        if (cancels)
        {
            text->clear();
            changed = true;
        }
        return changed;
    }
} // namespace Carbon
