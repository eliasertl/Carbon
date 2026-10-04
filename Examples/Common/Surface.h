#pragma once

#include <webgpu/webgpu_cpp.h>

struct GLFWwindow;

namespace Example
{
    /// Creates a WebGPU surface for a GLFW window: Win32 on Windows, X11 or Wayland on Linux.
    ///
    /// Dawn ships a helper for this (webgpu_glfw), but installed Dawn packages do not always include it, so the
    /// examples carry their own. This is host code; nothing like it exists inside the Carbon libraries.
    wgpu::Surface CreateSurfaceForWindow(const wgpu::Instance& instance, GLFWwindow* window);
} // namespace Example
