#include "Carbon/Extensions/DatePicker.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>

#include "Carbon/Extensions/DatePickerCalendar.h"
#include "Carbon/Extensions/Stepper.h"

namespace Carbon
{
    namespace
    {
        enum class Element : uint8_t
        {
            Year,
            Month,
            Day,
            Hour,
            Minute,
            Period
        };

        // The field as a sequence of elements and the literal text between them.
        constexpr int MaxParts = 12;
        struct Part
        {
            bool IsElement = false;
            Element Kind = Element::Year;
            std::string_view Literal;
        };

        struct FieldLayout
        {
            Part Parts[MaxParts];
            int PartCount = 0;
            /// The index into Parts of each element, in order.
            int Elements[6] = {};
            int ElementCount = 0;

            void AddElement(Element kind)
            {
                Elements[ElementCount++] = PartCount;
                Parts[PartCount++] = Part{true, kind, {}};
            }

            void AddLiteral(std::string_view text) { Parts[PartCount++] = Part{false, Element::Year, text}; }

            Element GetElement(int index) const { return Parts[Elements[index]].Kind; }
        };

        FieldLayout BuildLayout(DatePickerElements elements, const DateFormat& format)
        {
            FieldLayout layout;
            const std::string_view dateSeparator(&format.DateSeparator, 1);
            if (elements != DatePickerElements::Time)
            {
                Element order[3] = {Element::Year, Element::Month, Element::Day};
                if (format.Order == DateOrder::DayMonthYear)
                {
                    order[0] = Element::Day;
                    order[2] = Element::Year;
                }
                else if (format.Order == DateOrder::MonthDayYear)
                {
                    order[0] = Element::Month;
                    order[1] = Element::Day;
                    order[2] = Element::Year;
                }
                for (int i = 0; i < 3; i++)
                {
                    if (i > 0)
                        layout.AddLiteral(dateSeparator);
                    layout.AddElement(order[i]);
                }
            }
            if (elements == DatePickerElements::DateAndTime)
                layout.AddLiteral(format.DateTimeSeparator);
            if (elements != DatePickerElements::Date)
            {
                layout.AddElement(Element::Hour);
                layout.AddLiteral(std::string_view(&format.TimeSeparator, 1));
                layout.AddElement(Element::Minute);
                if (!format.Uses24HourClock)
                {
                    layout.AddLiteral(" ");
                    layout.AddElement(Element::Period);
                }
            }
            return layout;
        }

        bool IsDateElement(Element element)
        {
            return element == Element::Year || element == Element::Month || element == Element::Day;
        }

        // Writes `number` with at least `digits` digits.
        std::string_view FormatNumber(char (&buffer)[8], int number, int digits)
        {
            char* start = buffer;
            if (number < 0)
            {
                *start++ = '-';
                number = -number;
            }
            for (int power = 10, i = 1; i < digits; i++, power *= 10)
            {
                if (number < power)
                    *start++ = '0';
            }
            const char* end = std::to_chars(start, buffer + sizeof(buffer), number).ptr;
            return std::string_view(buffer, static_cast<size_t>(end - buffer));
        }

        std::string_view FormatElement(char (&buffer)[8], Element element, const DateTime& value,
                                       const DateFormat& format)
        {
            switch (element)
            {
                case Element::Year:
                    return FormatNumber(buffer, value.Year, 4);
                case Element::Month:
                    return FormatNumber(buffer, value.Month, format.PadsMonth ? 2 : 1);
                case Element::Day:
                    return FormatNumber(buffer, value.Day, format.PadsDay ? 2 : 1);
                case Element::Hour:
                {
                    const int hour =
                        format.Uses24HourClock ? value.Hour : (value.Hour % 12 == 0 ? 12 : value.Hour % 12);
                    return FormatNumber(buffer, hour, format.PadsHour ? 2 : 1);
                }
                case Element::Minute:
                    return FormatNumber(buffer, value.Minute, 2);
                case Element::Period:
                    return value.Hour < 12 ? "AM" : "PM";
            }
            return {};
        }

        int GetDigitCount(Element element)
        {
            return element == Element::Year ? 4 : (element == Element::Period ? 0 : 2);
        }

