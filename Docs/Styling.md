# Styling

> This guide grows with the framework. Themes, typography, icons and corner shapes exist today; the style stack
> and per-call options arrive with milestone 4.

## Themes

A `Theme` is the complete look of the interface: semantic colors, metrics and the type ramp. Carbon ships two.

| | `Theme::Light()` | `Theme::Dark()` |
| --- | --- | --- |
| `Background` | `#FFFFFF` | `#000000` (pure black, for OLED) |
| `SecondaryBackground` (grouped boxes, sidebars) | `#F2F2F7` | `#1C1C1E` |
| `Label` | `#000000` | `#FFFFFF` |
| `SecondaryLabel` | `#6E6E73` | `#98989F` |
| `TertiaryLabel` (placeholders, disabled) | `#AEAEB2` | `#636366` |
| `Separator` | `#DCDCE0` | `#38383A` |
| `ControlFill` (buttons, tracks) | `#E9E9EB` | `#2C2C2E` |
| `Accent` (system blue) | `#007AFF` | `#0A84FF` |
| `Destructive` (system red) | `#FF3B30` | `#FF453A` |

Surfaces are opaque: there is no translucency or blur. Primary and secondary text meet the HIG's 4.5 : 1 contrast
minimum on the background colors. `StyleColor` also contains the control, selection, overlay and focus-ring
roles and the system palette (`Red` … `Gray`) for charts and status colors; see `Carbon/Style/StyleColor.h`.

``cpp
Carbon::SetTheme(Carbon::Theme::Dark());             // the host chooses; Carbon cannot detect the OS setting

Carbon::Theme theme = Carbon::Theme::Dark();          // customize: start from a built-in theme
theme.SetColor(Carbon::StyleColor::Accent, Carbon::Color::FromHex(0xFF9F0A));
theme.SetVar(Carbon::StyleVar::CornerRadius, 8.0f);
Carbon::SetTheme(theme);
``

