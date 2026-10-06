#pragma once

#include "Carbon/Core/Callbacks.h"

namespace Carbon
{
    /// Everything needed to create a context. The host owns the window, the device and every surface or texture
    /// Carbon renders into; a renderer backend connects the context to them (WebGPUInit, VulkanInit, ...). A
    /// context without a backend is headless: it builds draw data but cannot render (unit tests, tools).
    struct ContextDescription
    {
        /// Host hooks: logging, clipboard, cursor, asserts.
        Carbon::Callbacks Callbacks = {};
    };
} // namespace Carbon
