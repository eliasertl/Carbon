#include "Carbon/Extensions/Internal/NumberEditing.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <string>

namespace Carbon::Internal
{
    namespace
    {
        // Room for the text of a number with its prefix and suffix while it is shown or typed.
        constexpr size_t TextCapacity = 64;
        // Points the pointer must travel before a press on a scrub field becomes a drag instead of a click.
        constexpr float ScrubThreshold = 3.0f;
        constexpr float ChevronInset = 5.0f;

        // Per-ID state, kept while the control is shown. The text lives here, in a fixed buffer that the text field
        // edits directly.
        struct NumberEditState
        {
            char Text[TextCapacity];
            /// The value Text was last written from.
            double ShownValue;
            /// The unrounded value during a scrub, so that slow drags still add up.
            double DragValue;
            bool HasText;
            /// The user typed since Text was last written from the value.
            bool IsDirty;
            bool WasFocused;
            /// The current press has moved past the threshold and changes the value.
            bool IsScrubbing;
        };

        // Shift for fine steps, Ctrl for coarse ones.
        double GetStepScale()
        {
            const KeyModifiers modifiers = GetKeyModifiers();
            if (HasModifiers(modifiers, KeyModifiers::Shift))
                return 0.1;
            if (HasModifiers(modifiers, KeyModifiers::Ctrl))
                return 10.0;
            return 1.0;
        }

        // Clamps to the options' range and to what the bound type can hold, rounding for integers.
        double Normalize(double value, NumberKind kind, const NumberEditOptions& options)
        {
            value = std::clamp(value, options.Min, options.Max);
            switch (kind)
            {
                case NumberKind::Int:
                {
                    // A range without a whole number in it, such as [0.2, 0.8], gives the first integer above Min.
                    const double low = std::max(std::ceil(options.Min), double(std::numeric_limits<int>::min()));
                    const double high = std::min(std::floor(options.Max), double(std::numeric_limits<int>::max()));
                    return std::clamp(std::round(value), low, std::max(low, high));
                }
                case NumberKind::Float:
                {
                    const double limit = double(std::numeric_limits<float>::max());
                    return double(static_cast<float>(std::clamp(value, -limit, limit)));
                }
                case NumberKind::Double:
                    break;
            }
            return value;
        }

        void ShowValue(NumberEditState& state, double value, const NumberFormat& format)
        {
            const std::string_view text = FormatNumber(value, format, std::span<char>(state.Text, TextCapacity - 1));
            state.Text[text.size()] = '\0';
            state.ShownValue = value;
            state.HasText = true;
            state.IsDirty = false;
        }

        // Applies what the user typed, if it is a number, and shows the resulting value.
        bool Commit(NumberEditState& state, double& value, NumberKind kind, const NumberEditOptions& options)
        {
            bool changed = false;
            double parsed = 0.0;
            if (state.IsDirty && ParseNumber(state.Text, options.Format, &parsed))
            {
                const double next = Normalize(parsed, kind, options);
                changed = next != value;
                value = next;
            }
            ShowValue(state, value, options.Format);
            return changed;
        }

        // The look of a scrub field while it is not being typed into: a field with the value in its middle and,
        // under the pointer, chevrons at both ends that tell it can be dragged.
        void DrawScrubField(ID id, const Rect& rect, std::string_view text, const DragInteraction& drag,
                            ControlSize size)
        {
            DrawList& drawList = GetDrawList();
            const ControlMetrics metrics = GetControlMetrics(size);
            const TextSpec spec = GetTextSpec(metrics.Style);
            const float radius = metrics.CornerRadius;
            const ControlFeedback feedback = AnimateFeedback(id, drag.Hovered || drag.Active, drag.Active);

            drawList.AddSquircle(rect, GetStyleColor(StyleColor::ControlBackground), radius);
            const Color border = Blend(GetStyleColor(StyleColor::ControlBorder), GetStyleColor(StyleColor::Label),
                                       0.3f * feedback.Hover);
            drawList.AddSquircleStroke(rect, border, radius, GetStyleVar(StyleVar::BorderWidth));

            const float iconSize = GetFontMetrics(spec).LineHeight * 0.75f;
            if (feedback.Hover > 0.01f)
            {
                const Color chevron = GetStyleColor(StyleColor::SecondaryLabel).WithOpacity(feedback.Hover);
                const float y = rect.GetCenter().Y;
                DrawIcon(drawList, Vec2(rect.X + ChevronInset + iconSize * 0.5f, y), Icons::CaretLeft, iconSize,
                         chevron, IconVariant::Bold);
                DrawIcon(drawList, Vec2(rect.GetRight() - ChevronInset - iconSize * 0.5f, y), Icons::CaretRight,
                         iconSize, chevron, IconVariant::Bold);
            }

            // The value is centered between the chevrons and cut off with an ellipsis when it does not fit.
            TextSpec valueSpec = spec;
            valueSpec.MaxWidth = std::max(rect.Width - 2.0f * (ChevronInset + iconSize), 1.0f);
            valueSpec.Wraps = false;
            const float width = std::min(MeasureText(text, valueSpec).X, valueSpec.MaxWidth);
            DrawLabel(drawList, rect, std::round(rect.GetCenter().X - width * 0.5f), text, valueSpec,
                      GetStyleColor(StyleColor::Label));
        }
    } // namespace

