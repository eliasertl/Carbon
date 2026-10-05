# Components

Every component has a page with its purpose, the matching page of Apple's Human Interface Guidelines, an example,
its options and its keyboard behaviour.

## Core (`Carbon`)

| Component | Purpose |
| --- | --- |
| [Text](Text.md) | Displays text in one of the macOS text styles |
| [Icon](Icon.md) | Displays a Phosphor icon |
| [Button](Button.md) | Starts an action |
| [Toggle](Toggle.md) | A switch or checkbox bound to a `bool` |
| [Slider](Slider.md) | Picks a value from a range |
| [TextField](TextField.md) | Edits a single line of text |
| [Image](Image.md) | Displays a texture of the host |
| [Separator](Separator.md) | A thin line between items |
| [Tooltip](Tooltip.md) | Explains the control under the pointer |
| [Stacks and Spacer](Stack.md) | Lay items out vertically or horizontally |
| [Grid](Grid.md) | Rows of cells whose columns line up |
| [ScrollView](ScrollView.md) | A clipped, scrollable area |

## Extensions (`CarbonExtensions`)

Link `Carbon::Extensions` and include `Carbon/Extensions/Extensions.h`, or the header of a single component.
These components are built only on Carbon's public extension API; see
[Custom components](../CustomComponents.md) for building your own the same way.

| Component | Purpose |
| --- | --- |
| [Sidebar](Sidebar.md) | Navigates between the areas of an app |
| [TabView](TabView.md) | Several panes in one place, switched by tabs |
| [SplitView](SplitView.md) | Two panes with a movable divider |
| [SegmentedControl](SegmentedControl.md) | One of a few closely related choices |
| [PopUpButton](PopUpButton.md) | One value from a list, shown on a button |
| [PullDownButton](PullDownButton.md) | A button that opens a menu of commands |
| [Menu](Menu.md) | Commands on demand, with submenus |
| [ContextMenu](ContextMenu.md) | A menu at the pointer on a right click |
| [Popover](Popover.md) | A transient view attached to a control |
| [Alert](Alert.md) | Critical information that needs an answer |
| [Sheet](Sheet.md) | A modal view for a task of its own |
| [Stepper](Stepper.md) | Small steps on a value |
| [ProgressIndicator](ProgressIndicator.md) | Progress as a bar or a spinner |
| [SearchField](SearchField.md) | A text field for search terms |
| [ColorWell](ColorWell.md) | Shows a color and opens a picker |
| [List](List.md) | A scrolling column of selectable rows |
| [Table](Table.md) | Rows in columns with a header |
| [Charts](Chart.md) | Line and bar charts |
| [OutlineView](OutlineView.md) | Hierarchical rows with disclosure triangles, such as a file browser |
| [ColumnView](ColumnView.md) | A hierarchy as columns, one per level |
| [MenuBar](MenuBar.md) | The menus of a window, in a row at its top |
| [ComboBox](ComboBox.md) | A text field with a list of choices |
| [Notifications](Notification.md) | Banners that tell about something that happened |
| [RadioGroup](RadioGroup.md) | A few mutually exclusive choices as radio buttons |
| [PathControl](PathControl.md) | The path to an item, from its root; each part can be clicked |

Popovers, menus, alerts and sheets are [overlays](../Overlays.md).

## Conventions shared by all components

- **Label and ID.** The first argument of an interactive component is its label, which is also its identity.
  `"Delete##row3"` shows "Delete" and keeps the ID unique; `"###save"` fixes the ID whatever the text before it.
  Inside loops, wrap items in `PushID(index)` / `PopID()`.
- **Options.** The last argument is an options struct, used with designated initializers. Every field is
  optional: `Button("Delete", { .Role = ButtonRole::Destructive, .CornerRadius = 12.0f })`. Fields must be named
  in the order they are declared, which is the order of the tables on each page.
- **Styling precedence.** An option given in the call beats the style stack
  (`PushStyleColor` / `PushStyleVar`), which beats the theme. See [Styling](../Styling.md).
- **Return value.** Components that change a value return `true` on the frame it changed; buttons on the frame
  they were activated.
- **Size.** `Width` (and `Height` where present) take a `Size`: `Size::Fit()`, a number of points, or
  `Size::Fill()`. See [Layout](../Layout.md#sizes).
- **Control sizes.** `ControlSize::Small`, `Regular` (default) and `Large` are 20, 24 and 30 points tall in the
  default theme.
- **Disabled.** `.Disabled = true`, or a `PushDisabled()` / `PopDisabled()` scope around several components,
  dims them and makes them ignore input and Tab.
- **After the call.** `IsItemHovered()`, `IsItemFocused()`, `IsItemActive()`, `GetItemRect()` and `Tooltip()`
  refer to the component submitted last.
- **Begin and End.** Containers come as a pair. `BeginVStack`, `BeginGrid`, `BeginGridRow`, `BeginScrollView`, `BeginSidebar`, `BeginList`,
  `BeginTable`, `BeginTabView` and `BeginSplitView` always need their `End`. Pairs that can be closed return a
  `bool` from `Begin` (`BeginPopover`, `BeginMenu`, `BeginSubmenu`, `BeginContextMenu`, `BeginPullDownButton`,
  `BeginSheet`, `BeginOverlay`): call `End` only when it returned `true`.
- **Open at the same ID scope.** `OpenPopover("name")`, `OpenMenu`, `OpenAlert` and `OpenSheet` find their
  component by name, so call them where the matching `Begin` is called: not inside another `PushID`, and not
  from inside the overlay itself.
