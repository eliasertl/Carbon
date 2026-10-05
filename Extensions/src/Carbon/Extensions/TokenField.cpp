#include "Carbon/Extensions/TokenField.h"

#include <algorithm>
#include <cstring>

#include "Carbon/Extensions/Menu.h"

namespace Carbon
{
    namespace
    {
        // The text being typed is kept per field in a fixed buffer, so that the state stays trivially copyable.
        constexpr size_t MaxPending = 512;
        // In characters; at most four bytes each, so the text always fits the buffer.
        constexpr size_t MaxPendingCharacters = 120;

        constexpr float FieldPadding = 3.0f;
        constexpr float TokenPadding = 6.0f;
        constexpr float TokenGap = 4.0f;
        constexpr float LineGap = 3.0f;
        constexpr float TokenRadius = 5.0f;
        // The text field after the last token is at least this wide; otherwise it starts a new line.
        constexpr float MinTextWidth = 60.0f;
        // TextField draws its text this far inside its rectangle.
        constexpr float TextFieldInset = 7.0f;
        constexpr AnimationSpec HeightSpring = AnimationSpec::Spring(0.25f);

        struct TokenFieldState
        {
            char Pending[MaxPending];
            /// Selected tokens: from Anchor to Focus, both included, while HasSelection.
            int Anchor;
            int Focus;
            bool HasSelection;
            bool WasFocused;
            /// The token whose context menu is open, and where it opened.
            int MenuToken;
            Vec2 MenuPosition;
        };

        // The text field edits this string while it is called; the field's own text is copied in and out around
        // the call, so the string's capacity is reserved once and reused by every token field.
        std::string& GetScratch()
        {
            static std::string s_Scratch;
            if (s_Scratch.capacity() < MaxPending)
                s_Scratch.reserve(MaxPending);
            return s_Scratch;
        }

        std::string_view Trim(std::string_view text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
                text.remove_prefix(1);
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
                text.remove_suffix(1);
            return text;
        }

        // Where tokens and the text go, relative to the field's top-left corner and before scrolling.
        struct FieldGeometry
        {
            float Width = 0.0f;
            float TokenHeight = 0.0f;
            bool Wraps = true;
            /// The text needs room after the tokens: while editing, or to show the placeholder.
            bool NeedsTextRoom = true;

            float GetLineTop(int line) const
            {
                return FieldPadding + static_cast<float>(line) * (TokenHeight + LineGap);
            }
        };

        struct LayoutResult
        {
            int Lines = 1;
            float TextX = 0.0f;
            int TextLine = 0;
            float ContentRight = 0.0f;
        };

        // Calls `visit(index, rect)` for every token, in field coordinates, and returns where the text goes.
        template <typename Visit>
        LayoutResult LayOut(const std::vector<std::string>& tokens, const TextSpec& spec, const FieldGeometry& geometry,
                            Visit&& visit)
        {
            const float left = FieldPadding;
            const float right = geometry.Width - FieldPadding;
            float x = left;
            int line = 0;
            for (size_t i = 0; i < tokens.size(); i++)
            {
                const float width =
                    std::min(MeasureText(tokens[i], spec).X + TokenPadding * 2.0f, std::max(right - left, 1.0f));
                if (geometry.Wraps && x > left && x + width > right)
                {
                    line++;
                    x = left;
                }
                visit(static_cast<int>(i), Rect(x, geometry.GetLineTop(line), width, geometry.TokenHeight));
                x += width + TokenGap;
            }
            if (geometry.Wraps && geometry.NeedsTextRoom && x > left && right - x < MinTextWidth)
            {
                line++;
                x = left;
            }
            LayoutResult result;
            result.Lines = line + 1;
            result.TextX = x;
            result.TextLine = line;
            result.ContentRight = x + MinTextWidth;
            return result;
        }
    } // namespace

