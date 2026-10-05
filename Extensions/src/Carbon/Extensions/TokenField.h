#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// How a token field arranges its tokens.
    enum class TokenFieldLayout : uint8_t
    {
        /// Tokens wrap onto further lines, and the field grows taller.
        Wrapping,
        /// One line that scrolls sideways to show where the user is typing.
        SingleLine
    };

    /// Per-call options of TokenField. All fields are optional.
    struct TokenFieldOptions
    {
        /// Shown while the field has neither tokens nor text. Defaults to the field's label.
        std::string_view Placeholder = {};
        TokenFieldLayout Layout = TokenFieldLayout::Wrapping;
        /// Token fields do not fit their content, so the default is a fixed 260 points.
        Size Width = Size::Fixed(260.0f);
        /// With Wrapping: the most lines shown before the field scrolls; 0 for no limit.
        int MaxLines = 0;
        /// Characters that turn the text typed so far into a token. A comma by default, as on macOS.
        std::string_view Delimiters = ",";
        /// Return also turns the text into a token. With nothing typed, Return submits (IsItemSubmitted).
        bool TokenizesOnReturn = true;
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        bool Disabled = false;
    };

    /// A text field that turns what the user types into tokens, such as the recipients of a mail. The application
    /// owns the tokens; text that is still being typed becomes a token at a delimiter, at Return or when the field
    /// loses focus. Tokens can be selected with the mouse and the keyboard and deleted with Backspace or Delete.
    /// Returns true on frames the tokens changed.
    ///
    /// The label identifies the field and serves as its placeholder.
    bool TokenField(std::string_view label, std::vector<std::string>* tokens, const TokenFieldOptions& options = {});

    /// The context menu of a token, opened by a right click on it. Call right after TokenField with the same label;
    /// while it returns true, `token` is the index of the token it belongs to: add its items, then call
    /// EndTokenFieldMenu.
    bool BeginTokenFieldMenu(std::string_view label, int* token);
    void EndTokenFieldMenu();
} // namespace Carbon
