#pragma once

namespace Carbon::Internal
{
    /// True when, during this frame so far, a submenu was built, or the arrow keys opened or closed one. A menu bar
    /// then leaves the left and right arrow keys to the menus instead of moving to the neighbouring menu.
    bool DidMenusUseHorizontalArrows();
} // namespace Carbon::Internal
