#include "Support/BackendHarness.h"

namespace Carbon
{
    std::vector<std::string> GetCompiledBackends()
    {
        std::vector<std::string> names;
#if defined(CARBON_HAS_BACKEND_WEBGPU)
        names.emplace_back("WebGPU");
#endif
        // The GL harnesses need a window system; in Node (Emscripten) there is none.
#if defined(CARBON_HAS_BACKEND_OPENGL) && !defined(__EMSCRIPTEN__)
        names.emplace_back("OpenGL");
#endif
#if defined(CARBON_HAS_BACKEND_OPENGLES) && !defined(__EMSCRIPTEN__)
        names.emplace_back("OpenGLES");
#endif
#if defined(CARBON_HAS_BACKEND_VULKAN)
        // Both of the Vulkan backend's pipeline modes.
        names.emplace_back("Vulkan");
        names.emplace_back("VulkanRenderPass");
#endif
        return names;
    }

    std::unique_ptr<BackendHarness> CreateBackendHarness(std::string_view name)
    {
#if defined(CARBON_HAS_BACKEND_WEBGPU)
        if (name == "WebGPU")
            return CreateWebGPUHarness();
#endif
#if defined(CARBON_HAS_BACKEND_OPENGL) && !defined(__EMSCRIPTEN__)
        if (name == "OpenGL")
            return CreateOpenGLHarness(false);
#endif
#if defined(CARBON_HAS_BACKEND_OPENGLES) && !defined(__EMSCRIPTEN__)
        if (name == "OpenGLES")
            return CreateOpenGLHarness(true);
#endif
#if defined(CARBON_HAS_BACKEND_VULKAN)
        if (name == "Vulkan" || name == "VulkanRenderPass")
            return CreateVulkanHarness(name == "VulkanRenderPass");
#endif
        static_cast<void>(name);
        return nullptr;
    }
} // namespace Carbon
