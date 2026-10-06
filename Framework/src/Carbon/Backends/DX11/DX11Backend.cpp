#include "Carbon/Backends/DX11/DX11Backend.h"

#include <memory>

#include "Carbon/Backends/DX11/DX11RendererInternal.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/Log.h"
#include "Carbon/Renderer/RendererBackend.h"

namespace Carbon
{
    namespace
    {
        Internal::DX11Renderer* GetRenderer(std::string_view function)
        {
            Internal::DX11Renderer* renderer = GetRendererBackend<Internal::DX11Renderer>();
            CB_VERIFY(renderer != nullptr, "{} needs the Direct3D 11 backend; call DX11Init first", function);
            return renderer;
        }
    } // namespace

    bool DX11Init(const DX11InitInfo& info)
    {
        if (info.Device == nullptr || info.Context == nullptr)
        {
            CB_LOG_ERROR("DX11", "DX11Init needs a device and a device context");
            return false;
        }
        if (!IsColorFormat(info.ColorFormat))
        {
            CB_LOG_ERROR("DX11", "Unsupported color format {}", ToString(info.ColorFormat));
            return false;
        }
        std::unique_ptr<Internal::DX11Renderer> renderer = std::make_unique<Internal::DX11Renderer>(info);
        if (!renderer->IsValid())
            return false;
        return InstallRendererBackend(std::move(renderer));
    }

    void DX11Shutdown()
    {
        if (GetRendererBackend<Internal::DX11Renderer>() != nullptr)
            RemoveRendererBackend();
    }

    void DX11Render(ID3D11DeviceContext* context)
    {
        Internal::DX11Renderer* renderer = GetRenderer("DX11Render");
        if (renderer == nullptr)
            return;
        renderer->SetContext(context);
        RenderDrawData();
        renderer->SetContext(nullptr);
    }

    TextureID DX11GetTextureID(ID3D11ShaderResourceView* view)
    {
        Internal::DX11Renderer* renderer = GetRenderer("DX11GetTextureID");
        return renderer != nullptr ? renderer->RegisterTexture(view) : TextureID();
    }

    void DX11Image(ID3D11ShaderResourceView* view, Vec2 size, const ImageOptions& options)
    {
        Image(DX11GetTextureID(view), size, options);
    }
} // namespace Carbon