        // The largest first digit of a two-digit value; a larger one is the whole value ("5" for May).
        int GetLargestFirstDigit(Element element, const DateFormat& format)
        {
            switch (element)
            {
                case Element::Month:
                    return 1;
                case Element::Day:
                    return 3;
                case Element::Hour:
                    return format.Uses24HourClock ? 2 : 1;
                case Element::Minute:
                    return 5;
                default:
                    return 9;
            }
        }

        int Wrap(int value, int count)
        {
            return ((value % count) + count) % count;
        }

        DateTime Finish(const DateTime& value, const DatePickerOptions& options)
        {
            return ClampDate(MakeValidDate(value), options.MinDate, options.MaxDate);
        }

        // One step of an element up or down, as the arrow keys and the stepper do. Elements wrap within their
        // range without carrying into the next (the day after the 31st is the 1st of the same month), as on macOS.
        DateTime StepElement(DateTime value, Element element, int delta, int interval, const DatePickerOptions& options)
        {
            switch (element)
            {
                case Element::Year:
                    value.Year += delta;
                    break;
                case Element::Month:
                    value.Month = Wrap(value.Month - 1 + delta, 12) + 1;
                    break;
                case Element::Day:
                    value.Day = Wrap(value.Day - 1 + delta, GetDaysInMonth(value.Year, value.Month)) + 1;
                    break;
                case Element::Hour:
                    value.Hour = Wrap(value.Hour + delta, 24);
                    break;
                case Element::Minute:
                    value.Minute = Wrap(value.Minute / interval * interval + delta * interval, 60);
                    break;
                case Element::Period:
                    value.Hour = Wrap(value.Hour + 12, 24);
                    break;
            }
            return Finish(value, options);
        }

        // Puts a typed number into an element.
        DateTime ApplyTyped(DateTime value, Element element, int typed, int digits, int interval,
                            const DatePickerOptions& options)
        {
            switch (element)
            {
                case Element::Year:
                    // Two digits mean this century, as on macOS: "26" is 2026.
                    value.Year = digits <= 2 ? 2000 + typed : typed;
                    break;
                case Element::Month:
                    if (typed >= 1)
                        value.Month = std::min(typed, 12);
                    break;
                case Element::Day:
                    if (typed >= 1)
                        value.Day = std::min(typed, GetDaysInMonth(value.Year, value.Month));
                    break;
                case Element::Hour:
                    if (options.Format.Uses24HourClock)
                        value.Hour = std::min(typed, 23);
                    else if (typed >= 1)
                        value.Hour = std::min(typed, 12) % 12 + (value.Hour >= 12 ? 12 : 0);
                    break;
                case Element::Minute:
                {
                    const int minute = std::min(typed, 59);
                    value.Minute = minute - minute % interval;
                    break;
                }
                case Element::Period:
                    break;
            }
            return Finish(value, options);
        }

        // The width of a separator, spaces included: text measurement leaves out trailing spaces, so the
        // literal is measured between two bars.
        float MeasureLiteral(std::string_view text, const TextSpec& spec)
        {
            char buffer[24] = "|";
            const size_t length = std::min(text.size(), sizeof(buffer) - 3);
            std::memcpy(buffer + 1, text.data(), length);
            buffer[length + 1] = '|';
            return MeasureText(std::string_view(buffer, length + 2), spec).X - MeasureText("||", spec).X;
        }

        // Remembered per picker.
        struct DatePickerState
        {
            /// The element the keys act on.
            int Selected;
            /// Digits typed into it that are not applied yet.
            int Typed;
            int TypedDigits;
            /// The selection moved on by itself because the element was complete. A separator typed next only
            /// confirms that move, as on macOS.
            bool HasAdvanced;
            bool WasFocused;
            /// The popover was open during the last frame. Focus requests take effect a frame late, so a popover
            /// that has just been opened is not closed for the field's missing focus.
            bool WasPopoverOpen;
            /// The calendar in the popover and the popover's area, from the last frame.
            ID Calendar;
            Rect Popover;
        };

