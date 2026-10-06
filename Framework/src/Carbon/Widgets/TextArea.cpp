#include "Carbon/Widgets/TextArea.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Animation/Animation.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/State.h"
#include "Carbon/Core/UTF8.h"
#include "Carbon/Input/Input.h"
#include "Carbon/Interaction/Interaction.h"
#include "Carbon/Layout/Layout.h"
#include "Carbon/Layout/ScrollView.h"
#include "Carbon/Style/Style.h"
#include "Carbon/Widgets/ControlFeedback.h"
#include "Carbon/Widgets/Internal/TextInput.h"

namespace Carbon
{
    namespace
    {
        using Internal::TextAreaLine;
        using Internal::TextEditor;
        using Internal::TextEditState;

        // Space between the bezel and the text.
        constexpr float HorizontalPadding = 6.0f;
        constexpr float VerticalPadding = 4.0f;
        constexpr float DefaultWidth = 240.0f;
        constexpr float CaretWidth = 1.5f;
        // The caret is visible for the first half of every blink period.
        constexpr float BlinkPeriod = 1.0f;
        // Tab stops are this many spaces apart.
        constexpr float TabColumns = 4.0f;
        // A selection that includes a line break shows it as a little extra width at the end of its line.
        constexpr float LineBreakSelectionWidth = 4.0f;

        // What a text area keeps between frames, besides the scroll offset that its scroll view keeps.
        struct TextAreaState
        {
            /// Where the caret was horizontally before a run of up and down moves, so that the run keeps to one
            /// column across shorter lines.
            float PreferredX;
            bool HasPreferredX;
            /// Frames left in which the caret is scrolled into view. Two, because the scroll view clamps its
            /// offset to the content measured in the frame before, which a new last line has not reached yet.
            int RevealFrames;
        };

        // The visual lines of the text, and the caret's position before each byte relative to its line.
        struct AreaLayout
        {
            const std::vector<TextAreaLine>& Lines;
            const std::vector<float>& CaretX;

            // The last line that starts at or before the offset: at a wrap, the caret belongs to the next line.
            size_t FindLine(size_t offset) const
            {
                const auto after =
                    std::upper_bound(Lines.begin(), Lines.end(), offset,
                                     [](size_t value, const TextAreaLine& line) { return value < line.Start; });
                return after == Lines.begin() ? 0 : static_cast<size_t>(after - Lines.begin()) - 1;
            }

            // The last offset at which the caret can sit on a line: before the character at the wrap.
            size_t GetLastOffset(std::string_view text, const TextAreaLine& line) const
            {
                if (line.EndsParagraph || line.End <= line.Start)
                    return line.End;
                return PreviousCodepointOffset(text, line.End);
            }

            // The character boundary of a line whose caret position is closest to `x`.
            size_t HitTestLine(std::string_view text, size_t lineIndex, float x) const
            {
                const TextAreaLine& line = Lines[std::min(lineIndex, Lines.size() - 1)];
                const size_t last = GetLastOffset(text, line);
                size_t best = line.Start;
                float bestDistance = std::abs(CaretX[line.Start] - x);
                for (size_t offset = line.Start; offset < last;)
                {
                    offset = NextCodepointOffset(text, offset);
                    const float distance = std::abs(CaretX[offset] - x);
                    if (distance < bestDistance)
                    {
                        best = offset;
                        bestDistance = distance;
                    }
                }
                return best;
            }

            // The offset under a point given relative to the top-left of the text.
            size_t HitTest(std::string_view text, Vec2 point, float lineHeight) const
            {
                const float line = std::floor(point.Y / lineHeight);
                const float lastLine = static_cast<float>(Lines.size() - 1);
                return HitTestLine(text, static_cast<size_t>(std::clamp(line, 0.0f, lastLine)), point.X);
            }
        };

