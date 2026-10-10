#include "Carbon/Extensions/ComboBox.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Carbon/Extensions/Internal/RowClipping.h"

namespace Carbon
{
    namespace
    {
        constexpr float ButtonInset = 2.0f;
        constexpr float ListPadding = 5.0f;
        constexpr float RowHeight = 22.0f;
        constexpr float RowPadding = 8.0f;
        constexpr float RowCornerRadius = 4.0f;
        constexpr int MaxVisibleRows = 8;

        // Remembered per combo box.
        struct ComboBoxState
        {
            /// The list item the keyboard or the pointer highlights, as an index into all items; -1 when none.
            int Highlight;
            /// The list shows all items (opened with the button or the arrow keys) instead of the matching ones.
            bool ShowsAll;
            /// The keyboard moved the highlight: scroll it into view.
            bool RevealHighlight;
        };

        char ToLower(char c)
        {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        }

        // Case-insensitive for ASCII letters; other characters must match exactly.
        bool Contains(std::string_view item, std::string_view text)
        {
            if (text.empty())
                return true;
            if (text.size() > item.size())
                return false;
            for (size_t start = 0; start + text.size() <= item.size(); start++)
            {
                size_t i = 0;
                while (i < text.size() && ToLower(item[start + i]) == ToLower(text[i]))
                    i++;
                if (i == text.size())
                    return true;
            }
            return false;
        }

        // The next item at or after `from` in `direction` that the list shows; -1 when there is none.
        int FindShown(std::span<const std::string_view> items, std::string_view text, bool showsAll, int from,
                      int direction)
        {
            for (int i = from; i >= 0 && i < static_cast<int>(items.size()); i += direction)
            {
                if (showsAll || Contains(items[static_cast<size_t>(i)], text))
                    return i;
            }
            return -1;
        }

        // The items a filtered list shows, as indices into all items. Finding them means looking at every item,
        // so they are found when the text or the items change and kept while the list stays open. One list is
        // open at a time, so one set of matches is enough; its storage is reused.
        struct Matches
        {
            ID Owner;
            uint64_t Key = 0;
            std::vector<int> Indices;
        };
        Matches s_Matches;

        // The items are told apart by where they are and how many: an application that rewrites an item's
        // characters in place sees the matches of before until the text changes.
        std::span<const int> GetMatches(ID id, std::span<const std::string_view> items, std::string_view text)
        {
            uint64_t key = HashBytes(text);
            key = HashCombine(key, reinterpret_cast<uintptr_t>(items.data()));
            key = HashCombine(key, items.size());
            if (s_Matches.Owner != id || s_Matches.Key != key)
            {
                s_Matches.Owner = id;
                s_Matches.Key = key;
                s_Matches.Indices.clear();
                for (size_t i = 0; i < items.size(); i++)
                {
                    if (Contains(items[i], text))
                        s_Matches.Indices.push_back(static_cast<int>(i));
                }
            }
            return s_Matches.Indices;
        }

        // The accent-colored button with a chevron, inside the field's trailing edge.
        void DrawButton(const Rect& button, const ControlMetrics& metrics, const ControlFeedback& feedback)
        {
            DrawList& drawList = GetDrawList();
            const Color accent = GetStyleColor(StyleColor::Accent);
            const Color fill = Blend(Blend(accent, Color::White(), 0.1f * feedback.Hover * (1.0f - feedback.Press)),
                                     Color::Black(), 0.18f * feedback.Press);
            drawList.AddSquircle(button, fill, std::max(metrics.CornerRadius - ButtonInset, 0.0f),
                                 GetStyleVar(StyleVar::CornerSmoothing));
            DrawIcon(drawList, button.GetCenter(), Icons::CaretDown, std::round(button.Height * 0.6f),
                     GetStyleColor(StyleColor::OnAccent), IconVariant::Bold);
        }
    } // namespace

