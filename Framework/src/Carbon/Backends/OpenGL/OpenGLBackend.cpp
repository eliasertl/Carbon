#include "Carbon/Backends/OpenGL/OpenGLBackend.h"

#include "Carbon/Backends/OpenGL/OpenGLRendererInternal.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Renderer/RendererBackend.h"

namespace Carbon
{
    namespace
    {
        Internal::OpenGLRenderer* GetRenderer(std::string_view function)
        {
            Internal::OpenGLRenderer* renderer = GetRendererBackend<Internal::OpenGLRenderer>();
            CB_VERIFY(renderer != nullptr, "{} needs the OpenGL backend; call OpenGLInit first", function);
            return renderer;
        }
    } // namespace

    bool OpenGLInit(const OpenGLInitInfo& info)
    {
        return Internal::OpenGLRenderer::Install(info.GetProcAddress, info.ColorFormat, false);
    }

    void OpenGLShutdown()
    {
        if (GetRendererBackend<Internal::OpenGLRenderer>() != nullptr)
            RemoveRendererBackend();
    }

    void OpenGLRender()
    {
        if (GetRenderer("OpenGLRender") != nullptr)
            RenderDrawData();
    }

    TextureID OpenGLGetTextureID(uint32_t texture)
    {
        Internal::OpenGLRenderer* renderer = GetRenderer("OpenGLGetTextureID");
        return renderer != nullptr ? renderer->RegisterTexture(texture) : TextureID();
    }

    void OpenGLImage(uint32_t texture, Vec2 size, const ImageOptions& options)
    {
        Image(OpenGLGetTextureID(texture), size, options);
    }
} // namespace Carbon
