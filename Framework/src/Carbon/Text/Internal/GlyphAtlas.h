#pragma once

#include <cstdint>
#include <vector>

namespace Carbon::Internal
{
    /// A rectangle inside the atlas, in texels.
    struct AtlasRegion
    {
        uint32_t X = 0;
        uint32_t Y = 0;
        uint32_t Width = 0;
        uint32_t Height = 0;
    };

    /// A texture that glyph bitmaps are packed into as they are first used: coverage with one byte per texel, or
    /// color glyphs with four.
    ///
    /// Packing uses the skyline algorithm. When the atlas is full it doubles in size up to a maximum; existing
    /// regions keep their texel coordinates, so quads already emitted this frame stay valid. The renderer uploads
    /// the dirty region, or everything when the generation changed.
    class GlyphAtlas
    {
    public:
        GlyphAtlas(uint32_t width, uint32_t height, uint32_t maxSize, uint32_t bytesPerTexel = 1);

        /// Copies a bitmap (`pitch` bytes per row, `width` texels of the atlas's size each) into the atlas and
        /// returns where it was placed. Returns false when it does not fit even after growing to the maximum size.
        bool Insert(uint32_t width, uint32_t height, const uint8_t* pixels, int32_t pitch, AtlasRegion& region);

        /// Removes all glyphs but keeps the current size.
        void Clear();

        /// Changes the size the atlas may grow to. An atlas that is larger already is cleared and shrunk to the
        /// new maximum; returns true when that happened, because every region handed out so far is then invalid.
        bool SetMaxSize(uint32_t maxSize);
        uint32_t GetMaxSize() const { return m_MaxSize; }

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        uint32_t GetBytesPerTexel() const { return m_BytesPerTexel; }
        /// Width * Height texels of GetBytesPerTexel() bytes each, row by row from the top.
        const std::vector<uint8_t>& GetPixels() const { return m_Pixels; }

        /// Changes whenever the atlas grows or is cleared: the texture must be recreated or fully re-uploaded.
        uint32_t GetGeneration() const { return m_Generation; }

        /// True when pixels changed since the last ClearDirty.
        bool IsDirty() const { return m_DirtyMaxY > m_DirtyMinY; }
        /// The texel rows [min, max) that changed since the last ClearDirty.
        uint32_t GetDirtyMinY() const { return m_DirtyMinY; }
        uint32_t GetDirtyMaxY() const { return m_DirtyMaxY; }
        void ClearDirty();

    private:
        struct SkylineNode
        {
            int32_t X = 0;
            int32_t Y = 0;
            int32_t Width = 0;
        };

        bool Pack(uint32_t width, uint32_t height, uint32_t& x, uint32_t& y);
        int32_t Fit(size_t index, int32_t width, int32_t height) const;
        void AddSkylineLevel(size_t index, int32_t x, int32_t y, int32_t width, int32_t height);
        bool Grow();

    private:
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_MaxSize;
        uint32_t m_BytesPerTexel;
        std::vector<uint8_t> m_Pixels;
        std::vector<SkylineNode> m_Skyline;
        uint32_t m_Generation = 1;
        uint32_t m_DirtyMinY = 0;
        uint32_t m_DirtyMaxY = 0;
    };
} // namespace Carbon::Internal