Switching themes is animated: every color and metric glides to its new value (see
[Animation](Animation.md#theme-switches)). Components read the current values each frame:

``cpp
Carbon::Color label = Carbon::GetStyleColor(Carbon::StyleColor::Label);
float radius = Carbon::GetStyleVar(Carbon::StyleVar::CornerRadius);
``

### Metrics

| `StyleVar` | Default | Meaning |
| --- | --- | --- |
| `CornerRadius` | 6 | Controls: buttons, fields, selection highlights |
| `CornerSmoothing` | 0.6 | Smoothing of every rounded shape |
| `GroupCornerRadius` | 10 | Grouped boxes and cards |
| `OverlayCornerRadius` | 10 | Menus, popovers, alerts, sheets |
| `Spacing` | 8 | Default distance between the items of a stack |
| `ControlHeight` | 24 | Height of regular-size controls |
| `ControlPadding` | 10 | Horizontal padding inside buttons and fields |
| `BorderWidth` | 1 | Control outlines and separators |
| `FocusRingWidth` / `FocusRingOffset` | 3 / 1 | The keyboard focus ring |
| `DisabledOpacity` | 0.4 | Opacity of disabled content |
| `HoverAmount` / `PressedAmount` | 0.05 / 0.12 | How far a control's fill shifts towards the label color |
| `ScrollIndicatorWidth` | 7 | Overlay scroll indicators |

## Typography

Carbon's default typeface is [Public Sans](https://public-sans.digital.gov), embedded as two variable fonts
(upright and italic) with a continuous weight axis from 100 to 900.

### Text styles

`Carbon::TextStyle` is the macOS type ramp from the
[HIG Typography page](https://developer.apple.com/design/human-interface-guidelines/typography). Sizes and line
heights are in points. The ramp is part of the theme: `Theme::SetTextStyle` changes a style and `Theme::Font`
replaces the typeface.

| Style | Size / line height | Weight | Emphasized weight |
| --- | --- | --- | --- |
| `LargeTitle` | 26 / 32 | Regular | Bold |
| `Title1` | 22 / 26 | Regular | Bold |
| `Title2` | 17 / 22 | Regular | Bold |
| `Title3` | 15 / 20 | Regular | Semibold |
| `Headline` | 13 / 16 | Bold | Heavy |
| `Body` | 13 / 16 | Regular | Semibold |
| `Callout` | 12 / 15 | Regular | Semibold |
| `Subheadline` | 11 / 14 | Regular | Semibold |
| `Footnote` | 10 / 13 | Regular | Semibold |
| `Caption1` | 10 / 13 | Regular | Medium |
| `Caption2` | 10 / 13 | Medium | Semibold |

```cpp
Carbon::TextSpec title = Carbon::GetTextSpec(Carbon::TextStyle::Title1, /* emphasized */ true);
Carbon::Vec2 size = Carbon::MeasureText("Settings", title);
Carbon::GetDrawList().AddText(Carbon::Vec2(20, 20), "Settings", title, Carbon::Color::Black());
```

A `TextSpec` can also be filled in by hand: font, size, weight (any value on the 100–900 axis, for example
`Carbon::FontWeight(450)`), italic, line height, tracking and the icon variant.

### Public Sans compared with SF Pro

The HIG's sizes are specified for SF Pro. They are used unchanged for Public Sans because the two faces read the
same size:

| Metric (fraction of the em) | SF Pro Text (approximate) | Public Sans |
| --- | --- | --- |
| x-height | 0.519 | 0.517 |
| Cap height | 0.705 | 0.723 |

The Public Sans values are read from the embedded font; the SF Pro values are its commonly published metrics.
The x-height, which determines how large running text looks, is identical to within half a percent. Capitals are
2.5 % taller, which is below one pixel at every size in the ramp on a standard display. Public Sans is slightly
wider than SF Pro; allow a little more room for labels than a macOS mock-up suggests.

### How text is rendered

- Text is shaped with HarfBuzz (kerning and ligatures on) and rasterized with FreeType at
  `size × content scale` pixels, so it is as sharp as the display allows.
- Glyphs are not hinted, like on macOS: letter shapes and spacing stay faithful to the design at every size.
- Each glyph is placed on whole pixels vertically and at quarter-pixel steps horizontally, which keeps spacing
  even at small sizes without blurring stems.
- Lines are separated by `'\n'`. The glyphs are centered vertically in the line height.

## Icons

The [Phosphor](https://phosphoricons.com) icon set (1530 icons) is embedded in three weights: regular, bold and
fill. Every icon is a constant in `Carbon::Icons`, named after the Phosphor icon in PascalCase:

```cpp
#include <Carbon/Text/Icons.h>

std::string label = std::string(Carbon::Icons::Folder) + "  Documents";
drawList.AddText(position, label, Carbon::GetTextSpec(Carbon::TextStyle::Body), color);
```

Each constant is a short UTF-8 string, so icons can stand alone or sit inside any text. They are drawn through
the text pipeline: an icon is 1.2 × the text size and centered on the capital letters of the surrounding text.

- **Weight follows the text.** Text that is semibold or heavier uses the bold icons; lighter text uses the
  regular ones.
- **Choosing explicitly.** Set `TextSpec::Icons` to `IconVariant::Regular`, `Bold` or `Fill` to override that.
- `Carbon::Icons::All` lists every icon with its name, for pickers and galleries. The constants are generated
  from Phosphor's stylesheet at build time.

## Corners

Every rounded shape in Carbon has continuous-curvature corners, like Apple's, instead of plain circular arcs.
Two parameters describe a corner:

- **Corner radius**, in points. It is clamped to half of the shape's shorter side.
- **Corner smoothing**, from 0 to 1. At 0 the corner is a circular arc. Higher values start the curve earlier
  along the edge and let the curvature build up gradually. The default, `Carbon::DefaultCornerSmoothing` (0.6),
  matches the look of macOS and iOS.

```cpp
drawList.AddSquircle(rect, color, 12.0f);          // default smoothing
drawList.AddSquircle(rect, color, 12.0f, 0.0f);    // circular arcs
drawList.AddSquircleStroke(rect, color, 12.0f, 1.0f);
drawList.AddFocusRing(rect, accent, 12.0f, 3.0f, 1.0f);   // concentric ring outside the shape
```

When the radius reaches half of the shorter side there is no room left to smooth, so pills and circles are exact
circular arcs — as Apple's capsules are. The shape function is public (`Carbon/Draw/Squircle.h`) for hit testing
in custom components; [Architecture](Architecture.md#squircle-shape-function) describes the math.
