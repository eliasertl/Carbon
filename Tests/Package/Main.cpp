// Uses an installed Carbon like an application would: a context, a few frames of an interface made of built-in
// widgets, extension components and a custom component, drawn by a renderer backend written here against the
// installed headers alone. See CMakeLists.txt next to this file.

#include <cstdio>
#include <memory>

#include <Carbon/Extensions/Extensions.h>
#include <Carbon/Reflection/Reflection.h>

#include "StarRating.h"

namespace PackageCheck
{
    // A renderer backend written outside Carbon, against the installed Carbon/Renderer/RendererBackend.h only. It
    // draws nothing; it counts what Carbon hands it, which is what a real backend would upload and draw.
    static_assert(Carbon::RendererBackendVersion == 2, "This backend was written for version 2 of the contract");

    class CountingBackend : public Carbon::RendererBackend
    {
    public:
        std::string_view GetName() const override { return "Counting"; }
        void UpdateGlyphAtlas(const Carbon::GlyphAtlasUpdate& update) override
        {
            AtlasUpdates++;
            AtlasBytes += update.Pixels.size();
        }
        void Render(const Carbon::DrawData& drawData) override
        {
            Renders++;
            Commands += drawData.Commands.size();
        }
        void ReleaseTexture(Carbon::TextureID) override { Releases++; }

    public:
        int AtlasUpdates = 0;
        size_t AtlasBytes = 0;
        int Renders = 0;
        size_t Commands = 0;
        int Releases = 0;
    };

    // The backend's own functions, in the shape of the built-in ones.
    bool CountingInit()
    {
        return Carbon::InstallRendererBackend(std::make_unique<CountingBackend>());
    }

    void CountingRender()
    {
        if (Carbon::GetRendererBackend<CountingBackend>() != nullptr)
            Carbon::RenderDrawData();
    }

    enum class Quality
    {
        Low,
        High,
        VeryHigh
    };
    CB_REFLECT_ENUM(Quality, CB_VALUE(VeryHigh, {.DisplayName = "Ultra"}));

    struct Settings
    {
        Quality TextureQuality = Quality::High;
        bool VSync = true;
    };
} // namespace PackageCheck

int main()
{
    Carbon::ContextDescription description;
    description.Callbacks.Log = [](Carbon::LogLevel level, std::string_view source, std::string_view message)
    {
        if (level >= Carbon::LogLevel::Warning)
        {
            std::fprintf(stderr, "%.*s: %.*s\n", static_cast<int>(source.size()), source.data(),
                         static_cast<int>(message.size()), message.data());
        }
    };
    int failedChecks = 0;
    description.Callbacks.AssertFailed = [&failedChecks](const Carbon::AssertInfo&) { failedChecks++; };
    Carbon::Context* context = Carbon::CreateContext(description);
    const bool isInstalled = PackageCheck::CountingInit();
    const PackageCheck::CountingBackend* backend = Carbon::GetRendererBackend<PackageCheck::CountingBackend>();

    Carbon::IO& io = Carbon::GetIO();
    io.SetDisplaySize(640.0f, 480.0f);
    io.SetDeltaTime(1.0f / 60.0f);

    int rating = 3;
    int segment = 0;
    PackageCheck::Settings settings;
    size_t vertexCount = 0;
    for (int frame = 0; frame < 5; frame++)
    {
        Carbon::NewFrame();
        Carbon::BeginVStack({.Spacing = 12.0f, .Padding = 20.0f});
        Carbon::Text("Installed package", {.Style = Carbon::TextStyle::Title2});
        Carbon::Button("Button");
        Carbon::SegmentedControl("Segments", &segment, {"One", "Two"});
        Carbon::Reflect("Settings", &settings);
        Carbon::ProgressIndicator(0.5f);
        Example::StarRating("Rating", &rating);
        Carbon::EndVStack();
        Carbon::EndFrame();
        vertexCount = Carbon::GetDrawData().Vertices.size();
        PackageCheck::CountingRender();
    }
    // Containers fade in during their first frames, which have nothing to draw and are not rendered.
    const bool isRendered = isInstalled && backend != nullptr && backend->Renders > 0 && backend->Commands > 0 &&
                            backend->AtlasUpdates >= 1 && backend->AtlasBytes > 0;

    const Carbon::Version version = Carbon::GetExtensionsVersion();
    const bool isReflected = Carbon::GetEnumCount<PackageCheck::Quality>() == 3 &&
                             Carbon::GetEnumDisplayName(PackageCheck::Quality::VeryHigh) == "Ultra" &&
                             Carbon::GetFieldDisplayName<PackageCheck::Settings>(0) == "Texture Quality";
    Carbon::DestroyContext(context);

    if (vertexCount == 0 || failedChecks != 0 || !isReflected || !isRendered)
    {
        std::fprintf(stderr, "Package check failed: %zu vertices, %d failed checks, reflection %s, custom backend %s\n",
                     vertexCount, failedChecks, isReflected ? "works" : "failed", isRendered ? "works" : "failed");
        return 1;
    }
    std::printf("Carbon %u.%u.%u package check passed (%zu vertices)\n", version.Major, version.Minor, version.Patch,
                vertexCount);
    return 0;
}
