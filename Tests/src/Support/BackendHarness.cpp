#include "Support/BackendHarness.h"

namespace Carbon
{
    std::vector<std::string> GetCompiledBackends()
    {
        std::vector<std::string> names;
#if defined(CARBON_HAS_BACKEND_WEBGPU)
        names.emplace_back("WebGPU");
#endif
        return names;
    }

    std::unique_ptr<BackendHarness> CreateBackendHarness(std::string_view name)
    {
#if defined(CARBON_HAS_BACKEND_WEBGPU)
        if (name == "WebGPU")
            return CreateWebGPUHarness();
#endif
        static_cast<void>(name);
        return nullptr;
    }
} // namespace Carbon
