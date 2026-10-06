#include "Carbon/Backends/OpenGL/OpenGLBackend.h"

#include <memory>

#include "Carbon/Backends/OpenGL/OpenGLFunctionsInternal.h"
#include "Carbon/Backends/OpenGL/OpenGLRendererInternal.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/Log.h"
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
        if (info.GetProcAddress == nullptr)
        {
            CB_LOG_ERROR("OpenGL", "OpenGLInit needs a GetProcAddress function");
            return false;
        }
        if (!IsColorFormat(info.ColorFormat))
        {
            CB_LOG_ERROR("OpenGL", "Unsupported color format {}", ToString(info.ColorFormat));
            return false;
        }

        Internal::OpenGLFunctions functions;
        std::string_view missing;
        if (!functions.Load(info.GetProcAddress, missing))
        {
            CB_LOG_ERROR("OpenGL", "The OpenGL function {} could not be resolved; is an OpenGL 3.3 context current?",
                         missing);
            return false;
        }
        Internal::GLint major = 0;
        Internal::GLint minor = 0;
        functions.GetIntegerv(Internal::GL::MajorVersion, &major);
        functions.GetIntegerv(Internal::GL::MinorVersion, &minor);
        if (major < 3 || (major == 3 && minor < 3))
        {
            CB_LOG_ERROR("OpenGL", "The OpenGL backend needs OpenGL 3.3; the context has {}.{}", major, minor);
            return false;
        }

        std::unique_ptr<Internal::OpenGLRenderer> renderer =
            std::make_unique<Internal::OpenGLRenderer>(functions, info.ColorFormat);
        if (!renderer->IsValid())
            return false;
        return InstallRendererBackend(std::move(renderer));
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
