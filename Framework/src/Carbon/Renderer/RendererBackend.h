#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

#include "Carbon/Draw/DrawTypes.h"

namespace Carbon
{
    /// Version of the contract between Carbon and a renderer backend: this interface, the layout and meaning of
    /// the draw data (DrawVertex, DrawPrimitive, DrawPrimitiveKind, DrawCommand), the UV and color conventions and
    /// the format of the glyph atlas. It changes whenever an existing backend has to change to stay correct.
    ///
    /// A backend outside the repository pins the version it was written for, so that a later change stops its
    /// build instead of drawing wrong pixels: `static_assert(Carbon::RendererBackendVersion == 2);`
    ///
    /// Version 2 added color glyphs: a second atlas (GlyphAtlasFormat::Color) that draw commands sample through
    /// ColorGlyphAtlasTextureID, and DrawPrimitiveKind::ColorGlyph.
    inline constexpr uint32_t RendererBackendVersion = 2;

    /// Limits of the device a backend draws with. Every field has a default that suits a desktop GPU, so a
    /// backend only sets what its device restricts.
    struct RendererBackendCapabilities
    {
        /// Largest width and height of a texture, in texels. The glyph atlas never grows beyond it (nor beyond
        /// Carbon's own limit of 4096).
        uint32_t MaxTextureSize = 4096;
    };

    /// The two glyph atlases, by the format of their texels.
    enum class GlyphAtlasFormat : uint8_t
    {
        /// The glyph atlas: one byte per texel (an 8-bit single-channel texture such as R8Unorm) holding the
        /// coverage of every glyph in use. Draw commands with the default TextureID sample it.
        Coverage,
        /// The color glyph atlas: four bytes per texel, R, G, B, A in that order (an RGBA8Unorm texture), with
        /// premultiplied alpha and sRGB-encoded colors like vertex colors. Draw commands with
        /// ColorGlyphAtlasTextureID sample it. It exists only once a color glyph (an emoji) has been drawn.
        Color
    };

    /// Glyph-atlas pixels a backend has to copy into one of its two atlas textures. Glyph quads carry UVs in
    /// texels of their atlas.
    struct GlyphAtlasUpdate
    {
        /// Which atlas, and so the format of its texels.
        GlyphAtlasFormat Format = GlyphAtlasFormat::Coverage;
        /// The whole atlas: Width * Height texels (one byte each for Coverage, four for Color), row by row from
        /// the top, without padding. Valid during the call only.
        std::span<const uint8_t> Pixels;
        /// Size of the atlas in texels.
        uint32_t Width = 0;
        uint32_t Height = 0;
        /// Changes whenever the atlas grew or was cleared.
        uint32_t Generation = 0;
        /// The rows [FirstRow, FirstRow + RowCount) differ from what this backend received before. A full update
        /// covers every row.
        uint32_t FirstRow = 0;
        uint32_t RowCount = 0;
        /// True when the texture has to be created or written completely: the first update a backend receives,
        /// a new generation (the size may have changed), or after InvalidateGlyphAtlas.
        bool IsFull = false;
    };

    /// What Carbon needs from a graphics API. A renderer backend implements this interface and is installed into
    /// a context with InstallRendererBackend; it is the only connection between Carbon's core and the GPU.
    ///
    /// Carbon ships backends for several APIs (see Docs/Backends.md), and an application can write its own
    /// against this header alone. Native objects (devices, command buffers, render passes, textures) never pass
    /// through the interface: a backend takes them in functions of its own, keeps what it needs, and calls
    /// RenderDrawData when the host asks it to draw.
    ///
    /// UpdateGlyphAtlas, Render and ReleaseTexture are called only from inside RenderDrawData, FlushGlyphAtlas
    /// and ReleaseHostTexture, which the backend's own functions call, and when the backend is destroyed. They
    /// can therefore rely on whatever the backend's functions require of the host (a current OpenGL context, the
    /// render thread). BeginFrame and EndFrame are called from NewFrame and EndFrame and must not touch the GPU.
    class RendererBackend
    {
    public:
        /// Releases everything the backend created. The backend's context is current while it is destroyed.
        virtual ~RendererBackend() = default;

        /// A short name for log messages, such as "Vulkan".
        virtual std::string_view GetName() const = 0;

        /// The limits of the backend's device. Read once, when the backend is installed.
        virtual RendererBackendCapabilities GetCapabilities() const { return {}; }

        /// Called by NewFrame with the number of the frame that starts. For bookkeeping such as counting down to
        /// the destruction of objects that frames in flight may still use.
        virtual void BeginFrame(uint64_t frameCount) { static_cast<void>(frameCount); }

        /// Called by EndFrame when the frame's draw data is final. Every Render call until the next EndFrame gets
        /// that same draw data, so a backend can upload its buffers once per frame however often it is drawn.
        virtual void EndFrame() {}