        // Breaks the text into visual lines no wider than `wrapWidth` and finds the caret's position before each
        // byte. Lines break after the last space that fits; a word longer than a line breaks anywhere. Spaces at
        // the end of a line hang past the width. Tabs advance to the next tab stop.
        void LayOutText(std::string_view text, const TextSpec& spec, float wrapWidth, float tabWidth,
                        TextEditState& edit)
        {
            std::vector<TextAreaLine>& lines = edit.AreaLines;
            std::vector<float>& caretX = edit.AreaCaretX;
            std::vector<float>& positions = edit.CaretPositions;
            lines.clear();
            caretX.assign(text.size() + 1, 0.0f);

            size_t start = 0;
            while (true)
            {
                const size_t newline = text.find('\n', start);
                const size_t end = newline == std::string_view::npos ? text.size() : newline;
                const std::string_view paragraph = text.substr(start, end - start);

                // Tabs are shaped as spaces, then widened to the next stop.
                const bool hasTabs = paragraph.find('\t') != std::string_view::npos;
                std::string_view shaped = paragraph;
                if (hasTabs)
                {
                    edit.AreaParagraph.assign(paragraph);
                    std::replace(edit.AreaParagraph.begin(), edit.AreaParagraph.end(), '\t', ' ');
                    shaped = edit.AreaParagraph;
                }
                GetCaretPositions(shaped, spec, positions);
                if (hasTabs && tabWidth > 0.0f)
                {
                    float shift = 0.0f;
                    for (size_t i = 0; i < paragraph.size(); i++)
                    {
                        const float space = positions[i + 1] - positions[i];
                        positions[i] += shift;
                        if (paragraph[i] == '\t')
                        {
                            const float stop = (std::floor(positions[i] / tabWidth) + 1.0f) * tabWidth;
                            shift += stop - positions[i] - space;
                        }
                    }
                    positions[paragraph.size()] += shift;
                }

                const size_t firstLine = lines.size();
                size_t lineStart = 0;
                size_t lastBreak = std::string_view::npos;
                size_t i = 0;
                while (i < paragraph.size())
                {
                    const size_t next = NextCodepointOffset(paragraph, i);
                    if (paragraph[i] == ' ' || paragraph[i] == '\t')
                    {
                        lastBreak = next;
                        i = next;
                        continue;
                    }
                    if (wrapWidth > 0.0f && i > lineStart && positions[next] - positions[lineStart] > wrapWidth)
                    {
                        const bool hasBreak = lastBreak != std::string_view::npos && lastBreak > lineStart;
                        const size_t breakAt = hasBreak ? lastBreak : i;
                        lines.push_back(TextAreaLine{start + lineStart, start + breakAt, 0.0f, false});
                        lineStart = breakAt;
                        lastBreak = std::string_view::npos;
                        continue; // the same character again, now on the new line
                    }
                    i = next;
                }
                lines.push_back(TextAreaLine{start + lineStart, end, 0.0f, true});

                for (size_t index = firstLine; index < lines.size(); index++)
                {
                    TextAreaLine& line = lines[index];
                    const float lineX = positions[line.Start - start];
                    line.Width = positions[line.End - start] - lineX;
                    // At a wrap, the end of this line is the start of the next, whose position the caret takes.
                    const size_t last = line.EndsParagraph ? line.End : line.End - 1;
                    for (size_t offset = line.Start; offset <= last; offset++)
                        caretX[offset] = positions[offset - start] - lineX;
                }

                if (newline == std::string_view::npos)
                    break;
                start = newline + 1;
            }
        }

