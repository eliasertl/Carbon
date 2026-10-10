#pragma once

#include <Carbon/Carbon.h>

namespace Example
{
    /// What a browser on a phone or a tablet adds to the examples' host: fingers and pens, the safe area, the text
    /// size, and the on-screen keyboard through a hidden input element that follows Carbon's caret, so that
    /// autocorrection, suggestions, CJK input methods and the iOS keyboard work. A mouse keeps going through GLFW,
    /// exactly as before. Everywhere else these functions do nothing. See Docs/Mobile.md.

    /// Sets the callbacks the host needs (the on-screen keyboard). Call it on the description before CreateContext.
    void InstallWebHost(Carbon::Callbacks& callbacks);

    /// Called after CreateContext: listens to the page's touch, pointer, keyboard and viewport events.
    void StartWebHost();

    /// Called before NewFrame: passes the safe area, the text size, the area the keyboard covers and the kind of
    /// pointer the device has to Carbon.
    void UpdateWebHostBeforeFrame();

    /// Called after EndFrame: moves the hidden input element to the caret and gives it the text being edited.
    void UpdateWebHostAfterFrame();
} // namespace Example
