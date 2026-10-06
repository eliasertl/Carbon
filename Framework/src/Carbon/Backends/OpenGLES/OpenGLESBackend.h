#pragma once

#include <cstdint>

#include "Carbon/Core/Vec2.h"
#include "Carbon/Draw/DrawTypes.h"
#include "Carbon/Renderer/TextureFormat.h"
#include "Carbon/Widgets/Image.h"

/// The OpenGL ES renderer backend, for OpenGL ES 3.0 and later and for WebGL 2 (Emscripten): Android, iOS through
/// ANGLE, embedded Linux and the browser. Available when Carbon was built with CARBON_BACKEND_OPENGLES (then
/// CARBON_HAS_BACKEND_OPENGLES is defined). See Docs/Backends.md.
///
/// This header includes no OpenGL ES header: the host creates the context (EGL, SDL, GLFW, Emscripten) and Carbon
/// resolves the functions it needs through the host's GetProcAddress into a table of its own.

namespace Carbon
{
    /// A function as returned by a GetProcAddress-style loader.
    using OpenGLESFunction = void (*)();

    /// A GetProcAddress-style function: returns the address of an OpenGL ES function of the current context by
    /// name, or null. It must return every core OpenGL ES 3.0 function, which eglGetProcAddress (EGL 1.5),
    /// SDL_GL_GetProcAddress, glfwGetProcAddress and emscripten_webgl_get_proc_address do (with Emscripten, link
    /// with -sGL_ENABLE_GET_PROC_ADDRESS).
    using OpenGLESGetProcAddress = OpenGLESFunction (*)(const char* name);

    /// What the OpenGL ES backend needs from the host.
    struct OpenGLESInitInfo
    {
        /// Resolves OpenGL ES functions, such as eglGetProcAddress.
        OpenGLESGetProcAddress GetProcAddress = nullptr;
        /// Format of the framebuffers Carbon draws into. Only whether it is sRGB matters: with an sRGB format Carbon
        /// writes linear values, which OpenGL ES encodes. A browser canvas (WebGL) is RGBA8Unorm.
        TextureFormat ColorFormat = TextureFormat::RGBA8Unorm;
    };

    /// Installs the OpenGL ES backend into the current context. The host's OpenGL ES 3.0 (or WebGL 2) context must
    /// be current, here and in every other OpenGLES* call. Returns false, and logs why, when a function cannot be
    /// resolved, the version is too old, a shader does not compile, or the context has a backend already.
    bool OpenGLESInit(const OpenGLESInitInfo& info);

    /// Removes the OpenGL ES backend from the current context and deletes its objects. The context must be
    /// current.
    void OpenGLESShutdown();

    /// Draws the last finished frame into the framebuffer that is bound to GL_DRAW_FRAMEBUFFER, whose size is the
    /// display size times the content scale. Call it after EndFrame.
    ///
    /// Carbon saves every piece of state it changes and restores it before returning: program, vertex array, array
    /// buffer, active texture and the texture and sampler bindings of units 0 and 1, blending, scissor, culling,
    /// depth, stencil and color masks, viewport and the pixel-unpack state.
    void OpenGLESRender();

    /// Returns the TextureID of one of the host's 2D textures (a GLuint name), to draw it with Image or
    /// DrawList::AddImage. MakeTextureID(texture) gives the same ID without registering it. Carbon samples it with
    /// its own sampler object (linear, clamped). Row 0 of the texture is the top of the image.
    TextureID OpenGLESGetTextureID(uint32_t texture);

    /// Displays one of the host's textures at `size` points: Image(OpenGLESGetTextureID(texture), ...).
    void OpenGLESImage(uint32_t texture, Vec2 size, const ImageOptions& options = {});
} // namespace Carbon
