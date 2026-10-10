#pragma once

#include <cstdint>

namespace Carbon
{
    /// Carbon keeps its macOS look on every device, but adapts to phones and tablets the way Apple's Human
    /// Interface Guidelines ask for iPhone and iPad. Two states, derived at the start of every frame, decide how:
    ///
    ///  - touch mode: the most recent pointer input came from a finger or a pen. Hit areas grow to at least
    ///    MinimumTouchTarget points, spacing grows, nothing reacts to hover, tooltips appear on a long press;
    ///  - the horizontal size class: compact when the display is narrower than CompactWidthLimit, as an iPhone in
    ///    portrait, regular otherwise, as an iPad in full screen. In compact width split views become navigation
    ///    stacks and menus and popovers become sheets that slide up from the bottom.
    ///
    /// The host can override both (IO::SetTouchModeOverride, IO::SetSizeClassOverride); applications query them
    /// to adapt layouts of their own. See Docs/Mobile.md.

    /// The width class of the display, after the size classes of Apple's HIG.
    enum class SizeClass : uint8_t
    {
        /// Room for content side by side: an iPad in full screen, a Mac or PC window.
        Regular,
        /// A narrow display: an iPhone in portrait, a narrow window.
        Compact
    };

    /// Displays narrower than this many points have the compact size class. iPhones are 320 to 440 points wide in
    /// portrait and iPads at least 744 points in full screen, so any limit between them tells the two apart.
    inline constexpr float CompactWidthLimit = 600.0f;

    /// The smallest side of the area that reacts to a finger in touch mode, in points: the HIG's minimum.
    inline constexpr float MinimumTouchTarget = 44.0f;

    /// True when the interface is in touch mode: the most recent pointer input came from a finger or a pen, or
    /// the host forced it. Valid between NewFrame and EndFrame; it changes only at the start of a frame.
    bool IsTouchMode();

    /// The size class of the display width this frame.
    SizeClass GetSizeClass();

    /// True when the display width is compact.
    inline bool IsCompactWidth()
    {
        return GetSizeClass() == SizeClass::Compact;
    }
} // namespace Carbon