        /// Copies glyph-atlas pixels into the backend's texture for that atlas (update.Format). Called before Render
        /// whenever an atlas changed, and always once for the glyph atlas, with a full update, before the first
        /// Render; the color glyph atlas follows with a full update of its own once it exists.
        virtual void UpdateGlyphAtlas(const GlyphAtlasUpdate& update) = 0;

        /// Draws a finished frame into the target the host gave the backend. `drawData` has at least one command
        /// and a display of at least one pixel; it stays valid until the next NewFrame.
        ///
        /// Draw every command in order with premultiplied-alpha blending, clipped to its ClipRect (points; times
        /// ContentScale for pixels). TextureID() is the glyph atlas and ColorGlyphAtlasTextureID the color glyph
        /// atlas; other IDs are host textures. An ID the backend has not seen yet is a native handle the host drew
        /// without registering it (MakeTextureID): the backend resolves it then, as its GetTextureID function
        /// would with default settings, or skips the command with a warning if its API cannot draw a raw handle.
        virtual void Render(const DrawData& drawData) = 0;

        /// A host texture is no longer in use: a whole frame passed in which it was neither registered nor
        /// drawn, or ReleaseHostTexture was called. The backend drops what it keeps for the texture, whether it
        /// was registered or resolved from a raw handle in Render.
        virtual void ReleaseTexture(TextureID texture) = 0;
    };

    namespace Internal
    {
        /// One object per backend type; its address identifies the type without run-time type information.
        /// Not const: linkers may merge identical constants, which would give two types one address.
        template <typename T>
        inline char g_RendererBackendTag = 0;

        bool InstallRendererBackend(std::unique_ptr<RendererBackend> backend, const void* tag);
        RendererBackend* GetRendererBackend(const void* tag);
    } // namespace Internal

    /// Installs a backend into the current context, which takes ownership. A context has one backend at a time:
    /// if one is installed already, this fails a check, destroys `backend` and returns false. The backends that
    /// ship with Carbon call this from their Init function.
    template <std::derived_from<RendererBackend> T>
    bool InstallRendererBackend(std::unique_ptr<T> backend)
    {
        return Internal::InstallRendererBackend(std::move(backend), &Internal::g_RendererBackendTag<T>);
    }

    /// Returns the current context's backend if it was installed as a T, and null otherwise. This is how a
    /// backend's own functions get at their object.
    template <std::derived_from<RendererBackend> T>
    T* GetRendererBackend()
    {
        return static_cast<T*>(Internal::GetRendererBackend(&Internal::g_RendererBackendTag<T>));
    }

    /// Returns the current context's backend, or null for a headless context.
    RendererBackend* GetRendererBackend();

    /// Destroys the current context's backend; the context is headless again. Does nothing without one.
    /// DestroyContext does the same. The backends that ship with Carbon call this from their Shutdown function.
    void RemoveRendererBackend();

    /// Hands the last finished frame to the current context's backend: host textures that expired
    /// (ReleaseTexture), glyph-atlas changes (UpdateGlyphAtlas), then the draw data (Render). A backend calls this
    /// from its own render function, after storing the native target it was given. It may be called more than
    /// once per frame to draw the same frame into several targets.
    ///
    /// Must be called after EndFrame. Does nothing for an empty frame, and logs an error (once) without a backend.
    void RenderDrawData();

    /// Hands expired host textures and glyph-atlas changes to the backend now, without drawing. For APIs that
    /// cannot upload textures where the frame is drawn (inside a render pass): a backend can offer a function
    /// the host calls earlier. RenderDrawData then has nothing left to upload.
    void FlushGlyphAtlas();

    /// Makes the next update of both glyph atlases a full one. For a backend that lost its textures (device reset,
    /// lost context) or could not create them. May be called from UpdateGlyphAtlas.
    void InvalidateGlyphAtlas();

    /// Marks a host texture as used in the current frame and returns its TextureID, whose Value is `key`. The key
    /// is whatever identifies the texture to the backend, such as a handle or the address of an object; zero is
    /// the glyph atlas and returns the default TextureID, and the color glyph atlas's value is not a valid key.
    ///
    /// A texture stays registered while it is registered again or drawn in every frame; after a whole frame
    /// without either, the backend's ReleaseTexture is called. A backend calls this from its GetTextureID
    /// function and keeps what it needs to bind the texture (a view, a descriptor set) under the same key. Use the
    /// key MakeTextureID gives for the native handle, so that a registered texture and the same texture drawn by
    /// its raw handle are one texture.
    TextureID RegisterHostTexture(uint64_t key);

    /// Forgets a host texture now and calls the backend's ReleaseTexture. For hosts that destroy a texture
    /// before the frame after its last use, where the API may hand out the same key for a new object.
    void ReleaseHostTexture(uint64_t key);
} // namespace Carbon
