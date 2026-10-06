#include "Carbon/Renderer/RendererBackend.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/Log.h"
#include "Carbon/Renderer/RenderStateInternal.h"
#include "Carbon/Text/Internal/GlyphAtlas.h"
#include "Carbon/Text/Internal/TextSystem.h"

namespace Carbon::Internal
{
    namespace
    {
        // Tells the backend about the textures that expired since it was last called.
        void DeliverPendingReleases(RenderState& state)
        {
            for (const TextureID texture : state.PendingReleases)
                state.Backend->ReleaseTexture(texture);
            state.PendingReleases.clear();
        }

        // Sends the atlas rows the backend has not seen: everything for a new generation, else the dirty rows.
        void SendGlyphAtlas(Context& context)
        {
            RenderState& state = context.Render;
            GlyphAtlas& atlas = context.Text->GetAtlas();
            const bool isFull = state.AtlasGeneration != atlas.GetGeneration();
            if (!isFull && !atlas.IsDirty())
                return;

            GlyphAtlasUpdate update;
            update.Pixels = atlas.GetPixels();
            update.Width = atlas.GetWidth();
            update.Height = atlas.GetHeight();
            update.Generation = atlas.GetGeneration();
            update.FirstRow = isFull ? 0 : atlas.GetDirtyMinY();
            update.RowCount = isFull ? atlas.GetHeight() : atlas.GetDirtyMaxY() - atlas.GetDirtyMinY();
            update.IsFull = isFull;

            // Recorded before the call, so that a backend can ask for the update again from inside it.
            state.AtlasGeneration = atlas.GetGeneration();
            atlas.ClearDirty();
            state.Backend->UpdateGlyphAtlas(update);
        }

        void ForgetPendingRelease(RenderState& state, TextureID texture)
        {
            std::erase(state.PendingReleases, texture);
        }
    } // namespace

    void BeginRenderFrame(Context& context)
    {
        RenderState& state = context.Render;

        // A texture last used in frame N is kept through frame N + 1, whose draw data may still be rendered, and
        // expires at the start of N + 2.
        for (auto it = state.Textures.begin(); it != state.Textures.end();)
        {
            if (context.FrameCount > it->second + 1)
            {
                if (state.Backend != nullptr)
                    state.PendingReleases.push_back(TextureID{it->first});
                it = state.Textures.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (state.Backend != nullptr)
            state.Backend->BeginFrame(context.FrameCount);
    }

    void EndRenderFrame(Context& context)
    {
        RenderState& state = context.Render;
        if (!state.Textures.empty())
        {
            for (const DrawCommand& command : context.Draw.GetDrawData().Commands)
            {
                if (command.Texture == TextureID())
                    continue;
                const auto found = state.Textures.find(command.Texture.Value);
                if (found != state.Textures.end())
                    found->second = context.FrameCount;
            }
        }

        if (state.Backend != nullptr)
            state.Backend->EndFrame();
    }

    bool InstallRendererBackend(Context& context, std::unique_ptr<RendererBackend> backend, const void* tag)
    {
        RenderState& state = context.Render;
        CB_VERIFY(backend != nullptr, "InstallRendererBackend needs a backend");
        if (backend == nullptr)
            return false;
        CB_VERIFY(state.Backend == nullptr,
                  "The context already has the renderer backend '{}'; remove it before installing '{}'",
                  state.Backend->GetName(), backend->GetName());
        if (state.Backend != nullptr)
            return false;

        state.Backend = std::move(backend);
        state.BackendTag = tag;
        // Whatever was registered belonged to no backend, or to an earlier one.
        state.Textures.clear();
        state.PendingReleases.clear();
        state.AtlasGeneration = 0;
        state.HasReportedMissingBackend = false;
        context.Text->SetMaxAtlasSize(state.Backend->GetCapabilities().MaxTextureSize);
        CB_LOG_INFO("Renderer", "Renderer backend '{}' installed", state.Backend->GetName());
        return true;
    }

    void DestroyRendererBackend(Context& context)
    {
        RenderState& state = context.Render;
        if (state.Backend == nullptr)
            return;
        CB_LOG_INFO("Renderer", "Renderer backend '{}' removed", state.Backend->GetName());
        state.Backend.reset();
        state.BackendTag = nullptr;
        state.Textures.clear();
        state.PendingReleases.clear();
        state.AtlasGeneration = 0;
        context.Text->SetMaxAtlasSize(RendererBackendCapabilities().MaxTextureSize);
    }

    bool InstallRendererBackend(std::unique_ptr<RendererBackend> backend, const void* tag)
    {
        return InstallRendererBackend(GetContext(), std::move(backend), tag);
    }

    RendererBackend* GetRendererBackend(const void* tag)
    {
        const RenderState& state = GetContext().Render;
        return state.BackendTag == tag ? state.Backend.get() : nullptr;
    }
} // namespace Carbon::Internal

namespace Carbon
{
    RendererBackend* GetRendererBackend()
    {
        return Internal::GetContext().Render.Backend.get();
    }

    void RemoveRendererBackend()
    {
        Internal::DestroyRendererBackend(Internal::GetContext());
    }

    void RenderDrawData()
    {
        Context& context = Internal::GetContext();
        CB_VERIFY(!context.IsInFrame, "The frame must be rendered after EndFrame");
        if (context.IsInFrame)
            return;

        Internal::RenderState& state = context.Render;
        if (state.Backend == nullptr)
        {
            if (!state.HasReportedMissingBackend)
                CB_LOG_ERROR("Renderer", "Cannot render: the context has no renderer backend");
            state.HasReportedMissingBackend = true;
            return;
        }

        Internal::DeliverPendingReleases(state);

        const DrawData& drawData = context.Draw.GetDrawData();
        const long targetWidth = std::lround(drawData.DisplaySize.X * drawData.ContentScale);
        const long targetHeight = std::lround(drawData.DisplaySize.Y * drawData.ContentScale);
        if (drawData.Commands.empty() || targetWidth <= 0 || targetHeight <= 0)
            return;

        Internal::SendGlyphAtlas(context);
        state.Backend->Render(drawData);
    }

    void FlushGlyphAtlas()
    {
        Context& context = Internal::GetContext();
        if (context.Render.Backend == nullptr)
            return;
        Internal::DeliverPendingReleases(context.Render);
        Internal::SendGlyphAtlas(context);
    }

    void InvalidateGlyphAtlas()
    {
        Internal::GetContext().Render.AtlasGeneration = 0;
    }

    TextureID RegisterHostTexture(uint64_t key)
    {
        if (key == 0)
            return TextureID();
        Context& context = Internal::GetContext();
        context.Render.Textures[key] = context.FrameCount;
        // If the texture expired this frame and the backend does not know yet, it never needs to.
        Internal::ForgetPendingRelease(context.Render, TextureID{key});
        return TextureID{key};
    }

    void ReleaseHostTexture(uint64_t key)
    {
        Internal::RenderState& state = Internal::GetContext().Render;
        const bool wasRegistered = state.Textures.erase(key) != 0;
        const size_t pendingBefore = state.PendingReleases.size();
        Internal::ForgetPendingRelease(state, TextureID{key});
        const bool wasPending = state.PendingReleases.size() != pendingBefore;
        if ((wasRegistered || wasPending) && state.Backend != nullptr)
            state.Backend->ReleaseTexture(TextureID{key});
    }
} // namespace Carbon
