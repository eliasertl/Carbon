#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <Carbon/Carbon.h>

namespace WebApp
{
    /// The PNG images of the documentation, as WebGL textures. Sizes are read from the files' headers, so that a
    /// document can be laid out before its images are decoded; an image is decoded and uploaded when it is first
    /// drawn, and Clear deletes the textures again, so that only the images of the shown document use memory.
    class DocImageCache
    {
    public:
        explicit DocImageCache(std::filesystem::path root) : m_Root(std::move(root)) {}
        ~DocImageCache();

        DocImageCache(const DocImageCache&) = delete;
        DocImageCache& operator=(const DocImageCache&) = delete;

        /// The size in pixels of the image at `path` (relative to the root); zero when it cannot be read.
        Carbon::Vec2 GetSize(std::string_view path);
        /// The texture of the image; invalid when it cannot be decoded. The WebGL context must be current.
        Carbon::TextureID GetTexture(std::string_view path);
        /// Deletes every texture. Call it between frames, never between drawing an image and rendering it.
        void Clear();

    private:
        struct Entry
        {
            std::string Path;
            Carbon::Vec2 Size;
            uint32_t Texture = 0;
            bool IsDecoded = false;
        };

        Entry& GetEntry(std::string_view path);

    private:
        std::filesystem::path m_Root;
        std::vector<Entry> m_Entries;
    };
} // namespace WebApp
