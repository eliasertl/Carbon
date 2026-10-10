# Changelog

Carbon follows [semantic versioning](https://semver.org): from 1.0 on, the API changes incompatibly only with a
new major version. The numbers in parentheses are entries of the decision log in
[Architecture.md](Docs/Architecture.md#16-decision-log).

## 1.0.0 (2026-10-10)

The first stable release. Compared with 0.1 (2026-10-06, never tagged):

### Added

- `TextField` bound to a caller-owned fixed buffer (`std::span<char>`) or to a callback
  (`Carbon::FunctionRef`, a new non-owning callable reference), besides `std::string` (129).
- `SetTextFieldSelection` to select text in a field that is about to be edited (132).
- `TextArea`, a multi-line plain-text editor with wrapping, scrolling and the three text forms of `TextField`
  (134).
- `NumberField` and `ScrubField` for `int`, `float` and `double`, with `NumberFormat`, `FormatNumber` and
  `ParseNumber` (132).
- `Slider` for `int` and `double`, vertical sliders (`SliderOptions::Axis`, `Height`) and a logarithmic scale
  (`SliderOptions::Scale`) (133).
- A warning through the log callback when one call site begins several containers in the same scope, and
  `GridRowOptions::ID` to tell them apart (130).
- Input method composition (Japanese, Chinese, Korean): composition events in `IO`, drawn inline in text fields
  and text areas; the examples forward it on Windows (137).
- Color emoji from the system's emoji font: COLR (versions 0 and 1), CBDT and sbix glyphs in a second atlas, with
  skin tones, families, keycaps and flags kept in one font (138, 139).
- Tables with any number of columns, resized and fitted by the user, scrolled sideways, sorted by clicking a
  header (`TableSort`) and reordered by dragging headers (140–144).
- Drag and drop in the core (`Carbon/Interaction/DragDrop.h`): drag sources with typed payloads, drop targets,
  previews, Escape, auto-scrolling scroll views; reordering of `List` rows and `OutlineView` items; files dropped
  from the system through `IO` (145–148).
- Phones and tablets: touch input with momentum scrolling and bounce, long presses, swipe back and pinch zoom
  (`PanBehavior`, `ZoomBehavior`); touch mode with 44-point targets and size classes; `NavigationSplitView`,
  collapsing split views, sheets from the bottom, a "more" menu in the menu bar and wrapping horizontal stacks in
  compact width; safe areas, the system's text size and the on-screen keyboard (152–173).
- The web app ([live demo](https://eliasertl.github.io/Carbon/)): a start screen, the Gallery and a reader for
  every document in `Docs/`, deployed by a manual GitHub Pages workflow; it works on phones and tablets (151,
  168, 169).
- An Android app of the Gallery on the OpenGL ES backend, built with Gradle and the NDK (171, 172).
- `HStackOptions::Wraps`, `NavigationSplitViewOptions::RootBackTitle`, `TextFieldOptions::Keyboard` and
  `Image` zooming (`Zoomable`) (158, 162, 166, 170).

### Changed

- The renderer backend contract is version 2 (`RendererBackendVersion`): backends receive a second, RGBA glyph
  atlas (`GlyphAtlasFormat::Color`, sampled through `ColorGlyphAtlasTextureID`) and draw
  `DrawPrimitiveKind::ColorGlyph`. Backends written for version 1 must be updated (138).
- A new container is drawn in its first frame unless its placement depends on measurements it does not have yet;
  it is no longer always hidden for a frame (131).
- `SliderOptions::Step` is a `double` (133).
- `BeginTable` returns `TableChanges`; `EndList`, `EndOutlineView` and `EndSelectionList` return the move a drag
  made (`ListMove`, `OutlineMove`, `RowMove`); `EndNavigationSplitView` returns `bool` (143, 146, 170).
- In touch mode, Return in a single-line `TextField` submits and ends editing (173).
- The installed package (`CarbonConfigVersion.cmake`) is compatible within a major version instead of a minor
  one.
- With `CARBON_DEPS_HARFBUZZ_BUILD=OFF`, HarfBuzz's raster library is also found in packages that install it
  without a CMake target (vcpkg's, Linux distributions'), so COLR emoji keep their colors there (176).
- Every example is built into a folder per backend (`Examples/<Backend>/<Example>`), and the minimal
  integrations share `Examples/Minimal` (135, 136).

### Removed

- The limit of 16 columns per table (140).
