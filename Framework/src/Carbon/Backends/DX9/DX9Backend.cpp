#include "Carbon/Backends/DX9/DX9Backend.h"

#include <memory>

#include "Carbon/Backends/DX9/DX9RendererInternal.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/Log.h"
#include "Carbon/Renderer/RendererBackend.h"

namespace Carbon
{
    namespace
    {
        Internal::DX9Renderer* GetRenderer(std::string_view function)
        {
            Internal::DX9Renderer* renderer = GetRendererBackend<Internal::DX9Renderer>();
            CB_VERIFY(renderer != nullptr, "{} needs the Direct3D 9 backend; call DX9Init first", function);
            return renderer;
        }
    } // namespace

    bool DX9Init(const DX9InitInfo& info)
    {
        if (info.Device == nullptr)
        {
            CB_LOG_ERROR("DX9", "DX9Init needs a device");
            return false;
        }
        if (!IsColorFormat(info.ColorFormat))
        {
            CB_LOG_ERROR("DX9", "Unsupported color format {}", ToString(info.ColorFormat));
            return false;
        }
        std::unique_ptr<Internal::DX9Renderer> renderer = std::make_unique<Internal::DX9Renderer>(info);
        if (!renderer->IsValid())
            return false;
        return InstallRendererBackend(std::move(renderer));
    }

    void DX9Shutdown()
    {
        if (GetRendererBackend<Internal::DX9Renderer>() != nullptr)
            RemoveRendererBackend();
    }

    void DX9Render()
    {
        Internal::DX9Renderer* renderer = GetRenderer("DX9Render");
        // While the device is lost, the frame's atlas changes stay pending until it is back.
        if (renderer != nullptr && renderer->IsDeviceReady())
            RenderDrawData();
    }

    void DX9InvalidateDeviceObjects()
    {
        Internal::DX9Renderer* renderer = GetRenderer("DX9InvalidateDeviceObjects");
        if (renderer != nullptr)
            renderer->InvalidateDeviceObjects();
    }

    TextureID DX9GetTextureID(IDirect3DTexture9* texture)
    {
        Internal::DX9Renderer* renderer = GetRenderer("DX9GetTextureID");
        return renderer != nullptr ? renderer->RegisterTexture(texture) : TextureID();
    }

    void DX9Image(IDirect3DTexture9* texture, Vec2 size, const ImageOptions& options)
    {
        Image(DX9GetTextureID(texture), size, options);
    }
} // namespace Carbon
