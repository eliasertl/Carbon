#include "Carbon/Widgets/TextField.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Animation/Animation.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/UTF8.h"
#include "Carbon/Input/Input.h"
#include "Carbon/Interaction/Interaction.h"
#include "Carbon/Layout/Layout.h"
#include "Carbon/Style/Style.h"
#include "Carbon/Text/Icons.h"
#include "Carbon/Widgets/ControlFeedback.h"
#include "Carbon/Widgets/Internal/TextInput.h"

namespace Carbon
{
    namespace
    {
        using Internal::TextEditor;
        using Internal::TextEditState;

        // U+2022 BULLET, shown instead of each character of a secure field.
        constexpr std::string_view Bullet = "\xE2\x80\xA2";
        constexpr float HorizontalPadding = 7.0f;
        constexpr float IconGap = 5.0f;
        constexpr float CaretWidth = 1.5f;
        // The caret is visible for the first half of every blink period.
        constexpr float BlinkPeriod = 1.0f;

        // In a secure field the text on screen (bullets) differs from the text being edited, so offsets are
        // translated by counting characters.
        size_t ToDisplayOffset(std::string_view text, size_t offset, bool isSecure)
        {
            return isSecure ? CountCodepoints(text.substr(0, offset)) * Bullet.size() : offset;
        }

        size_t ToTextOffset(std::string_view text, size_t displayOffset, bool isSecure)
        {
            if (!isSecure)
                return displayOffset;
            size_t offset = 0;
            for (size_t i = 0; i < displayOffset / Bullet.size() && offset < text.size(); i++)
                offset = NextCodepointOffset(text, offset);
            return offset;
        }

        // Selects from `anchor` to `caret`, both moved back onto character boundaries inside the text.
        void SelectRange(TextEditor& editor, const std::string& text, size_t anchor, size_t caret)
        {
            const auto toBoundary = [&](size_t offset)
            {
                offset = std::min(offset, text.size());
                while (offset > 0 && offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xC0) == 0x80)
                    offset--;
                return offset;
            };
            editor.SetCaret(text, toBoundary(anchor), false);
            editor.SetCaret(text, toBoundary(caret), true);
        }

