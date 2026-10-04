// webgpu_cpp.h must come before the native window-system headers: X11 defines macros such as `None`, `Always` and
// `Success` that collide with WebGPU's enumerators.
#include "Surface.h"

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace Example
{
#if defined(_WIN32)
    wgpu::Surface CreateSurfaceForWindow(const wgpu::Instance& instance, GLFWwindow* window)
    {
        wgpu::SurfaceSourceWindowsHWND source;
        source.hwnd = glfwGetWin32Window(window);
        source.hinstance = GetModuleHandleW(nullptr);
        wgpu::SurfaceDescriptor descriptor;
        descriptor.nextInChain = &source;
        return instance.CreateSurface(&descriptor);
    }
#elif defined(__linux__)
    wgpu::Surface CreateSurfaceForWindow(const wgpu::Instance& instance, GLFWwindow* window)
    {
        wgpu::SurfaceDescriptor descriptor;
        if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
        {
            wgpu::SurfaceSourceWaylandSurface source;
            source.display = glfwGetWaylandDisplay();
            source.surface = glfwGetWaylandWindow(window);
            descriptor.nextInChain = &source;
            return instance.CreateSurface(&descriptor);
        }

        wgpu::SurfaceSourceXlibWindow source;
        source.display = glfwGetX11Display();
        source.window = glfwGetX11Window(window);
        descriptor.nextInChain = &source;
        return instance.CreateSurface(&descriptor);
    }
#else
    wgpu::Surface CreateSurfaceForWindow(const wgpu::Instance&, GLFWwindow*)
    {
        return nullptr;
    }
#endif
} // namespace Example