    bool EditNumber(std::string_view label, double& value, NumberKind kind, const NumberEditOptions& options)
    {
        CB_VERIFY(options.Min <= options.Max, "The range of a number control is empty: Min {} is greater than Max {}",
                  options.Min, options.Max);
        CB_VERIFY(options.Step >= 0.0, "The Step of a number control must not be negative; it is {}", options.Step);
        if (!(options.Min <= options.Max) || !(options.Step >= 0.0))
            return false;

        const ID id = GetID(label);
        NumberEditState& state = *GetState<NumberEditState>(id);
        bool changed = false;

        // The text follows the value, except while it holds what the user is typing.
        if (!state.HasText || (!state.IsDirty && value != state.ShownValue))
            ShowValue(state, value, options.Format);

        const bool isFocused = IsFocused(id) && !options.Disabled;
        if (options.Scrubs && !isFocused)
        {
            const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
            PushDisabled(options.Disabled);
            const Rect rect = AllocateItem(Vec2(80.0f, metrics.Height), {.Width = options.Width});
            // A press must not take the focus, which would turn the control into a text field mid-drag; it is a Tab
            // stop all the same, and arriving there starts typing.
            RegisterFocusable(id, rect);
            const DragInteraction drag = DragBehavior(id, rect, {.Focusable = false});
            if (drag.Hovered || drag.Active)
                SetCursor(Cursor::ResizeHorizontal);
            if (drag.Started)
            {
                state.DragValue = value;
                state.IsScrubbing = false;
            }
            if (drag.Active)
            {
                // Movement within the threshold is a click in the making; the drag counts from there.
                float moved = drag.Delta.X;
                if (!state.IsScrubbing && std::abs(drag.Total.X) >= ScrubThreshold)
                {
                    state.IsScrubbing = true;
                    moved = drag.Total.X - std::copysign(ScrubThreshold, drag.Total.X);
                }
                if (state.IsScrubbing && moved != 0.0f)
                {
                    const double speed = options.Step * GetStepScale();
                    state.DragValue = std::clamp(state.DragValue + double(moved) * speed, options.Min, options.Max);
                    double next = speed > 0.0 ? std::round(state.DragValue / speed) * speed : state.DragValue;
                    next = Normalize(next, kind, options);
                    if (next != value)
                    {
                        value = next;
                        changed = true;
                        ShowValue(state, value, options.Format);
                    }
                }
            }
            if (drag.Ended && !state.IsScrubbing && !IsDisabled())
            {
                // A click: type a value instead, starting with all of it selected.
                SetFocus(id);
                const size_t length = std::char_traits<char>::length(state.Text);
                SetTextFieldSelection(label, {.Caret = length, .Start = 0, .End = length});
            }

            DrawScrubField(id, rect, state.Text, drag, options.ControlSize);
            Interaction interaction;
            interaction.Hovered = drag.Hovered;
            interaction.Pressed = drag.Active;
            SetLastItem(id, rect, interaction);
            PopDisabled();
            state.WasFocused = false;
            return changed;
        }

        TextFieldOptions field;
        field.Width = options.Width;
        field.ControlSize = options.ControlSize;
        field.Disabled = options.Disabled;
        // The up and down arrows step the value.
        field.VerticalArrowsMoveCaret = false;
        if (TextField(label, std::span<char>(state.Text, TextCapacity), field))
            state.IsDirty = true;
        const bool isSubmitted = IsItemSubmitted();
        const bool isFocusedAfter = IsFocused(id);

        if (state.WasFocused && !isFocusedAfter && IsKeyPressed(Key::Escape, false))
        {
            // Escape left the field: the typing is discarded.
            ShowValue(state, value, options.Format);
        }
        else if (isSubmitted || (state.WasFocused && !isFocusedAfter))
        {
            // Enter, or the focus moved elsewhere: the typing is applied.
            changed = Commit(state, value, kind, options) || changed;
            if (isFocusedAfter)
                ReloadTextField(label);
        }

        if (isFocusedAfter && !options.Disabled)
        {
            int direction = 0;
            if (IsKeyPressed(Key::UpArrow))
                direction++;
            if (IsKeyPressed(Key::DownArrow))
                direction--;
            if (direction != 0)
            {
                // Steps go from what is typed when that is a number, so typing 5 and pressing up gives 6.
                double base = value;
                if (state.IsDirty)
                    ParseNumber(state.Text, options.Format, &base);
                const double next = Normalize(base + direction * options.Step * GetStepScale(), kind, options);
                changed = next != value || changed;
                value = next;
                ShowValue(state, value, options.Format);
                ReloadTextField(label);
            }
        }
        state.WasFocused = isFocusedAfter;
        return changed;
    }
} // namespace Carbon::Internal
