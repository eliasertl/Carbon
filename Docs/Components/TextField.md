# TextField

Edits a single line of text.
HIG: [Text fields](https://developer.apple.com/design/human-interface-guidelines/text-fields)

![Text fields: plain, with a placeholder, secure and a search field](../Images/Components/TextField.png)

```cpp
std::string name;
Carbon::TextField("Name", &name);                                   // "Name" is shown as the placeholder

Carbon::TextField("Email", &email, { .Placeholder = "name@example.com", .Width = 240.0f });
Carbon::TextField("Password", &password, { .IsSecure = true });

Carbon::TextField("Search", &query, { .Icon = Carbon::Icons::MagnifyingGlass, .ShowsClearButton = true });
if (Carbon::IsItemSubmitted())
    RunSearch(query);
```

`TextField` edits the `std::string` you pass (UTF-8) and returns `true` on frames the text changed. The label
identifies the field and serves as its placeholder; put a `Text` next to the field for a visible title.

## Options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Placeholder` | `std::string_view` | the label | Shown in a faint color while the field is empty |
| `Icon` | `std::string_view` | none | An icon from `Carbon::Icons` at the leading edge |
| `ShowsClearButton` | `bool` | `false` | A button at the trailing edge that clears the text while the field is not empty |
| `Width` | `Size` | 180 points | A field cannot fit its content, so the default is a fixed width |
| `ControlSize` | `ControlSize` | `Regular` | |
| `Disabled` | `bool` | `false` | |
| `IsSecure` | `bool` | `false` | Shows bullets and keeps the text off the clipboard |
| `MaxLength` | `size_t` | 0 | Longest text in characters; 0 is unlimited |
| `TrailingInset` | `float` | 0 | Points kept free at the trailing edge for an accessory you draw there, such as the button of a [ComboBox](ComboBox.md) |
| `VerticalArrowsMoveCaret` | `bool` | `true` | Up and down move the caret to the start and end. Turn it off when your component uses those keys. |
| `IsBezeled` | `bool` | `true` | Background, border and focus ring. Turn it off when your component draws the field around the text, as a [TokenField](TokenField.md) does |
| `AcceptsInput` | `bool` | `true` | While `false` the field keeps its focus but leaves keys, typing and clicks alone and hides its caret: for frames in which your component handles the keys itself |

When you replace the text of a field while it is being edited, call `ReloadTextField(label)` at the same ID
scope: the caret then moves to the end of the new text and undo starts over.

`GetTextFieldSelection(label, &selection)` tells where the caret and the selection of the field `label` are, as
byte offsets into its text (`Caret`, `Start`, `End`), and returns `false` while the field is not being edited.
Components built around a text field use it to act on keys at the ends of the text, such as Backspace with the
caret at the start.

## Mouse

| Action | Effect |
| --- | --- |
| Click | Focus the field and place the caret |
| Shift+click | Extend the selection to the click |
| Drag | Select |
| Double click | Select the word |
| Triple click | Select everything |

The pointer becomes an I-beam over the field (through the host's `SetCursor` callback), and the field's border
gets stronger.

## Keyboard

| Key | Effect |
| --- | --- |
| Tab / Shift+Tab | Focus the field and select all of its text |
| Left / Right | Move the caret; with Shift, extend the selection |
| Ctrl+Left / Ctrl+Right (or Alt) | Move by word |
| Home / End, Up / Down | Start / end of the text |
| Backspace / Delete | Delete the selection, or the character before / after the caret; with Ctrl, a word |
| Ctrl+A | Select all |
| Ctrl+C, Ctrl+X, Ctrl+V | Copy, cut, paste through the host's clipboard callbacks |
| Ctrl+Z, Ctrl+Shift+Z or Ctrl+Y | Undo, redo. A run of typing is one step. |
| Enter | Submit: `IsItemSubmitted()` is true for this frame. The text and the focus stay. |
| Escape | Give up focus |

Ctrl is the default shortcut modifier; see [Keyboard navigation](../KeyboardNavigation.md).

## Notes

- Text that is longer than the field scrolls to keep the caret visible.
- Pasted line breaks and tabs become spaces; other control characters are dropped.
- The focus ring shows whenever the field has focus, as on macOS.
- The application may change the string at any time, also while the field is focused.
- Input method composition (IME) and right-to-left text are not supported in this version.
- While a field is focused, `io.WantsTextInput()` is true and the caret blinks. The caret does not keep
  `IsAnimating()` true: it asks for a frame each time it changes, twice a second, which a host that renders on
  demand learns from `GetNextFrameDelay()` (see [Animation](../Animation.md#rendering-on-demand)).
