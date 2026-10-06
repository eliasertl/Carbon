#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Carbon/Draw/DrawTypes.h"
#include "Carbon/Renderer/RendererBackend.h"

namespace Carbon
{
    struct Context;
} // namespace Carbon

namespace Carbon::Internal
{
    /// The renderer side of a context: the installed backend, and the bookkeeping that is the same for every
    /// backend (which host textures are in use, what the backend knows of the glyph atlas).
    struct RenderState
    {
        /// Null for a headless context.
        std::unique_ptr<RendererBackend> Backend;
        /// Address of the g_RendererBackendTag of the type the backend was installed as.
        const void* BackendTag = nullptr;

        /// Host textures by key, with the frame in which each was last registered or drawn.
        std::unordered_map<uint64_t, uint64_t> Textures;
        /// Textures that expired in NewFrame and that the backend has not been told about yet. Delivered with
        /// the next RenderDrawData or FlushGlyphAtlas, where the backend may touch the GPU.
        std::vector<TextureID> PendingReleases;

        /// The atlas generation the backend received last; 0 when it has to receive a full update.
        uint32_t AtlasGeneration = 0;
        bool HasReportedMissingBackend = false;
    };

    /// Called by NewFrame: expires host textures and tells the backend that a frame starts.
    void BeginRenderFrame(Context& context);

    /// Called by EndFrame once the draw data is final: marks the textures it draws as used and tells the backend.
    void EndRenderFrame(Context& context);

    /// Installs a backend into `context`; see Carbon::InstallRendererBackend.
    bool InstallRendererBackend(Context& context, std::unique_ptr<RendererBackend> backend, const void* tag);

    /// Destroys the backend of `context`, which must be the current context while this runs.
    void DestroyRendererBackend(Context& context);
} // namespace Carbon::Internal