        // The text area, for all three ways of passing the text. `maxBytes` limits the size of the text in bytes
        // (0 = unlimited), for a caller's fixed buffer.
        bool EditTextArea(Context& context, std::string_view label, std::string& text, size_t maxBytes,
                          const TextAreaOptions& options)
        {
            Internal::InteractionState& interactionState = context.Interaction;
            const Internal::InputState& input = context.Input;
            TextEditState& edit = context.TextEdit;
            TextEditor& editor = edit.Editor;

            const ID id = GetID(label);
            TextAreaState& state = *GetState<TextAreaState>(id);
            const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
            const TextSpec spec = GetTextSpec(metrics.Style);
            const float lineHeight = GetFontMetrics(spec).LineHeight;
            GetCaretPositions(" ", spec, edit.CaretPositions);
            const float tabWidth = edit.CaretPositions.back() * TabColumns;
            bool changed = false;

            PushDisabled(options.Disabled);
            const bool isDisabled = interactionState.DisabledDepth > 0;

            // The text wraps inside the width the scroll view is going to get.
            const Size width = options.Width.Mode == SizeMode::Fit ? Size::Fixed(DefaultWidth) : options.Width;
            const float areaWidth = ResolveItemSize(Vec2(DefaultWidth, 0.0f), {.Width = width}).X;
            const float wrapWidth = std::max(areaWidth - HorizontalPadding * 2.0f, 1.0f);
            // Height of the visible text for a given height of the text; a text area that fits grows with it.
            const auto getVisibleHeight = [&](float textHeight)
            {
                const Vec2 fit(areaWidth, textHeight + VerticalPadding * 2.0f);
                return ResolveItemSize(fit, {.Height = options.Height}).Y - VerticalPadding * 2.0f;
            };

            // Editing starts when the text area gets the focus and ends when it loses it.
            const auto updateSession = [&]
            {
                const bool isFocused = !isDisabled && IsFocused(id);
                if (isFocused && edit.Owner != id)
                {
                    edit.Owner = id;
                    edit.BlinkTime = 0.0f;
                    editor.Reset(text);
                    state.HasPreferredX = false;
                }
                else if (!isFocused && edit.Owner == id)
                {
                    edit.Owner = ID();
                }
                return isFocused;
            };
            bool isFocused = updateSession();

            LayOutText(text, spec, wrapWidth, tabWidth, edit);
            const AreaLayout layout{edit.AreaLines, edit.AreaCaretX};
            const size_t caretBefore = isFocused ? editor.GetCaret() : 0;
            const size_t anchorBefore = isFocused ? editor.GetAnchor() : 0;
            bool keepsPreferredX = false;

            // ---- Keyboard ----
            if (isFocused)
            {
                interactionState.IsTextInputActive = true;
                editor.SetMultiLine(true);
                editor.ClampTo(text);
                if (options.AcceptsTab)
                    Internal::TakeTabKey(context, id);

                const bool isShiftHeld = HasModifiers(input.Modifiers, KeyModifiers::Shift);
                const bool isShortcutHeld = HasModifiers(input.Modifiers, context.HostIO.GetShortcutModifier());

                // Up, Down, Page Up and Page Down move between lines and keep to one column.
                const float textHeight = lineHeight * static_cast<float>(layout.Lines.size());
                const int pageLines = std::max(1, static_cast<int>(getVisibleHeight(textHeight) / lineHeight) - 1);
                int lineSteps = 0;
                if (IsKeyPressed(Key::UpArrow))
                    lineSteps -= 1;
                if (IsKeyPressed(Key::DownArrow))
                    lineSteps += 1;
                if (IsKeyPressed(Key::PageUp))
                    lineSteps -= pageLines;
                if (IsKeyPressed(Key::PageDown))
                    lineSteps += pageLines;
                if (lineSteps != 0)
                {
                    // Past the first line is the start of the text, past the last its end, as on macOS.
                    const size_t caret = editor.GetCaret();
                    const float x = state.HasPreferredX ? state.PreferredX : layout.CaretX[caret];
                    const long target = static_cast<long>(layout.FindLine(caret)) + lineSteps;
                    const bool isInside = target >= 0 && target < static_cast<long>(layout.Lines.size());
                    size_t offset = 0;
                    if (isInside)
                        offset = layout.HitTestLine(text, static_cast<size_t>(target), x);
                    else if (target > 0)
                        offset = text.size();
                    editor.SetCaret(text, offset, isShiftHeld);
                    // Reaching the start or the end of the text ends the run: the column starts over from there.
                    state.PreferredX = x;
                    state.HasPreferredX = isInside;
                    keepsPreferredX = isInside;
                }

                // Home and End go to the ends of the line; with the shortcut modifier, of the text.
                if (IsKeyPressed(Key::Home))
                {
                    const size_t offset = isShortcutHeld ? 0 : layout.Lines[layout.FindLine(editor.GetCaret())].Start;
                    editor.SetCaret(text, offset, isShiftHeld);
                }
                if (IsKeyPressed(Key::End))
                {
                    const TextAreaLine& line = layout.Lines[layout.FindLine(editor.GetCaret())];
                    editor.SetCaret(text, isShortcutHeld ? text.size() : layout.GetLastOffset(text, line), isShiftHeld);
                }

                Internal::TextInputOptions inputOptions;
                inputOptions.MaxLength = options.MaxLength;
                inputOptions.MaxBytes = maxBytes;
                changed = Internal::ApplyTextInput(context, editor, text, inputOptions) || changed;

                if (IsKeyPressed(Key::Enter) || IsKeyPressed(Key::KeypadEnter))
                    changed = editor.Insert(text, "\n", options.MaxLength, maxBytes) || changed;
                // A tab only for Tab alone: Ctrl+Tab and Shift+Tab move the focus.
                if (options.AcceptsTab && IsKeyPressed(Key::Tab) && input.Modifiers == KeyModifiers::None)
                    changed = editor.Insert(text, "\t", options.MaxLength, maxBytes) || changed;
                if (IsKeyPressed(Key::Escape, false))
                    ClearFocus();
            }
            if (changed)
                LayOutText(text, spec, wrapWidth, tabWidth, edit);
            const float textHeight = lineHeight * static_cast<float>(layout.Lines.size());

            // After the caret moved, scroll just enough to show it, at once.
            if (isFocused && (changed || editor.GetCaret() != caretBefore || editor.GetAnchor() != anchorBefore))
            {
                state.RevealFrames = 2;
                edit.BlinkTime = 0.0f;
            }
            if (state.RevealFrames > 0 && edit.Owner == id)
            {
                state.RevealFrames--;
                const float visibleHeight = getVisibleHeight(textHeight);
                const float caretY = lineHeight * static_cast<float>(layout.FindLine(editor.GetCaret()));
                const Vec2 offset = GetScrollOffset(label);
                float target = offset.Y;
                if (caretY + lineHeight > target + visibleHeight)
                    target = caretY + lineHeight - visibleHeight;
                if (caretY < target)
                    target = caretY;
                if (target != offset.Y)
                    SetScrollOffset(label, Vec2(offset.X, target), false);
            }

            // ---- The scrolling area: the text is one item in a scroll view ----
            ScrollViewOptions scroll;
            scroll.Width = width;
            scroll.Height = options.Height;
            scroll.Spacing = 0.0f;
            scroll.Padding = EdgeInsets(HorizontalPadding, VerticalPadding);
            BeginScrollView(label, scroll);
            const Rect content = AllocateItem(Vec2(wrapWidth, textHeight));
            const Rect inner = GetContentRect();
            const Rect viewport(inner.X - HorizontalPadding, inner.Y - VerticalPadding,
                                inner.Width + HorizontalPadding * 2.0f, inner.Height + VerticalPadding * 2.0f);

            // ---- Mouse: place the caret, select by dragging, by word (double click) or all (triple click) ----
            bool isHovered = false;
            if (!isDisabled)
            {
                isHovered = Internal::UpdateHover(context, id, viewport);
                if (isHovered)
                    SetCursor(Cursor::IBeam);
                const Vec2 pointer = input.MousePos - content.GetMin();
                if (isHovered && input.MousePressed[static_cast<size_t>(MouseButton::Left)] &&
                    !interactionState.ActiveID.IsValid())
                {
                    interactionState.ActiveID = id;
                    interactionState.IsActiveAlive = true;
                    SetFocus(id, false);
                    isFocused = updateSession();
                    editor.SetMultiLine(true);
                    const int clicks = input.MouseClickCount[static_cast<size_t>(MouseButton::Left)];
                    if (clicks >= 3)
                        editor.SelectAll(text);
                    else if (clicks == 2)
                        editor.SelectWordAt(text, layout.HitTest(text, pointer, lineHeight));
                    else
                        editor.SetCaret(text, layout.HitTest(text, pointer, lineHeight),
                                        HasModifiers(input.Modifiers, KeyModifiers::Shift));
                    edit.IsDragSelecting = clicks == 1;
                    edit.BlinkTime = 0.0f;
                }
                else if (interactionState.ActiveID == id)
                {
                    interactionState.IsActiveAlive = true;
                    if (!input.MouseDown[static_cast<size_t>(MouseButton::Left)])
                    {
                        interactionState.ActiveID = ID();
                    }
                    else if (edit.IsDragSelecting && edit.Owner == id)
                    {
                        // Dragging past the top or the bottom selects on, and scrolls as the caret follows.
                        const size_t offset = layout.HitTest(text, pointer, lineHeight);
                        if (offset != editor.GetCaret())
                        {
                            editor.SetCaret(text, offset, true);
                            state.RevealFrames = 1;
                            edit.BlinkTime = 0.0f;
                        }
                    }
                }
            }
            if (!keepsPreferredX && isFocused && editor.GetCaret() != caretBefore)
                state.HasPreferredX = false;

            // ---- Drawing ----
            DrawList& drawList = context.Draw;
            const float radius = metrics.CornerRadius;
            drawList.AddSquircle(viewport, context.Style.GetColor(StyleColor::ControlBackground), radius);

            const bool isEditing = isFocused && edit.Owner == id;
            if (isEditing)
            {
                // The caret blinks: the next frame is due when it changes, and a host may sleep until then.
                edit.BlinkTime += context.DeltaTime;
                const float halfPeriod = BlinkPeriod * 0.5f;
                RequestFrameAfter(halfPeriod - std::fmod(edit.BlinkTime, halfPeriod));
            }

            // Only the lines inside the viewport are drawn.
            const std::vector<TextAreaLine>& lines = layout.Lines;
            const float firstLine = std::floor((viewport.Y - content.Y) / lineHeight);
            const float endLine = std::ceil((viewport.GetBottom() - content.Y) / lineHeight);
            const size_t firstVisible = static_cast<size_t>(std::max(0.0f, firstLine));
            const size_t endVisible = std::min(lines.size(), static_cast<size_t>(std::max(0.0f, endLine)));
            const size_t selectionStart = isEditing ? editor.GetSelectionStart() : 0;
            const size_t selectionEnd = isEditing ? editor.GetSelectionEnd() : 0;
            const Color selectionColor = context.Style.GetColor(StyleColor::TextSelection);
            const Color textColor = context.Style.GetColor(StyleColor::Label);
            const std::string_view all = text;
            for (size_t index = firstVisible; index < endVisible; index++)
            {
                const TextAreaLine& line = lines[index];
                const float y = content.Y + lineHeight * static_cast<float>(index);

                if (selectionStart < selectionEnd && selectionStart <= line.End && selectionEnd > line.Start)
                {
                    const size_t from = std::max(selectionStart, line.Start);
                    const size_t to = std::min(selectionEnd, line.End);
                    float right = content.X + (to == line.End ? line.Width : layout.CaretX[to]);
                    // A selected line break shows as a little extra width.
                    if (line.EndsParagraph && selectionEnd > line.End)
                        right += LineBreakSelectionWidth;
                    const float left = content.X + layout.CaretX[from];
                    if (right > left)
                        drawList.AddRect(Rect(left, y, right - left, lineHeight), selectionColor);
                }

                // Text between tabs is drawn piece by piece at its tab stops.
                size_t piece = line.Start;
                while (piece < line.End)
                {
                    const size_t tab = all.find('\t', piece);
                    const size_t pieceEnd = std::min(tab, line.End);
                    if (pieceEnd > piece)
                    {
                        drawList.AddText(context.Scale.Snap(Vec2(content.X + layout.CaretX[piece], y)),
                                         all.substr(piece, pieceEnd - piece), spec, textColor);
                    }
                    piece = pieceEnd + 1;
                }
            }

            if (text.empty())
            {
                const std::string_view placeholder =
                    options.Placeholder.empty() ? GetDisplayLabel(label) : options.Placeholder;
                TextSpec placeholderSpec = spec;
                placeholderSpec.MaxWidth = wrapWidth;
                drawList.AddText(context.Scale.Snap(content.GetMin()), placeholder, placeholderSpec,
                                 context.Style.GetColor(StyleColor::TertiaryLabel));
            }

            if (isEditing && !editor.HasSelection() && std::fmod(edit.BlinkTime, BlinkPeriod) < BlinkPeriod * 0.5f)
            {
                // After spaces that hang past the end of a line, the caret waits at the edge.
                const size_t caret = editor.GetCaret();
                const float x = std::min(layout.CaretX[caret], wrapWidth);
                const float y = lineHeight * static_cast<float>(layout.FindLine(caret));
                const Rect caretRect(content.X + x, content.Y + y, CaretWidth, lineHeight);
                drawList.AddRect(context.Scale.Snap(caretRect), context.Style.GetColor(StyleColor::Accent));
            }

            EndScrollView();
            const Rect rect = GetLastItemRect();
            // A Tab stop. Registered with the area's own rectangle, outside its scroll view, so that arriving by
            // Tab scrolls the views around it to show the whole area.
            RegisterFocusable(id, rect);

            // The border gets stronger under the pointer; the focus ring shows whenever the area is focused.
            const ControlFeedback feedback = AnimateFeedback(id, isHovered, false);
            const Color border = Blend(context.Style.GetColor(StyleColor::ControlBorder),
                                       context.Style.GetColor(StyleColor::Label), 0.3f * feedback.Hover);
            drawList.AddSquircleStroke(rect, border, radius, context.Style.GetVar(StyleVar::BorderWidth));
            DrawFocusRing(id, rect, radius, true);
            PopDisabled();

            Interaction interaction;
            interaction.Hovered = isHovered;
            interaction.Focused = isFocused;
            interaction.Pressed = interactionState.ActiveID == id;
            SetLastItem(id, rect, interaction);
            return changed;
        }
    } // namespace

