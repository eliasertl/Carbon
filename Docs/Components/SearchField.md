# SearchField

A text field for search terms.
HIG: [Search fields](https://developer.apple.com/design/human-interface-guidelines/search-fields)
Library: CarbonExtensions, `#include <Carbon/Extensions/SearchField.h>`

```cpp
std::string query;
if (Carbon::SearchField("Search", &query))
    UpdateResults(query);                 // search as the user types
if (Carbon::IsItemSubmitted())
    RunFullSearch(query);                 // or when Enter is pressed
```

It is a [TextField](TextField.md) with a magnifying glass at the leading edge and a clear button at the
trailing edge once there is text. It returns `true` on the frame the text changed. The label identifies the
field and is not drawn.

## Options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Placeholder` | `std::string_view` | `"Search"` | Shown while the field is empty |
| `Width` | `Size` | 180 | |
| `ControlSize` | `ControlSize` | `Regular` | |
| `Disabled` | `bool` | `false` | |

## Keyboard

Everything a [TextField](TextField.md#keyboard) does, plus:

| Key | Effect |
| --- | --- |
| Escape | Clears the text and gives up focus |
| Enter | Submits; `IsItemSubmitted()` is `true` for that frame |

## Guidance from the HIG

- Describe what can be searched in the placeholder when it is not obvious: "Search Mail".
- Start searching while the user types when results come fast enough; otherwise search on Enter.
