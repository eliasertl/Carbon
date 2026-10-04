// Uses an installed Carbon like an application would: a headless context, a few frames of an interface made of
// built-in widgets, extension components and a custom component. See CMakeLists.txt next to this file.

#include <cstdio>

#include <Carbon/Extensions/Extensions.h>

#include "StarRating.h"

int main()
{
    // Without a device Carbon produces draw data but cannot render it, which is all this check needs.
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

    Carbon::IO& io = Carbon::GetIO();
    io.SetDisplaySize(640.0f, 480.0f);
    io.SetDeltaTime(1.0f / 60.0f);

    int rating = 3;
    int segment = 0;
    size_t vertexCount = 0;
    for (int frame = 0; frame < 5; frame++)
    {
        Carbon::NewFrame();
        Carbon::BeginVStack({.Spacing = 12.0f, .Padding = 20.0f});
        Carbon::Text("Installed package", {.Style = Carbon::TextStyle::Title2});
        Carbon::Button("Button");
        Carbon::SegmentedControl("Segments", &segment, {"One", "Two"});
        Carbon::ProgressIndicator(0.5f);
        Example::StarRating("Rating", &rating);
        Carbon::EndVStack();
        Carbon::EndFrame();
        vertexCount = Carbon::GetDrawData().Vertices.size();
    }

    const Carbon::Version version = Carbon::GetExtensionsVersion();
    Carbon::DestroyContext(context);

    if (vertexCount == 0 || failedChecks != 0)
    {
        std::fprintf(stderr, "Package check failed: %zu vertices, %d failed checks\n", vertexCount, failedChecks);
        return 1;
    }
    std::printf("Carbon %u.%u.%u package check passed (%zu vertices)\n", version.Major, version.Minor, version.Patch,
                vertexCount);
    return 0;
}