    bool TokenField(std::string_view label, std::vector<std::string>* tokens, const TokenFieldOptions& options)
    {
        CB_VERIFY(tokens != nullptr, "TokenField needs a list of tokens to edit");
        if (tokens == nullptr)
            return false;

        const ID id = GetID(label);
        TokenFieldState& state = *GetState<TokenFieldState>(id);
        const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
        const TextSpec spec = GetTextSpec(metrics.Style);
        const int count = static_cast<int>(tokens->size());
        const bool isFocused = IsFocused(id);
        if (state.HasSelection && count == 0)
            state.HasSelection = false;
        state.Anchor = std::clamp(state.Anchor, 0, std::max(count - 1, 0));
        state.Focus = std::clamp(state.Focus, 0, std::max(count - 1, 0));

        // Lay the tokens out once to know the field's height; wrapping fields grow with their lines.
        ItemOptions sizing;
        sizing.Width = options.Width;
        FieldGeometry geometry;
        geometry.Width = ResolveItemSize(Vec2(260.0f, metrics.Height), sizing).X;
        geometry.TokenHeight = metrics.Height - FieldPadding * 2.0f;
        geometry.Wraps = options.Layout == TokenFieldLayout::Wrapping;
        geometry.NeedsTextRoom = isFocused || count == 0 || state.Pending[0] != '\0';
        float selectedLeft = 0.0f;
        float selectedRight = 0.0f;
        const LayoutResult layout = LayOut(*tokens, spec, geometry,
                                           [&](int index, const Rect& token)
                                           {
                                               if (state.HasSelection && index == state.Focus)
                                               {
                                                   selectedLeft = token.X;
                                                   selectedRight = token.GetRight();
                                               }
                                           });
        const int visibleLines = options.MaxLines > 0 ? std::min(layout.Lines, options.MaxLines) : layout.Lines;
        const int shownLines = geometry.Wraps ? visibleLines : 1;
        const float pitch = geometry.TokenHeight + LineGap;
        const float targetHeight = FieldPadding * 2.0f + geometry.TokenHeight * static_cast<float>(shownLines) +
                                   LineGap * static_cast<float>(shownLines - 1);
        const float height = Animate(HashID("##height", id), targetHeight, HeightSpring);

        // Scrolling: a wrapping field beyond its line limit shows its last lines, where the typing happens. A
        // single line shows the selected token, or the end while typing.
        Vec2 scroll;
        if (geometry.Wraps)
        {
            scroll.Y = static_cast<float>(layout.Lines - visibleLines) * pitch;
        }
        else
        {
            const float visible = geometry.Width - FieldPadding * 2.0f;
            const float overflow = std::max(layout.ContentRight - FieldPadding - visible, 0.0f);
            if (state.HasSelection)
                scroll.X = std::clamp(selectedRight - FieldPadding - visible, 0.0f,
                                      std::max(selectedLeft - FieldPadding, 0.0f));
            else if (isFocused)
                scroll.X = overflow;
            scroll.X = std::min(Animate(HashID("##scroll", id), scroll.X, HeightSpring), overflow);
        }

        PushDisabled(options.Disabled);
        VStackOptions stack;
        stack.Spacing = 0.0f;
        stack.Width = options.Width;
        stack.Height = Size::Fixed(height);
        stack.ID = label;
        BeginVStack(stack);
        ItemOptions item;
        item.Width = options.Width.Mode == SizeMode::Fit ? Size::Fit() : Size::Fill();
        const Rect rect = AllocateItem(Vec2(geometry.Width, height), item);
        const Vec2 origin = rect.GetMin() - scroll;
        const Rect inner = rect.Inset(EdgeInsets(1.0f));

        // The field, drawn first: the tokens and the text field go on top.
        DrawList& drawList = GetDrawList();
        const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
        const bool isHovered = IsRectHovered(rect);
        const ControlFeedback fieldFeedback = AnimateFeedback(id, isHovered, false);
        drawList.AddSquircle(rect, GetStyleColor(StyleColor::ControlBackground), metrics.CornerRadius, smoothing);
        drawList.AddSquircleStroke(rect,
                                   Blend(GetStyleColor(StyleColor::ControlBorder), GetStyleColor(StyleColor::Label),
                                         0.3f * fieldFeedback.Hover),
                                   metrics.CornerRadius, GetStyleVar(StyleVar::BorderWidth), smoothing);

        // ---- Mouse: tokens are selected by clicking, extended with Shift, and have a context menu ----
        int clicked = -1;
        int hovered = -1;
        int pressed = -1;
        int rightClicked = -1;
        LayOut(*tokens, spec, geometry,
               [&](int index, const Rect& token)
               {
                   const Rect visible = Rect(token.GetMin() + origin, token.GetSize()).GetIntersection(inner);
                   if (visible.Width <= 0.0f || visible.Height <= 0.0f)
                       return;
                   ButtonBehaviorOptions behavior;
                   behavior.Focusable = false;
                   const Interaction interaction = ButtonBehavior(HashID(index, id), visible, behavior);
                   if (interaction.Hovered)
                       hovered = index;
                   if (interaction.Pressed)
                       pressed = index;
                   if (interaction.Clicked)
                       clicked = index;
                   if (IsRectHovered(visible) && IsMousePressed(MouseButton::Right))
                       rightClicked = index;
               });
        const bool isShiftHeld = IsKeyDown(Key::LeftShift) || IsKeyDown(Key::RightShift);
        if (clicked >= 0 || rightClicked >= 0)
        {
            const int index = clicked >= 0 ? clicked : rightClicked;
            if (!(isShiftHeld && state.HasSelection && clicked >= 0))
                state.Anchor = index;
            state.Focus = index;
            state.HasSelection = true;
            SetFocus(id);
        }
        else if (IsMousePressed() && isHovered && hovered < 0)
        {
            // A click beside the tokens goes to the text.
            state.HasSelection = false;
            SetFocus(id);
        }
        if (rightClicked >= 0)
        {
            state.MenuToken = rightClicked;
            state.MenuPosition = GetMousePos();
            OpenOverlay(HashID("##tokenmenu", id));
        }

        // ---- Keyboard ----
        std::string& text = GetScratch();
        text.assign(state.Pending);
        bool changed = false;
        bool acceptsInput = !state.HasSelection;
        const auto eraseSelection = [&]
        {
            const int first = std::min(state.Anchor, state.Focus);
            const int last = std::max(state.Anchor, state.Focus);
            tokens->erase(tokens->begin() + first, tokens->begin() + last + 1);
            state.HasSelection = false;
            changed = true;
        };
        if (isFocused && !IsDisabled())
        {
            TextFieldSelection selection;
            const bool isCaretAtStart =
                GetTextFieldSelection(label, &selection) && selection.Caret == 0 && selection.Start == selection.End;
            if (state.HasSelection)
            {
                if (IsKeyPressed(Key::Backspace) || IsKeyPressed(Key::Delete))
                {
                    eraseSelection();
                }
                else if (IsKeyPressed(Key::LeftArrow))
                {
                    if (isShiftHeld)
                        state.Focus = std::max(state.Focus - 1, 0);
                    else
                        state.Anchor = state.Focus = std::max(std::min(state.Anchor, state.Focus) - 1, 0);
                }
                else if (IsKeyPressed(Key::RightArrow))
                {
                    const int next = std::max(state.Anchor, state.Focus) + 1;
                    if (isShiftHeld)
                        state.Focus = std::min(state.Focus + 1, count - 1);
                    else if (next >= count)
                        state.HasSelection = false; // past the last token: back to the text
                    else
                        state.Anchor = state.Focus = next;
                }
                else if (IsKeyPressed(Key::Escape, false))
                {
                    state.HasSelection = false;
                }
                else if (IsShortcutPressed(Key::A))
                {
                    state.Anchor = 0;
                    state.Focus = count - 1;
                }
                else if (!GetInputCharacters().empty())
                {
                    // Typing replaces the selected tokens, as in Mail.
                    eraseSelection();
                    acceptsInput = true;
                }
            }
            else if (count > 0 && isCaretAtStart && (IsKeyPressed(Key::Backspace) || IsKeyPressed(Key::LeftArrow)))
            {
                // Backspace or Left at the start of the text selects the token before it; a second Backspace
                // deletes it.
                state.Anchor = state.Focus = count - 1;
                state.HasSelection = true;
                acceptsInput = false;
            }
            else if (count > 0 && text.empty() && IsShortcutPressed(Key::A))
            {
                state.Anchor = 0;
                state.Focus = count - 1;
                state.HasSelection = true;
                acceptsInput = false;
            }
        }
        if (!isFocused && state.WasFocused)
            state.HasSelection = false; // leaving the field deselects; a click that focuses it selects at once

        // ---- Tokens ----
        const Color labelColor = GetStyleColor(StyleColor::Label);
        const Color accent = GetStyleColor(StyleColor::Accent);
        const int selectionFirst = std::min(state.Anchor, state.Focus);
        const int selectionLast = std::max(state.Anchor, state.Focus);
        drawList.PushClipRect(inner);
        LayOut(*tokens, spec, geometry,
               [&](int index, const Rect& local)
               {
                   const Rect token(local.GetMin() + origin, local.GetSize());
                   if (!token.Intersects(inner))
                       return;
                   const bool isSelected = state.HasSelection && index >= selectionFirst && index <= selectionLast;
                   const ControlFeedback feedback =
                       AnimateFeedback(HashID(index, id), index == hovered, index == pressed);
                   const Color fill = isSelected ? accent : accent.WithOpacity(0.18f);
                   drawList.AddSquircle(token, ApplyFeedback(fill, labelColor, feedback), TokenRadius, smoothing);
                   TextSpec tokenSpec = spec;
                   tokenSpec.MaxWidth = std::max(token.Width - TokenPadding * 2.0f + 1.0f, 1.0f);
                   tokenSpec.Wraps = false;
                   const std::string& tokenText = (*tokens)[static_cast<size_t>(index)];
                   DrawLabel(drawList, token, token.X + TokenPadding, tokenText, tokenSpec,
                             isSelected ? GetStyleColor(StyleColor::OnAccent) : labelColor);
               });

        // ---- The text being typed, in a borderless text field after the last token ----
        const float lineCenter = origin.Y + geometry.GetLineTop(layout.TextLine) + geometry.TokenHeight * 0.5f;
        const float textLeft = origin.X + layout.TextX + (layout.TextX > FieldPadding ? 0.0f : TokenGap);
        const Rect textRect(textLeft - TextFieldInset, lineCenter - metrics.Height * 0.5f,
                            std::max(rect.GetRight() - FieldPadding - textLeft + TextFieldInset, MinTextWidth),
                            metrics.Height);
        SetCursorPos(textRect.GetMin());
        TextFieldOptions field;
        // The placeholder shows only while the field is empty; a space draws nothing.
        field.Placeholder = count > 0 ? std::string_view(" ") : options.Placeholder;
        field.Width = textRect.Width;
        field.ControlSize = options.ControlSize;
        field.Disabled = options.Disabled;
        field.MaxLength = MaxPendingCharacters;
        field.IsBezeled = false;
        field.AcceptsInput = acceptsInput;
        TextField(label, &text, field);
        bool isSubmitted = IsItemSubmitted();
        drawList.PopClipRect();

        // ---- Turning text into tokens: at a delimiter, at Return, and when the field loses focus ----
        const auto addToken = [&](std::string_view part)
        {
            part = Trim(part);
            if (part.empty())
                return;
            tokens->emplace_back(part);
            changed = true;
        };
        if (!options.Delimiters.empty() && text.find_first_of(options.Delimiters) != std::string::npos)
        {
            size_t start = 0;
            for (size_t delimiter = text.find_first_of(options.Delimiters); delimiter != std::string::npos;
                 delimiter = text.find_first_of(options.Delimiters, start))
            {
                addToken(std::string_view(text).substr(start, delimiter - start));
                start = delimiter + 1;
            }
            // What follows the last delimiter is still being typed.
            const std::string_view rest = Trim(std::string_view(text).substr(start));
            std::memmove(text.data(), rest.data(), rest.size());
            text.resize(rest.size());
            ReloadTextField(label);
        }
        const bool hasText = !Trim(text).empty();
        if ((isSubmitted && options.TokenizesOnReturn && hasText) || (!isFocused && state.WasFocused && hasText))
        {
            addToken(text);
            text.clear();
            ReloadTextField(label);
            isSubmitted = false;
        }
        const size_t length = std::min(text.size(), MaxPending - 1);
        std::memcpy(state.Pending, text.data(), length);
        state.Pending[length] = '\0';
        state.WasFocused = isFocused;

        DrawFocusRing(id, rect, metrics.CornerRadius, true);
        EndVStack();

        Interaction summary;
        summary.Hovered = isHovered;
        summary.Focused = isFocused;
        summary.FocusVisible = IsFocusVisible(id);
        summary.Clicked = changed;
        SetLastItem(id, rect, summary);
        if (isSubmitted)
            SetItemSubmitted();
        PopDisabled();
        return changed;
    }

    bool BeginTokenFieldMenu(std::string_view label, int* token)
    {
        CB_VERIFY(token != nullptr, "BeginTokenFieldMenu needs an index to fill");
        const ID id = GetID(label);
        const TokenFieldState& state = *GetState<TokenFieldState>(id);
        MenuOptions options;
        options.Anchor = Rect(state.MenuPosition, Vec2());
        options.Gap = 0.0f;
        if (token == nullptr || !BeginMenu(HashID("##tokenmenu", id), options))
            return false;
        *token = state.MenuToken;
        return true;
    }

    void EndTokenFieldMenu()
    {
        EndMenu();
    }
} // namespace Carbon