        // The character boundary whose caret position is closest to `x`.
        size_t HitTest(std::string_view display, const std::vector<float>& positions, float x)
        {
            size_t best = 0;
            float bestDistance = std::abs(positions[0] - x);
            size_t offset = 0;
            while (offset < display.size())
            {
                offset = NextCodepointOffset(display, offset);
                const float distance = std::abs(positions[offset] - x);
                if (distance < bestDistance)
                {
                    best = offset;
                    bestDistance = distance;
                }
            }
            return best;
        }
    } // namespace

    namespace
    {
        // The text field, for all three ways of passing the text. `maxBytes` limits the size of the text in bytes
        // (0 = unlimited), for a caller's fixed buffer.
        bool EditTextField(Context& context, std::string_view label, std::string& text, size_t maxBytes,
                           const TextFieldOptions& options)
        {
            Internal::InteractionState& interactionState = context.Interaction;
            const Internal::InputState& input = context.Input;
            const Internal::CompositionState& composition = input.Composition;
            TextEditState& edit = context.TextEdit;
            TextEditor& editor = edit.Editor;

            const ID id = GetID(label);
            const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
            const TextSpec spec = GetTextSpec(metrics.Style);
            const FontMetrics fontMetrics = GetFontMetrics(spec);
            bool changed = false;
            bool submitted = false;

            // A secure field never puts its text on the clipboard.
            Internal::TextInputOptions inputOptions;
            inputOptions.MaxLength = options.MaxLength;
            inputOptions.MaxBytes = maxBytes;
            inputOptions.CanCopy = !options.IsSecure;

            PushDisabled(options.Disabled);
            const bool isDisabled = interactionState.DisabledDepth > 0;

            ItemOptions item;
            item.Width = options.Width;
            const Rect rect = AllocateItem(Vec2(120.0f, metrics.Height), item);
            const float centerY = rect.GetCenter().Y;

            // Geometry: [padding][icon][text ........][clear button][padding]
            const float iconSize = fontMetrics.LineHeight;
            float textLeft = rect.X + HorizontalPadding;
            if (!options.Icon.empty())
                textLeft += iconSize + IconGap;
            const float trailing = rect.GetRight() - std::max(options.TrailingInset, 0.0f);
            float textRight = trailing - HorizontalPadding;
            const bool hasClearButton = options.ShowsClearButton && !text.empty() && !isDisabled;
            const Rect clearRect(trailing - HorizontalPadding - iconSize, centerY - iconSize * 0.5f, iconSize,
                                 iconSize);
            if (hasClearButton)
                textRight = clearRect.X - IconGap;
            const float textWidth = std::max(textRight - textLeft, 1.0f);

            bool isHovered = false;
            bool isPressedHere = false;
            if (!isDisabled)
            {
                RegisterFocusable(id, rect);
                if (options.AcceptsInput)
                    Internal::AddTextInputArea(context, rect);
                isHovered = Internal::UpdateHover(context, id, rect);
                if (isHovered)
                    SetCursor(Cursor::IBeam);

                // The clear button sits on top of the field and takes its own clicks.
                if (hasClearButton)
                {
                    ButtonBehaviorOptions clearBehavior;
                    clearBehavior.Focusable = false;
                    if (ButtonBehavior(HashID("##clear", id), clearRect, clearBehavior).Clicked)
                    {
                        text.clear();
                        changed = true;
                        if (edit.Owner == id)
                        {
                            Internal::CancelComposition(context);
                            editor.Reset(text);
                        }
                        SetFocus(id, false);
                    }
                }

                isPressedHere = isHovered && input.MousePressed[static_cast<size_t>(MouseButton::Left)] &&
                                !interactionState.ActiveID.IsValid();
                if (isPressedHere)
                {
                    interactionState.ActiveID = id;
                    SetFocus(id, false);
                }
            }
            else if (edit.Owner == id)
            {
                Internal::CancelComposition(context);
                edit.Owner = ID();
            }

            const bool isFocused = !isDisabled && IsFocused(id);
            if (isFocused && edit.Owner != id)
            {
                // Editing starts. A composition in the field edited until now is committed there. Arriving by
                // keyboard selects everything, as on macOS; a click places the caret.
                Internal::HandOverComposition(context, editor);
                edit.Owner = id;
                edit.ScrollX = 0.0f;
                edit.BlinkTime = 0.0f;
                editor.Reset(text);
                if (interactionState.DidFocusMove)
                    editor.SelectAll(text);
            }
            else if (!isFocused && edit.Owner == id)
            {
                // Editing ends; what the user was composing stays, as it stands.
                editor.SetMultiLine(false);
                changed = Internal::CommitComposition(context, editor, text, inputOptions) || changed;
                edit.Owner = ID();
            }
            if (edit.Owner != id)
                changed = Internal::ApplyHandedOverComposition(context, id, text, inputOptions, false) || changed;
            if (edit.Owner == id && edit.IsReloadPending)
            {
                editor.Reset(text);
                edit.IsReloadPending = false;
            }
            // A selection asked for with SetTextFieldSelection before editing started, in this frame or the last.
            if (edit.Owner == id && edit.PendingSelectionOwner == id)
            {
                if (context.FrameCount <= edit.PendingSelectionFrame + 1)
                    SelectRange(editor, text, edit.PendingAnchor, edit.PendingCaret);
                edit.PendingSelectionOwner = ID();
            }

            // A field that does not accept input for now keeps its focus and its caret, but leaves the keys alone.
            // An input method's pre-edit text is shown at the caret; a secure field shows only committed text.
            const bool acceptsInput = options.AcceptsInput;
            const auto showsComposition = [&]
            { return isFocused && acceptsInput && !options.IsSecure && Internal::IsComposing(context); };

            // What is on screen: the text itself, with the pre-edit text at the caret, or one bullet per character.
            const auto getDisplay = [&]() -> std::string_view
            {
                if (options.IsSecure)
                {
                    edit.SecureText.clear();
                    const size_t count = CountCodepoints(text);
                    for (size_t i = 0; i < count; i++)
                        edit.SecureText.append(Bullet);
                    return edit.SecureText;
                }
                if (showsComposition())
                {
                    const size_t caret = editor.GetCaret();
                    edit.ComposedText.assign(text, 0, caret);
                    edit.ComposedText.append(composition.Text);
                    edit.ComposedText.append(text, caret, std::string::npos);
                    return edit.ComposedText;
                }
                return text;
            };
            // The caret's offset in what is on screen: inside the pre-edit text while composing.
            const auto getDisplayCaret = [&]
            {
                const size_t caret = ToDisplayOffset(text, editor.GetCaret(), options.IsSecure);
                return showsComposition() ? caret + composition.Caret : caret;
            };
            std::string_view display = getDisplay();

            if (isFocused)
            {
                interactionState.IsTextInputActive = acceptsInput;
                editor.SetMultiLine(false);
                editor.ClampTo(text);
                const size_t caretBefore = editor.GetCaret();
                const size_t anchorBefore = editor.GetAnchor();

                // A click ends a composition: the pre-edit text stays as it is, and the click acts on the result.
                if (isPressedHere && acceptsInput && Internal::IsComposing(context))
                {
                    changed = Internal::CommitComposition(context, editor, text, inputOptions) || changed;
                    display = getDisplay();
                }
                GetCaretPositions(display, spec, edit.CaretPositions);

                // ---- Mouse: place the caret, select by dragging, by word (double click) or all (triple click) ----
                const float pointerX = input.MousePos.X - (textLeft - edit.ScrollX);
                const auto hitOffset = [&]()
                { return ToTextOffset(text, HitTest(display, edit.CaretPositions, pointerX), options.IsSecure); };
                if (isPressedHere && acceptsInput)
                {
                    const int clicks = input.MouseClickCount[static_cast<size_t>(MouseButton::Left)];
                    const bool isShiftHeld = HasModifiers(input.Modifiers, KeyModifiers::Shift);
                    if (clicks >= 3)
                        editor.SelectAll(text);
                    else if (clicks == 2)
                        editor.SelectWordAt(text, hitOffset());
                    else
                        editor.SetCaret(text, hitOffset(), isShiftHeld);
                    edit.IsDragSelecting = clicks == 1;
                }
                if (interactionState.ActiveID == id)
                {
                    interactionState.IsActiveAlive = true;
                    if (input.MouseDown[static_cast<size_t>(MouseButton::Left)])
                    {
                        // Composing started during the drag: the caret holds the pre-edit text in place.
                        if (edit.IsDragSelecting && !isPressedHere && acceptsInput && !showsComposition())
                            editor.SetCaret(text, hitOffset(), true);
                    }
                    else
                    {
                        interactionState.ActiveID = ID();
                    }
                }

                // ---- Keyboard ----
                if (acceptsInput)
                {
                    // One line: Home and End, and as on macOS Up and Down, go to the start and the end. While an
                    // input method composes, the keys are its own.
                    if (!Internal::IsComposing(context))
                    {
                        const bool isShiftHeld = HasModifiers(input.Modifiers, KeyModifiers::Shift);
                        const bool isUp = options.VerticalArrowsMoveCaret && IsKeyPressed(Key::UpArrow);
                        const bool isDown = options.VerticalArrowsMoveCaret && IsKeyPressed(Key::DownArrow);
                        if (IsKeyPressed(Key::Home) || isUp)
                            editor.MoveToStart(text, isShiftHeld);
                        if (IsKeyPressed(Key::End) || isDown)
                            editor.MoveToEnd(text, isShiftHeld);
                    }

                    changed = Internal::ApplyTextInput(context, editor, text, inputOptions) || changed;

                    if (!Internal::IsComposing(context))
                    {
                        submitted = IsKeyPressed(Key::Enter, false) || IsKeyPressed(Key::KeypadEnter, false);
                        if (IsKeyPressed(Key::Escape, false))
                            ClearFocus();
                    }
                }

                // The text may have changed: refresh what is on screen and where the caret can be. The pre-edit
                // text is laid out with the rest, as it changes from frame to frame.
                const bool isComposing = showsComposition();
                if (changed || isComposing)
                {
                    display = getDisplay();
                    GetCaretPositions(display, spec, edit.CaretPositions);
                }

                // Any caret movement restarts the blink, so the caret is visible while the user is working.
                if (editor.GetCaret() != caretBefore || editor.GetAnchor() != anchorBefore || changed ||
                    (isComposing && input.CompositionChanged))
                    edit.BlinkTime = 0.0f;
                else
                    edit.BlinkTime += context.DeltaTime;
                // The caret blinks: the next frame is due when it changes, and a host may sleep until then.
                const float halfPeriod = BlinkPeriod * 0.5f;
                RequestFrameAfter(halfPeriod - std::fmod(edit.BlinkTime, halfPeriod));

                // Scroll horizontally so the caret stays inside the field.
                const float caretX = edit.CaretPositions[getDisplayCaret()];
                const float contentWidth = edit.CaretPositions.back();
                if (caretX - edit.ScrollX > textWidth - CaretWidth)
                    edit.ScrollX = caretX - textWidth + CaretWidth;
                if (caretX < edit.ScrollX)
                    edit.ScrollX = caretX;
                edit.ScrollX = std::clamp(edit.ScrollX, 0.0f, std::max(0.0f, contentWidth - textWidth + CaretWidth));
            }

            // ---- Drawing ----
            DrawList& drawList = context.Draw;
            const float radius = metrics.CornerRadius;
            if (options.IsBezeled)
            {
                drawList.AddSquircle(rect, context.Style.GetColor(StyleColor::ControlBackground), radius);
                // The border gets stronger under the pointer: the field invites a click.
                const ControlFeedback feedback = AnimateFeedback(id, isHovered, false);
                const Color border = Blend(context.Style.GetColor(StyleColor::ControlBorder),
                                           context.Style.GetColor(StyleColor::Label), 0.3f * feedback.Hover);
                drawList.AddSquircleStroke(rect, border, radius, context.Style.GetVar(StyleVar::BorderWidth));
            }

            if (!options.Icon.empty())
            {
                TextSpec iconSpec = spec;
                iconSpec.MaxWidth = 0.0f;
                DrawLabel(drawList, rect, rect.X + HorizontalPadding, options.Icon, iconSpec,
                          context.Style.GetColor(StyleColor::SecondaryLabel));
            }
            if (hasClearButton)
            {
                TextSpec iconSpec = spec;
                iconSpec.Icons = IconVariant::Fill;
                DrawLabel(drawList, rect, clearRect.X, Icons::XCircle, iconSpec,
                          context.Style.GetColor(StyleColor::TertiaryLabel));
            }

            const float scrollX = isFocused ? edit.ScrollX : 0.0f;
            const float textX = context.Scale.Snap(textLeft - scrollX);
            const float lineTop = centerY - fontMetrics.LineHeight * 0.5f;
            const bool showsCaret = isFocused && acceptsInput;
            const bool isComposing = showsComposition();
            drawList.PushClipRect(Rect(textLeft, rect.Y, textWidth, rect.Height));
            if (showsCaret && editor.HasSelection())
            {
                const float start =
                    edit.CaretPositions[ToDisplayOffset(text, editor.GetSelectionStart(), options.IsSecure)];
                const float end =
                    edit.CaretPositions[ToDisplayOffset(text, editor.GetSelectionEnd(), options.IsSecure)];
                drawList.AddRect(Rect(textX + start, lineTop - 1.0f, end - start, fontMetrics.LineHeight + 2.0f),
                                 context.Style.GetColor(StyleColor::TextSelection));
            }
            if (display.empty())
            {
                const std::string_view placeholder =
                    options.Placeholder.empty() ? GetDisplayLabel(label) : options.Placeholder;
                TextSpec placeholderSpec = spec;
                placeholderSpec.MaxWidth = textWidth;
                placeholderSpec.Wraps = false;
                DrawLabel(drawList, rect, textLeft, placeholder, placeholderSpec,
                          context.Style.GetColor(StyleColor::TertiaryLabel));
            }
            else
            {
                const Color textColor = context.Style.GetColor(StyleColor::Label);
                DrawLabel(drawList, rect, textX, display, spec, textColor);
                if (isComposing)
                {
                    const size_t start = editor.GetCaret();
                    Internal::DrawCompositionUnderline(
                        context, drawList, start, start, start + composition.Text.size(), [&](size_t offset)
                        { return textX + edit.CaretPositions[offset]; }, lineTop + fontMetrics.LineHeight, textColor);
                }
            }
            if (showsCaret && !editor.HasSelection() && std::fmod(edit.BlinkTime, BlinkPeriod) < BlinkPeriod * 0.5f)
            {
                const float caretX = edit.CaretPositions[getDisplayCaret()];
                const Rect caret(textX + caretX, lineTop, CaretWidth, fontMetrics.LineHeight);
                drawList.AddRect(context.Scale.Snap(caret), context.Style.GetColor(StyleColor::Accent));
            }
            drawList.PopClipRect();

            // What a host's on-screen keyboard needs: the text and selection, and the field above the keyboard.
            if (isFocused && options.AcceptsInput)
            {
                Internal::PublishTextInput(context, id, text, editor.GetSelectionStart(), editor.GetSelectionEnd(),
                                           options.Keyboard, false, options.IsSecure);
                Internal::KeepAboveKeyboard(context, rect);
            }

            // Where the host's input method shows its candidates: at the caret, or the clause being converted.
            if (showsCaret)
            {
                size_t anchor = getDisplayCaret();
                if (isComposing)
                    anchor = ToDisplayOffset(text, editor.GetCaret(), false) + Internal::GetCompositionAnchor(context);
                const float x = std::clamp(textX + edit.CaretPositions[anchor], textLeft, textLeft + textWidth);
                interactionState.TextInputCaretRect = Rect(x, lineTop, CaretWidth, fontMetrics.LineHeight);
            }

            if (options.IsBezeled)
                DrawFocusRing(id, rect, radius, true);
            PopDisabled();

            Interaction interaction;
            interaction.Hovered = isHovered;
            interaction.Focused = isFocused;
            interaction.Pressed = interactionState.ActiveID == id;
            SetLastItem(id, rect, interaction);
            interactionState.LastItem.Submitted = submitted;
            return changed;
        }

    } // namespace

    bool TextField(std::string_view label, std::string* text, const TextFieldOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        CB_VERIFY(text != nullptr, "TextField needs a string to edit");
        if (text == nullptr)
            return false;
        return EditTextField(context, label, *text, 0, options);
    }

    bool TextField(std::string_view label, std::span<char> buffer, const TextFieldOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        CB_VERIFY(!buffer.empty(), "TextField needs a buffer with room for at least the terminating zero");
        if (buffer.empty())
            return false;

        // The field edits a copy whose capacity is kept between frames, so nothing allocates once it is large
        // enough. The text ends at the first zero, or one byte before the end of the buffer.
        std::string& text = context.TextEdit.BoundText;
        const size_t capacity = buffer.size() - 1;
        text.assign(buffer.data(), std::find(buffer.data(), buffer.data() + capacity, '\0'));
        const bool changed = EditTextField(context, label, text, capacity, options);
        std::copy(text.begin(), text.end(), buffer.data());
        buffer[text.size()] = '\0';
        return changed;
    }

    bool TextField(std::string_view label, std::string_view text, FunctionRef<void(std::string_view)> setText,
                   const TextFieldOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        std::string& edited = context.TextEdit.BoundText;
        edited.assign(text);
        const bool changed = EditTextField(context, label, edited, 0, options);
        if (changed)
            setText(edited);
        return changed;
    }

    void ReloadTextField(std::string_view label)
    {
        TextEditState& edit = Internal::GetContext().TextEdit;
        if (edit.Owner == GetID(label))
            edit.IsReloadPending = true;
    }

    bool GetTextFieldSelection(std::string_view label, TextFieldSelection* selection)
    {
        CB_VERIFY(selection != nullptr, "GetTextFieldSelection needs a TextFieldSelection to fill");
        const TextEditState& edit = Internal::GetContext().TextEdit;
        if (selection == nullptr || edit.Owner != GetID(label))
            return false;
        selection->Caret = edit.Editor.GetCaret();
        selection->Start = edit.Editor.GetSelectionStart();
        selection->End = edit.Editor.GetSelectionEnd();
        return true;
    }

    void SetTextFieldSelection(std::string_view label, const TextFieldSelection& selection)
    {
        Context& context = Internal::GetContext();
        TextEditState& edit = context.TextEdit;
        edit.PendingSelectionOwner = GetID(label);
        edit.PendingSelectionFrame = context.FrameCount;
        // The anchor is the end of the selection the caret is not at.
        edit.PendingAnchor = selection.Caret == selection.Start ? selection.End : selection.Start;
        edit.PendingCaret = selection.Caret;
    }
} // namespace Carbon