    bool ComboBox(std::string_view label, std::string* text, std::span<const std::string_view> items,
                  const ComboBoxOptions& options)
    {
        CB_VERIFY(text != nullptr, "ComboBox needs a string to edit");
        if (text == nullptr)
            return false;

        const ID id = GetID(label);
        const ID list = HashID("##list", id);
        const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
        const float buttonSize = metrics.Height - ButtonInset * 2.0f;
        ComboBoxState& state = *GetState<ComboBoxState>(id);
        const int count = static_cast<int>(items.size());

        // Escape closes the list but keeps the field in editing; the field itself would give up focus.
        const bool wasFocused = IsFocused(id);
        const bool wasOpen = IsOverlayOpen(list);
        const bool closesWithEscape = wasOpen && wasFocused && IsKeyPressed(Key::Escape, false);

        TextFieldOptions field;
        field.Placeholder = options.Placeholder;
        field.Width = options.Width;
        field.ControlSize = options.ControlSize;
        field.Disabled = options.Disabled;
        field.MaxLength = options.MaxLength;
        field.TrailingInset = buttonSize + ButtonInset;
        field.VerticalArrowsMoveCaret = false;
        bool changed = TextField(label, text, field);
        const Rect rect = GetItemRect();
        const bool isSubmitted = IsItemSubmitted();
        const Interaction fieldInteraction{.Hovered = IsItemHovered(), .Focused = IsItemFocused()};
        if (closesWithEscape)
            SetFocus(id);
        const bool isFocused = IsFocused(id);

        // The button opens and closes the list with all items.
        PushDisabled(options.Disabled);
        const Rect button(rect.GetRight() - ButtonInset - buttonSize, rect.Y + ButtonInset, buttonSize, buttonSize);
        ButtonBehaviorOptions behavior;
        behavior.Focusable = false;
        behavior.ActivateOnPress = true;
        const Interaction buttonInteraction = ButtonBehavior(HashID("##button", id), button, behavior);
        bool isOpen = wasOpen;
        const auto openList = [&](bool showsAll)
        {
            if (!isOpen)
                OpenOverlay(list);
            isOpen = true;
            state.ShowsAll = showsAll;
        };
        const auto closeList = [&]()
        {
            if (isOpen)
                CloseOverlay(list);
            isOpen = false;
        };
        if (buttonInteraction.Clicked && !IsDisabled())
        {
            if (isOpen)
            {
                closeList();
            }
            else
            {
                openList(true);
                state.Highlight = FindShown(items, *text, true, 0, 1);
                for (int i = 0; i < count; i++)
                {
                    if (items[static_cast<size_t>(i)] == *text)
                        state.Highlight = i;
                }
                state.RevealHighlight = true;
            }
            SetFocus(id);
        }
        DrawButton(button, metrics,
                   AnimateFeedback(HashID("##button", id), buttonInteraction.Hovered, buttonInteraction.Pressed));
        PopDisabled();

        // Typing filters the list and opens it when something matches.
        if (changed && options.FiltersWhileTyping && isFocused)
        {
            state.ShowsAll = false;
            state.Highlight = text->empty() ? -1 : FindShown(items, *text, false, 0, 1);
            if (state.Highlight >= 0 && !text->empty())
                openList(false);
            else
                closeList();
        }

        bool picked = false;
        int pick = -1;
        if (isFocused && !IsDisabled())
        {
            if (IsKeyPressed(Key::DownArrow))
            {
                if (!isOpen)
                {
                    openList(true);
                    state.Highlight = FindShown(items, *text, true, 0, 1);
                }
                else
                {
                    const int next = FindShown(items, *text, state.ShowsAll, state.Highlight + 1, 1);
                    if (next >= 0)
                        state.Highlight = next;
                }
                state.RevealHighlight = true;
            }
            if (IsKeyPressed(Key::UpArrow) && isOpen)
            {
                const int previous = FindShown(items, *text, state.ShowsAll, state.Highlight - 1, -1);
                if (previous >= 0)
                    state.Highlight = previous;
                state.RevealHighlight = true;
            }
            if (isSubmitted && isOpen && state.Highlight >= 0 && state.Highlight < count)
                pick = state.Highlight;
        }
        // The list belongs to the field: it closes when the field loses focus (the button gives it focus first).
        if (closesWithEscape || (!IsFocused(id) && isOpen))
            closeList();

        // The list, below the field and as wide as it. It does not take the keyboard: typing goes on in the field.
        OverlayOptions overlay;
        overlay.Anchor = rect;
        overlay.Gap = 2.0f;
        overlay.DismissOnOutsideClick = false;
        overlay.DismissOnEscape = false;
        overlay.Padding = EdgeInsets(ListPadding);
        overlay.Spacing = 0.0f;
        overlay.Width = Size::Fixed(rect.Width);
        overlay.CornerRadius = 8.0f;
        if (BeginOverlay(list, overlay))
        {
            // The rows of the list: every item, or the items that match the text.
            const std::span<const int> matches = state.ShowsAll ? std::span<const int>() : GetMatches(id, items, *text);
            const int shown = state.ShowsAll ? count : static_cast<int>(matches.size());
            const float rowHeight = GetAdaptiveRowHeight(RowHeight);
            BeginScrollView(
                "##rows", {.Height = rowHeight * static_cast<float>(std::min(shown, MaxVisibleRows)), .Spacing = 0.0f});
            const Vec2 delta = GetMouseDelta();
            const bool hasPointerMoved = delta.X != 0.0f || delta.Y != 0.0f;
            const TextSpec spec = GetTextSpec(TextStyle::Body);
            if (state.RevealHighlight && state.Highlight >= 0)
            {
                // Keep the highlighted row inside the visible rows.
                int highlightRow = state.Highlight;
                if (!state.ShowsAll)
                {
                    const auto found = std::lower_bound(matches.begin(), matches.end(), state.Highlight);
                    highlightRow = found != matches.end() && *found == state.Highlight
                                       ? static_cast<int>(found - matches.begin())
                                       : -1;
                }
                if (highlightRow >= 0)
                {
                    Vec2 offset = GetScrollOffset("##rows");
                    const float top = rowHeight * static_cast<float>(highlightRow);
                    const float visible = rowHeight * static_cast<float>(MaxVisibleRows);
                    offset.Y = std::clamp(offset.Y, top + rowHeight - visible, top);
                    SetScrollOffset("##rows", offset, true);
                }
            }

            // Only the rows in view are built; the others take their space in two pieces, before and after.
            const RowRange visibleRows = Internal::GetVisibleRows(shown, rowHeight, 0.0f, GetDrawList().GetClipRect());
            Internal::ReserveRows(visibleRows.First, rowHeight, 0.0f);
            for (int row = visibleRows.First; row < visibleRows.End; row++)
            {
                const int i = state.ShowsAll ? row : matches[static_cast<size_t>(row)];
                const std::string_view item = items[static_cast<size_t>(i)];
                ItemOptions itemOptions;
                itemOptions.Width = Size::Fill();
                const Rect rowRect = AllocateItem(Vec2(0.0f, rowHeight), itemOptions);
                ButtonBehaviorOptions rowBehavior;
                rowBehavior.Focusable = false;
                const Interaction interaction = ButtonBehavior(HashID(i, list), rowRect, rowBehavior);
                if (interaction.Hovered && hasPointerMoved)
                    state.Highlight = i;
                if (interaction.Clicked)
                    pick = i;

                const bool isHighlighted = i == state.Highlight;
                if (isHighlighted)
                {
                    GetDrawList().AddSquircle(rowRect, GetStyleColor(StyleColor::Selection), RowCornerRadius,
                                              GetStyleVar(StyleVar::CornerSmoothing));
                }
                TextSpec rowSpec = spec;
                rowSpec.MaxWidth = std::max(rowRect.Width - RowPadding * 2.0f, 1.0f);
                rowSpec.Wraps = false;
                DrawLabel(GetDrawList(), rowRect, rowRect.X + RowPadding, item, rowSpec,
                          GetStyleColor(isHighlighted ? StyleColor::OnAccent : StyleColor::Label));
            }
            Internal::ReserveRows(shown - visibleRows.End, rowHeight, 0.0f);
            state.RevealHighlight = false;
            EndScrollView();
            EndOverlay();
        }

        if (pick >= 0 && pick < count)
        {
            text->assign(items[static_cast<size_t>(pick)]);
            ReloadTextField(label);
            CloseOverlay(list);
            SetFocus(id);
            picked = true;
        }

        // What follows the combo box sees the field, with Enter reported as submitted.
        Interaction summary = fieldInteraction;
        summary.Clicked = picked;
        SetLastItem(id, rect, summary);
        if (isSubmitted)
            SetItemSubmitted();
        return changed || picked;
    }

    bool ComboBox(std::string_view label, std::string* text, std::initializer_list<std::string_view> items,
                  const ComboBoxOptions& options)
    {
        return ComboBox(label, text, std::span<const std::string_view>(items.begin(), items.size()), options);
    }
} // namespace Carbon
