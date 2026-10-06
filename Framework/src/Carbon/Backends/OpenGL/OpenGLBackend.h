#pragma once

#include <cstdint>

#include "Carbon/Core/Vec2.h"
#include "Carbon/Draw/DrawTypes.h"
#include "Carbon/Renderer/TextureFormat.h"
#include "Carbon/Widgets/Image.h"

/// The OpenGL renderer backend, for OpenGL 3.3 core profile and later. Available when Carbon was built with
/// CARBON_BACKEND_OPENGL (then CARBON_HAS_BACKEND_OPENGL is defined). See Docs/Backends.md.
///
/// This header includes no OpenGL header: the host loads OpenGL with whatever it uses (glad, GLFW, SDL, ...), and
/// Carbon resolves the functions it needs through the host's GetProcAddress into a table of its own.

namespace Carbon
{
    /// A function as returned by a GetProcAddress-style loader.
    using OpenGLFunction = void (*)();

    /// A GetProcAddress-style function: returns the address of an OpenGL function of the current context by name,
    /// or null. It must return OpenGL 1.0 and 1.1 functions too; glfwGetProcAddress, SDL_GL_GetProcAddress and
    /// eglGetProcAddress (EGL 1.5) do.
    using OpenGLGetProcAddress = OpenGLFunction (*)(const char* name);

    /// What the OpenGL backend needs from the host.
    struct OpenGLInitInfo
    {
        /// Resolves OpenGL functions, such as glfwGetProcAddress.
        OpenGLGetProcAddress GetProcAddress = nullptr;
        /// Format of the framebuffers Carbon draws into. Only whether it is sRGB matters: with an sRGB format
        /// Carbon writes linear values and enables GL_FRAMEBUFFER_SRGB while it draws; otherwise it disables
        /// GL_FRAMEBUFFER_SRGB, so that its colors are written as they are.
        TextureFormat ColorFormat = TextureFormat::RGBA8Unorm;
    };

    /// Installs the OpenGL backend into the current context. The host's OpenGL context (3.3 core or later) must be
    /// current, here and in every other OpenGL* call. Returns false, and logs why, when a function cannot be
    /// resolved, the OpenGL version is too old, a shader does not compile, or the context has a backend already.
    bool OpenGLInit(const OpenGLInitInfo& info);

    /// Removes the OpenGL backend from the current context and deletes its OpenGL objects. The OpenGL context must
    /// be current.
    void OpenGLShutdown();

    /// Draws the last finished frame into the framebuffer that is bound to GL_DRAW_FRAMEBUFFER, whose size is the
    /// display size times the content scale. Call it after EndFrame.
    ///
    /// Carbon saves every piece of OpenGL state it changes and restores it before returning: program, vertex array,
    /// array buffer, active texture and the texture and sampler bindings of units 0 and 1, blending, scissor,
    /// culling, depth, stencil and color masks, viewport, polygon mode, primitive restart, logic op,
    /// GL_FRAMEBUFFER_SRGB and the pixel-unpack state. The host's own drawing before and after is not disturbed.
    void OpenGLRender();

    /// Returns the TextureID of one of the host's 2D textures (a GLuint name), to draw it with Image or
    /// DrawList::AddImage. Carbon samples it with its own sampler object (linear, clamped), so the texture's own
    /// filter settings do not matter. Row 0 of the texture is the top of the image; a texture the host rendered
    /// into through a framebuffer has row 0 at the bottom, so flip it with ImageOptions::UV.
    TextureID OpenGLGetTextureID(uint32_t texture);

    /// Displays one of the host's textures at `size` points: Image(OpenGLGetTextureID(texture), ...).
    void OpenGLImage(uint32_t texture, Vec2 size, const ImageOptions& options = {});
} // namespace Carbon
