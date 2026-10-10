#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/Log.h"
#include "Carbon/Input/Cursor.h"

namespace Carbon
{
    /// Hooks the host provides. Every callback is optional; Carbon never touches the OS itself.
    struct Callbacks
    {
        /// Receives Carbon's log messages. `source` names the subsystem. Without it, logs are dropped.
        std::function<void(LogLevel level, std::string_view source, std::string_view message)> Log;
        /// Returns the clipboard's text as UTF-8. Without it, paste does nothing.
        std::function<std::string()> GetClipboardText;
        /// Replaces the clipboard's text (UTF-8). Without it, copy and cut do not reach the clipboard.
        std::function<void(std::string_view text)> SetClipboardText;
        /// Called at the end of a frame when the mouse cursor shape should change.
        std::function<void(Cursor cursor)> SetCursor;
        /// Called at the end of a frame in which a text control starts being edited (true), another one takes over
        /// (true again: its keyboard may differ), or editing ends (false). A host with an on-screen keyboard shows
        /// or hides it; IO::GetTextInputState() tells what kind. A host on a desktop leaves it unset.
        std::function<void(bool visible)> SetKeyboardVisible;
        /// Called when a CB_ASSERT or CB_VERIFY fails, after the failure was logged. When set, Carbon does not
        /// break into the debugger and continues.
        std::function<void(const AssertInfo& info)> AssertFailed;
    };
} // namespace Carbon
