#include "Carbon/Widgets/TextField.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/UTF8.h"
#include "Carbon/Input/Input.h"
#include "Carbon/Interaction/Interaction.h"
#include "Carbon/Layout/Layout.h"
#include "Carbon/Style/Style.h"
#include "Carbon/Text/Icons.h"
#include "Carbon/Widgets/ControlFeedback.h"

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

    bool TextField(std::string_view label, std::string* text, const TextFieldOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        CB_VERIFY(text != nullptr, "TextField needs a string to edit");
        if (text == nullptr)
            return false;

        Internal::InteractionState& interactionState = context.Interaction;
        const Internal::InputState& input = context.Input;
        TextEditState& edit = context.TextEdit;
        TextEditor& editor = edit.Editor;

        const ID id = GetID(label);
        const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
        const TextSpec spec = GetTextSpec(metrics.Style);
        const FontMetrics fontMetrics = GetFontMetrics(spec);
        bool changed = false;
        bool submitted = false;

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
        const bool hasClearButton = options.ShowsClearButton && !text->empty() && !isDisabled;
        const Rect clearRect(trailing - HorizontalPadding - iconSize, centerY - iconSize * 0.5f, iconSize, iconSize);
        if (hasClearButton)
            textRight = clearRect.X - IconGap;
        const float textWidth = std::max(textRight - textLeft, 1.0f);

        bool isHovered = false;
        bool isPressedHere = false;
        if (!isDisabled)
        {
            RegisterFocusable(id, rect);
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
                    text->clear();
                    changed = true;
                    if (edit.Owner == id)
                        editor.Reset(*text);
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
            edit.Owner = ID();
        }

        const bool isFocused = !isDisabled && IsFocused(id);
        if (isFocused && edit.Owner != id)
        {
            // Editing starts. Arriving by keyboard selects everything, as on macOS; a click places the caret.
            edit.Owner = id;
            edit.ScrollX = 0.0f;
            edit.BlinkTime = 0.0f;
            editor.Reset(*text);
            if (interactionState.DidFocusMove)
                editor.SelectAll(*text);
        }
        else if (!isFocused && edit.Owner == id)
        {
            edit.Owner = ID();
        }
        if (edit.Owner == id && edit.IsReloadPending)
        {
            editor.Reset(*text);
            edit.IsReloadPending = false;
        }

        // What is on screen: the text itself, or one bullet per character.
        std::string_view display = *text;
        if (options.IsSecure)
        {
            edit.SecureText.clear();
            const size_t count = CountCodepoints(*text);
            for (size_t i = 0; i < count; i++)
                edit.SecureText.append(Bullet);
            display = edit.SecureText;
        }

        if (isFocused)
        {
            interactionState.IsTextInputActive = true;
            editor.ClampTo(*text);
            GetCaretPositions(display, spec, edit.CaretPositions);
            const size_t caretBefore = editor.GetCaret();
            const size_t anchorBefore = editor.GetAnchor();

            // ---- Mouse: place the caret, select by dragging, by word (double click) or all (triple click) ----
            const float pointerX = input.MousePos.X - (textLeft - edit.ScrollX);
            const auto hitOffset = [&]()
            { return ToTextOffset(*text, HitTest(display, edit.CaretPositions, pointerX), options.IsSecure); };
            if (isPressedHere)
            {
                const int clicks = input.MouseClickCount[static_cast<size_t>(MouseButton::Left)];
                const bool isShiftHeld = HasModifiers(input.Modifiers, KeyModifiers::Shift);
                if (clicks >= 3)
                    editor.SelectAll(*text);
                else if (clicks == 2)
                    editor.SelectWordAt(*text, hitOffset());
                else
                    editor.SetCaret(*text, hitOffset(), isShiftHeld);
                edit.IsDragSelecting = clicks == 1;
            }
            if (interactionState.ActiveID == id)
            {
                interactionState.IsActiveAlive = true;
                if (input.MouseDown[static_cast<size_t>(MouseButton::Left)])
                {
                    if (edit.IsDragSelecting && !isPressedHere)
                        editor.SetCaret(*text, hitOffset(), true);
                }
                else
                {
                    interactionState.ActiveID = ID();
                }
            }

            // ---- Keyboard ----
            const KeyModifiers shortcut = context.HostIO.GetShortcutModifier();
            const bool isShiftHeld = HasModifiers(input.Modifiers, KeyModifiers::Shift);
            const bool byWord =
                HasModifiers(input.Modifiers, shortcut) || HasModifiers(input.Modifiers, KeyModifiers::Alt);

            if (IsKeyPressed(Key::LeftArrow))
                editor.MoveLeft(*text, isShiftHeld, byWord);
            if (IsKeyPressed(Key::RightArrow))
                editor.MoveRight(*text, isShiftHeld, byWord);
            const bool isUp = options.VerticalArrowsMoveCaret && IsKeyPressed(Key::UpArrow);
            const bool isDown = options.VerticalArrowsMoveCaret && IsKeyPressed(Key::DownArrow);
            if (IsKeyPressed(Key::Home) || isUp)
                editor.MoveToStart(*text, isShiftHeld);
            if (IsKeyPressed(Key::End) || isDown)
                editor.MoveToEnd(*text, isShiftHeld);
            if (IsKeyPressed(Key::Backspace))
                changed = editor.DeleteBackward(*text, byWord) || changed;
            if (IsKeyPressed(Key::Delete))
                changed = editor.DeleteForward(*text, byWord) || changed;

            if (IsShortcutPressed(Key::A))
                editor.SelectAll(*text);
            // A secure field never puts its text on the clipboard.
            const bool canCopy = editor.HasSelection() && !options.IsSecure && context.HostCallbacks.SetClipboardText;
            if (IsShortcutPressed(Key::C) && canCopy)
                context.HostCallbacks.SetClipboardText(editor.GetSelectedText(*text));
            if (IsShortcutPressed(Key::X) && canCopy)
            {
                context.HostCallbacks.SetClipboardText(editor.GetSelectedText(*text));
                changed = editor.DeleteSelection(*text) || changed;
            }
            if (IsShortcutPressed(Key::V) && context.HostCallbacks.GetClipboardText)
                changed = editor.Insert(*text, context.HostCallbacks.GetClipboardText(), options.MaxLength) || changed;
            if (IsShortcutPressed(Key::Z))
                changed = editor.Undo(*text) || changed;
            if (IsShortcutPressed(Key::Z, KeyModifiers::Shift) || IsShortcutPressed(Key::Y))
                changed = editor.Redo(*text) || changed;

            for (const char32_t character : input.Characters)
            {
                char encoded[4];
                const uint32_t length = EncodeUTF8(character, encoded);
                changed = editor.Insert(*text, std::string_view(encoded, length), options.MaxLength) || changed;
            }

            submitted = IsKeyPressed(Key::Enter, false) || IsKeyPressed(Key::KeypadEnter, false);
            if (IsKeyPressed(Key::Escape, false))
                ClearFocus();

            // The text may have changed: refresh what is on screen and where the caret can be.
            if (changed)
            {
                display = *text;
                if (options.IsSecure)
                {
                    edit.SecureText.clear();
                    const size_t count = CountCodepoints(*text);
                    for (size_t i = 0; i < count; i++)
                        edit.SecureText.append(Bullet);
                    display = edit.SecureText;
                }
                GetCaretPositions(display, spec, edit.CaretPositions);
            }

            // Any caret movement restarts the blink, so the caret is visible while the user is working.
            if (editor.GetCaret() != caretBefore || editor.GetAnchor() != anchorBefore || changed)
                edit.BlinkTime = 0.0f;
            else
                edit.BlinkTime += context.DeltaTime;
            context.IsAnimatingThisFrame = true; // the caret blinks

            // Scroll horizontally so the caret stays inside the field.
            const float caretX = edit.CaretPositions[ToDisplayOffset(*text, editor.GetCaret(), options.IsSecure)];
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
        drawList.AddSquircle(rect, context.Style.GetColor(StyleColor::ControlBackground), radius);
        // The border gets stronger under the pointer: the field invites a click.
        const ControlFeedback feedback = AnimateFeedback(id, isHovered, false);
        const Color border = Blend(context.Style.GetColor(StyleColor::ControlBorder),
                                   context.Style.GetColor(StyleColor::Label), 0.3f * feedback.Hover);
        drawList.AddSquircleStroke(rect, border, radius, context.Style.GetVar(StyleVar::BorderWidth));

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
        drawList.PushClipRect(Rect(textLeft, rect.Y, textWidth, rect.Height));
        if (isFocused && editor.HasSelection())
        {
            const float start =
                edit.CaretPositions[ToDisplayOffset(*text, editor.GetSelectionStart(), options.IsSecure)];
            const float end = edit.CaretPositions[ToDisplayOffset(*text, editor.GetSelectionEnd(), options.IsSecure)];
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
            DrawLabel(drawList, rect, textX, display, spec, context.Style.GetColor(StyleColor::Label));
        }
        if (isFocused && !editor.HasSelection() && std::fmod(edit.BlinkTime, BlinkPeriod) < BlinkPeriod * 0.5f)
        {
            const float caretX = edit.CaretPositions[ToDisplayOffset(*text, editor.GetCaret(), options.IsSecure)];
            const Rect caret(textX + caretX, lineTop, CaretWidth, fontMetrics.LineHeight);
            drawList.AddRect(context.Scale.Snap(caret), context.Style.GetColor(StyleColor::Accent));
        }
        drawList.PopClipRect();

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

    void ReloadTextField(std::string_view label)
    {
        TextEditState& edit = Internal::GetContext().TextEdit;
        if (edit.Owner == GetID(label))
            edit.IsReloadPending = true;
    }
} // namespace Carbon
