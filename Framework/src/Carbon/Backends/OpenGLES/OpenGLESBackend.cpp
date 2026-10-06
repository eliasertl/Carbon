#include "Carbon/Backends/OpenGLES/OpenGLESBackend.h"

#include "Carbon/Backends/OpenGL/OpenGLRendererInternal.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Renderer/RendererBackend.h"

// OpenGL ES shares its renderer with the OpenGL backend (Backends/OpenGL), which has an ES mode for the state and
// shader differences. This file is only the public API.

namespace Carbon
{
    namespace
    {
        Internal::OpenGLESRenderer* GetRenderer(std::string_view function)
        {
            Internal::OpenGLESRenderer* renderer = GetRendererBackend<Internal::OpenGLESRenderer>();
            CB_VERIFY(renderer != nullptr, "{} needs the OpenGL ES backend; call OpenGLESInit first", function);
            return renderer;
        }
    } // namespace

    bool OpenGLESInit(const OpenGLESInitInfo& info)
    {
        return Internal::OpenGLRenderer::Install(info.GetProcAddress, info.ColorFormat, true);
    }

    void OpenGLESShutdown()
    {
        if (GetRendererBackend<Internal::OpenGLESRenderer>() != nullptr)
            RemoveRendererBackend();
    }

    void OpenGLESRender()
    {
        if (GetRenderer("OpenGLESRender") != nullptr)
            RenderDrawData();
    }

    TextureID OpenGLESGetTextureID(uint32_t texture)
    {
        Internal::OpenGLESRenderer* renderer = GetRenderer("OpenGLESGetTextureID");
        return renderer != nullptr ? renderer->RegisterTexture(texture) : TextureID();
    }

    void OpenGLESImage(uint32_t texture, Vec2 size, const ImageOptions& options)
    {
        Image(OpenGLESGetTextureID(texture), size, options);
    }
} // namespace Carbon