        constexpr float TextInset = 7.0f;
        constexpr float StepperGap = 4.0f;
        constexpr float PopoverPadding = 12.0f;
    } // namespace

    std::string_view FormatDateTime(const DateTime& value, DatePickerElements elements, const DateFormat& format,
                                    std::span<char> buffer)
    {
        const FieldLayout layout = BuildLayout(elements, format);
        size_t length = 0;
        for (int i = 0; i < layout.PartCount; i++)
        {
            char element[8];
            const Part& part = layout.Parts[i];
            const std::string_view text =
                part.IsElement ? FormatElement(element, part.Kind, value, format) : part.Literal;
            const size_t count = std::min(text.size(), buffer.size() - length);
            std::memcpy(buffer.data() + length, text.data(), count);
            length += count;
        }
        return std::string_view(buffer.data(), length);
    }

    bool DatePicker(std::string_view label, DateTime* value, const DatePickerOptions& options)
    {
        CB_VERIFY(value != nullptr, "DatePicker needs a value to bind to");
        if (value == nullptr)
            return false;
        CB_VERIFY(options.MinuteInterval >= 1 && 60 % options.MinuteInterval == 0,
                  "DatePicker's MinuteInterval must divide 60, not {}", options.MinuteInterval);
        const int interval =
            options.MinuteInterval >= 1 && 60 % options.MinuteInterval == 0 ? options.MinuteInterval : 1;

        const ID id = GetID(label);
        const ID popover = HashID("##popover", id);
        const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
        const TextSpec spec = GetTextSpec(metrics.Style);
        const FieldLayout layout = BuildLayout(options.Elements, options.Format);
        const bool hasDate = options.Elements != DatePickerElements::Time;
        DatePickerState& state = *GetState<DatePickerState>(id);
        state.Selected = std::clamp(state.Selected, 0, layout.ElementCount - 1);

        // An invalid or out-of-range value is corrected without reporting a change.
        DateTime current = Finish(*value, options);
        *value = current;
        const DateTime before = current;

        // The field is as wide as its widest possible text, so it keeps its width while values change.
        float digitWidth = 0.0f;
        for (char digit = '0'; digit <= '9'; digit++)
            digitWidth = std::max(digitWidth, MeasureText(std::string_view(&digit, 1), spec).X);
        float fitWidth = TextInset * 2.0f;
        for (int i = 0; i < layout.PartCount; i++)
        {
            const Part& part = layout.Parts[i];
            if (!part.IsElement)
                fitWidth += MeasureLiteral(part.Literal, spec);
            else if (part.Kind == Element::Period)
                fitWidth += std::max(MeasureText("AM", spec).X, MeasureText("PM", spec).X);
            else
                fitWidth += digitWidth * static_cast<float>(GetDigitCount(part.Kind));
        }

        PushDisabled(options.Disabled);
        HStackOptions row;
        row.Spacing = StepperGap;
        row.ID = label;
        BeginHStack(row);
        const Rect rect = AllocateItem(Vec2(std::ceil(fitWidth), metrics.Height));
        ButtonBehaviorOptions behavior;
        behavior.ActivateOnPress = true;
        const Interaction interaction = ButtonBehavior(id, rect, behavior);
        const bool isFocused = IsFocused(id);

        // Where each element is drawn, for clicks: texts packed from the leading edge.
        const auto getText = [&](char (&buffer)[8], int index, const DateTime& date)
        {
            if (isFocused && index == state.Selected && state.TypedDigits > 0)
                return FormatNumber(buffer, state.Typed, 1);
            return FormatElement(buffer, layout.GetElement(index), date, options.Format);
        };
        const auto commitTyped = [&]
        {
            if (state.TypedDigits > 0)
                current = ApplyTyped(current, layout.GetElement(state.Selected), state.Typed, state.TypedDigits,
                                     interval, options);
            state.Typed = 0;
            state.TypedDigits = 0;
        };
        const auto select = [&](int index)
        {
            state.HasAdvanced = false;
            commitTyped();
            state.Selected = std::clamp(index, 0, layout.ElementCount - 1);
        };

        bool isSubmitted = false;
        if (interaction.Clicked && IsMousePressed())
        {
            // The element nearest to the click.
            int nearest = 0;
            float distance = 1e9f;
            float x = rect.X + TextInset;
            for (int i = 0, element = 0; i < layout.PartCount; i++)
            {
                char buffer[8];
                const Part& part = layout.Parts[i];
                const float width = part.IsElement ? MeasureText(getText(buffer, element, current), spec).X
                                                   : MeasureLiteral(part.Literal, spec);
                if (part.IsElement)
                {
                    const float d = std::abs(GetMousePos().X - (x + width * 0.5f));
                    if (d < distance)
                    {
                        distance = d;
                        nearest = element;
                    }
                    element++;
                }
                x += width;
            }
            select(nearest);
            SetFocus(id);
            if (hasDate && IsDateElement(layout.GetElement(nearest)) && !IsOverlayOpen(popover))
                OpenOverlay(popover);
        }
        else if (interaction.Clicked)
        {
            // Space and Enter reach the field as activation.
            if (IsKeyPressed(Key::Space, false) && hasDate)
            {
                if (IsOverlayOpen(popover))
                    CloseOverlay(popover);
                else
                    OpenOverlay(popover);
            }
            else if (!IsKeyPressed(Key::Space, false))
            {
                commitTyped();
                CloseOverlay(popover);
                isSubmitted = true;
            }
        }

        if (isFocused && !state.WasFocused && IsFocusVisible(id))
            state.Selected = 0; // arriving with Tab starts at the first element, as on macOS
        if (isFocused && !IsDisabled())
        {
            if (IsKeyPressed(Key::LeftArrow))
                select(state.Selected - 1);
            if (IsKeyPressed(Key::RightArrow))
                select(state.Selected + 1);
            const Element element = layout.GetElement(state.Selected);
            if (IsKeyPressed(Key::UpArrow) || IsKeyPressed(Key::DownArrow))
            {
                commitTyped();
                current = StepElement(current, element, IsKeyPressed(Key::UpArrow) ? 1 : -1, interval, options);
            }
            if (IsKeyPressed(Key::Backspace))
            {
                state.Typed = 0;
                state.TypedDigits = 0;
            }
            if (IsKeyPressed(Key::Escape, false))
            {
                state.Typed = 0;
                state.TypedDigits = 0;
                CloseOverlay(popover);
            }
            for (const char32_t character : GetInputCharacters())
            {
                const Element selected = layout.GetElement(state.Selected);
                if (character >= U'0' && character <= U'9' && selected != Element::Period)
                {
                    const int digit = static_cast<int>(character - U'0');
                    state.Typed = state.Typed * 10 + digit;
                    state.TypedDigits++;
                    const bool isComplete =
                        state.TypedDigits >= GetDigitCount(selected) ||
                        (state.TypedDigits == 1 && digit > GetLargestFirstDigit(selected, options.Format));
                    state.HasAdvanced = false;
                    if (isComplete)
                    {
                        select(state.Selected + 1 < layout.ElementCount ? state.Selected + 1 : state.Selected);
                        state.HasAdvanced = true;
                    }
                }
                else if ((character == U'a' || character == U'A' || character == U'p' || character == U'P') &&
                         !options.Format.Uses24HourClock && options.Elements != DatePickerElements::Date)
                {
                    commitTyped();
                    const bool isPM = character == U'p' || character == U'P';
                    if ((current.Hour >= 12) != isPM)
                        current = StepElement(current, Element::Period, 1, interval, options);
                }
                else if (character == U'-' || character == U'.' || character == U'/' || character == U':' ||
                         character == U',')
                {
                    if (!state.HasAdvanced)
                        select(state.Selected + 1);
                    state.HasAdvanced = false;
                }
            }
        }
        if (!isFocused)
            commitTyped(); // leaving the field applies what was typed

        // The stepper changes the selected element; it is bound to a scratch value that it moves by one.
        if (options.ShowsStepper)
        {
            int delta = 0;
            PushID(id);
            StepperOptions stepper;
            stepper.Min = -1.0;
            stepper.Max = 1.0;
            stepper.ControlSize = options.ControlSize;
            if (Stepper("##stepper", &delta, stepper) && delta != 0)
            {
                commitTyped();
                current = StepElement(current, layout.GetElement(state.Selected), delta, interval, options);
            }
            PopID();
        }
        EndHStack();

        // The calendar, below the field. Like a combo box's list it leaves the keyboard in the field: typing goes
        // on, and the calendar follows.
        if (hasDate)
        {
            const bool isCalendarFocused = state.Calendar.IsValid() && IsFocused(state.Calendar);
            const Vec2 mouse = GetMousePos();
            const bool isOutsideClick = IsMousePressed() && !rect.Contains(mouse) && !state.Popover.Contains(mouse);
            const bool hasLostFocus = state.WasPopoverOpen && !isFocused && !isCalendarFocused;
            if (IsOverlayOpen(popover) && (hasLostFocus || isOutsideClick))
                CloseOverlay(popover);

            OverlayOptions overlay;
            overlay.Anchor = rect;
            overlay.Gap = 2.0f;
            overlay.ShowsArrow = true;
            overlay.DismissOnOutsideClick = false;
            overlay.DismissOnEscape = false;
            overlay.Padding = PopoverPadding;
            if (BeginOverlay(popover, overlay))
            {
                state.Calendar = GetID("##calendar");
                DateTime picked = current;
                DatePickerCalendarOptions calendar;
                calendar.FirstWeekday = options.FirstWeekday;
                calendar.MinDate = options.MinDate;
                calendar.MaxDate = options.MaxDate;
                calendar.Today = options.Today;
                if (DatePickerCalendar("##calendar", &picked, calendar))
                {
                    commitTyped();
                    current = Finish(picked, options);
                    // A day picked with the mouse is the answer: the popover closes and the field keeps the focus.
                    if (IsMouseReleased())
                    {
                        CloseOverlay(popover);
                        SetFocus(id);
                    }
                }
                const Rect content = GetContentRect();
                state.Popover = Rect(content.X - PopoverPadding, content.Y - PopoverPadding * 2.0f,
                                     content.Width + PopoverPadding * 2.0f, content.Height + PopoverPadding * 3.0f);
                EndOverlay();
            }
            else
            {
                state.Popover = Rect();
            }
        }

        const bool changed = current != before;
        if (changed)
            *value = current;

        // The field: the bezel of a text field, the elements with the selected one highlighted while focused.
        DrawList& drawList = GetDrawList();
        const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
        drawList.AddSquircle(rect, GetStyleColor(StyleColor::ControlBackground), metrics.CornerRadius, smoothing);
        drawList.AddSquircleStroke(rect, GetStyleColor(StyleColor::ControlBorder), metrics.CornerRadius,
                                   GetStyleVar(StyleVar::BorderWidth), smoothing);
        const Color labelColor = GetStyleColor(StyleColor::Label);
        const bool showsSelection = IsFocused(id);
        float x = rect.X + TextInset;
        for (int i = 0, element = 0; i < layout.PartCount; i++)
        {
            char buffer[8];
            const Part& part = layout.Parts[i];
            const std::string_view text = part.IsElement ? getText(buffer, element, current) : part.Literal;
            const float width = part.IsElement ? MeasureText(text, spec).X : MeasureLiteral(text, spec);
            Color color = labelColor;
            if (part.IsElement && showsSelection && element == state.Selected)
            {
                drawList.AddSquircle(Rect(x - 1.0f, rect.Y + 3.0f, width + 2.0f, rect.Height - 6.0f),
                                     GetStyleColor(StyleColor::Selection), 3.0f, smoothing);
                color = GetStyleColor(StyleColor::OnAccent);
            }
            DrawLabel(drawList, rect, x, text, spec, color);
            x += width;
            if (part.IsElement)
                element++;
        }
        DrawFocusRing(id, rect, metrics.CornerRadius, true);

        Interaction summary = interaction;
        summary.Clicked = changed;
        SetLastItem(id, rect, summary);
        if (isSubmitted)
            SetItemSubmitted();
        state.WasFocused = isFocused;
        state.WasPopoverOpen = IsOverlayOpen(popover);
        PopDisabled();
        return changed;
    }
} // namespace Carbon
