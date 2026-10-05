#pragma once

/// Umbrella header for component authors: everything a custom component is built from. CarbonExtensions uses
/// nothing but these headers, and neither should your components. See Docs/CustomComponents.md.

// Identity and per-item state.
#include "Carbon/Core/Hash.h"
#include "Carbon/Core/ID.h"
#include "Carbon/Core/State.h"

// Geometry and frame information.
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/Color.h"
#include "Carbon/Core/ContentScale.h"
#include "Carbon/Core/Context.h"
#include "Carbon/Core/EdgeInsets.h"
#include "Carbon/Core/Log.h"
#include "Carbon/Core/Math.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Core/UTF8.h"
#include "Carbon/Core/Vec2.h"

// Layout: reserving space and nesting containers.
#include "Carbon/Layout/Grid.h"
#include "Carbon/Layout/Layout.h"
#include "Carbon/Layout/ScrollView.h"
#include "Carbon/Layout/Size.h"
#include "Carbon/Layout/Stack.h"

// Input, hit testing, focus and overlays.
#include "Carbon/Input/Cursor.h"
#include "Carbon/Input/Input.h"
#include "Carbon/Input/Key.h"
#include "Carbon/Input/MouseButton.h"
#include "Carbon/Interaction/Interaction.h"
#include "Carbon/Overlay/Overlay.h"

// Drawing and text.
#include "Carbon/Draw/DrawList.h"
#include "Carbon/Draw/DrawTypes.h"
#include "Carbon/Draw/Squircle.h"
#include "Carbon/Text/Font.h"
#include "Carbon/Text/Icons.h"
#include "Carbon/Text/TextSpec.h"
#include "Carbon/Text/TextStyle.h"

// Animation and styling.
#include "Carbon/Animation/Animation.h"
#include "Carbon/Animation/Easing.h"
#include "Carbon/Animation/Spring.h"
#include "Carbon/Style/Style.h"
#include "Carbon/Style/StyleColor.h"
#include "Carbon/Style/StyleVar.h"
#include "Carbon/Style/Theme.h"
#include "Carbon/Widgets/ControlFeedback.h"
#include "Carbon/Widgets/ControlSize.h"

// The built-in widgets, for components that are composed of them.
#include "Carbon/Widgets/Button.h"
#include "Carbon/Widgets/Image.h"
#include "Carbon/Widgets/Separator.h"
#include "Carbon/Widgets/Slider.h"
#include "Carbon/Widgets/Text.h"
#include "Carbon/Widgets/TextField.h"
#include "Carbon/Widgets/Toggle.h"
#include "Carbon/Widgets/Tooltip.h"