    bool TextArea(std::string_view label, std::string* text, const TextAreaOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        CB_VERIFY(text != nullptr, "TextArea needs a string to edit");
        if (text == nullptr)
            return false;
        return EditTextArea(context, label, *text, 0, options);
    }

    bool TextArea(std::string_view label, std::span<char> buffer, const TextAreaOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        CB_VERIFY(!buffer.empty(), "TextArea needs a buffer with room for at least the terminating zero");
        if (buffer.empty())
            return false;
        // As for TextField: the area edits a copy whose capacity is kept, and the text ends at the first zero.
        std::string& text = context.TextEdit.BoundText;
        const size_t capacity = buffer.size() - 1;
        text.assign(buffer.data(), std::find(buffer.data(), buffer.data() + capacity, '\0'));
        const bool changed = EditTextArea(context, label, text, capacity, options);
        std::copy(text.begin(), text.end(), buffer.data());
        buffer[text.size()] = '\0';
        return changed;
    }

    bool TextArea(std::string_view label, std::string_view text, FunctionRef<void(std::string_view)> setText,
                  const TextAreaOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        std::string& edited = context.TextEdit.BoundText;
        edited.assign(text);
        const bool changed = EditTextArea(context, label, edited, 0, options);
        if (changed)
            setText(edited);
        return changed;
    }
} // namespace Carbon
