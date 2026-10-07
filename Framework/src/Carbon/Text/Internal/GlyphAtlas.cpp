#include "Carbon/Text/Internal/GlyphAtlas.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace Carbon::Internal
{
    namespace
    {
        // Empty texels between glyphs, so linear filtering never picks up a neighbour.
        constexpr uint32_t Gutter = 1;
    } // namespace

    GlyphAtlas::GlyphAtlas(uint32_t width, uint32_t height, uint32_t maxSize, uint32_t bytesPerTexel)
        : m_Width(width),
          m_Height(height),
          m_MaxSize(std::max(maxSize, std::max(width, height))),
          m_BytesPerTexel(std::max(bytesPerTexel, 1u))
    {
        m_Pixels.assign(static_cast<size_t>(m_Width) * m_Height * m_BytesPerTexel, 0);
        Clear();
        m_Generation = 1;
    }

    bool GlyphAtlas::Insert(uint32_t width, uint32_t height, const uint8_t* pixels, int32_t pitch, AtlasRegion& region)
    {
        region = AtlasRegion();
        if (width == 0 || height == 0)
            return true;

        const uint32_t paddedWidth = width + Gutter;
        const uint32_t paddedHeight = height + Gutter;
        uint32_t x = 0;
        uint32_t y = 0;
        while (!Pack(paddedWidth, paddedHeight, x, y))
        {
            if (!Grow())
                return false;
        }

        const size_t rowBytes = static_cast<size_t>(width) * m_BytesPerTexel;
        for (uint32_t row = 0; row < height; row++)
        {
            const uint8_t* source = pixels + static_cast<ptrdiff_t>(row) * pitch;
            uint8_t* destination = m_Pixels.data() + (static_cast<size_t>(y + row) * m_Width + x) * m_BytesPerTexel;
            std::memcpy(destination, source, rowBytes);
        }

        if (!IsDirty())
        {
            m_DirtyMinY = y;
            m_DirtyMaxY = y + height;
        }
        else
        {
            m_DirtyMinY = std::min(m_DirtyMinY, y);
            m_DirtyMaxY = std::max(m_DirtyMaxY, y + height);
        }

        region.X = x;
        region.Y = y;
        region.Width = width;
        region.Height = height;
        return true;
    }

    void GlyphAtlas::Clear()
    {
        std::fill(m_Pixels.begin(), m_Pixels.end(), static_cast<uint8_t>(0));
        m_Skyline.clear();
        // Keep a gutter along the left and top borders as well.
        m_Skyline.push_back(SkylineNode{static_cast<int32_t>(Gutter), static_cast<int32_t>(Gutter),
                                        static_cast<int32_t>(m_Width - Gutter)});
        m_Generation++;
        m_DirtyMinY = 0;
        m_DirtyMaxY = 0;
    }

    bool GlyphAtlas::SetMaxSize(uint32_t maxSize)
    {
        m_MaxSize = std::max<uint32_t>(maxSize, 1);
        if (m_Width <= m_MaxSize && m_Height <= m_MaxSize)
            return false;

        m_Width = std::min(m_Width, m_MaxSize);
        m_Height = std::min(m_Height, m_MaxSize);
        m_Pixels.assign(static_cast<size_t>(m_Width) * m_Height * m_BytesPerTexel, 0);
        Clear();
        return true;
    }

    void GlyphAtlas::ClearDirty()
    {
        m_DirtyMinY = 0;
        m_DirtyMaxY = 0;
    }

    bool GlyphAtlas::Pack(uint32_t width, uint32_t height, uint32_t& x, uint32_t& y)
    {
        const int32_t packWidth = static_cast<int32_t>(width);
        const int32_t packHeight = static_cast<int32_t>(height);

        // Bottom-left heuristic: the position with the lowest top edge wins; ties go to the narrowest node.
        int32_t bestTop = std::numeric_limits<int32_t>::max();
        int32_t bestWidth = std::numeric_limits<int32_t>::max();
        int32_t bestY = 0;
        size_t bestIndex = m_Skyline.size();
        for (size_t i = 0; i < m_Skyline.size(); i++)
        {
            const int32_t fitY = Fit(i, packWidth, packHeight);
            if (fitY < 0)
                continue;
            const int32_t top = fitY + packHeight;
            if (top < bestTop || (top == bestTop && m_Skyline[i].Width < bestWidth))
            {
                bestTop = top;
                bestWidth = m_Skyline[i].Width;
                bestY = fitY;
                bestIndex = i;
            }
        }
        if (bestIndex == m_Skyline.size())
            return false;

        const int32_t bestX = m_Skyline[bestIndex].X;
        AddSkylineLevel(bestIndex, bestX, bestY, packWidth, packHeight);
        x = static_cast<uint32_t>(bestX);
        y = static_cast<uint32_t>(bestY);
        return true;
    }

    int32_t GlyphAtlas::Fit(size_t index, int32_t width, int32_t height) const
    {
        const int32_t x = m_Skyline[index].X;
        if (x + width > static_cast<int32_t>(m_Width))
            return -1;

        int32_t y = m_Skyline[index].Y;
        int32_t remaining = width;
        size_t i = index;
        while (remaining > 0)
        {
            if (i >= m_Skyline.size())
                return -1;
            y = std::max(y, m_Skyline[i].Y);
            if (y + height > static_cast<int32_t>(m_Height))
                return -1;
            remaining -= m_Skyline[i].Width;
            i++;
        }
        return y;
    }

    void GlyphAtlas::AddSkylineLevel(size_t index, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        m_Skyline.insert(m_Skyline.begin() + static_cast<ptrdiff_t>(index), SkylineNode{x, y + height, width});

        // Shrink or remove the nodes the new level now covers.
        size_t next = index + 1;
        while (next < m_Skyline.size())
        {
            const SkylineNode& previous = m_Skyline[next - 1];
            SkylineNode& node = m_Skyline[next];
            const int32_t overlap = previous.X + previous.Width - node.X;
            if (overlap <= 0)
                break;
            node.X += overlap;
            node.Width -= overlap;
            if (node.Width > 0)
                break;
            m_Skyline.erase(m_Skyline.begin() + static_cast<ptrdiff_t>(next));
        }

        // Merge neighbours at the same height.
        for (size_t i = 0; i + 1 < m_Skyline.size();)
        {
            if (m_Skyline[i].Y == m_Skyline[i + 1].Y)
            {
                m_Skyline[i].Width += m_Skyline[i + 1].Width;
                m_Skyline.erase(m_Skyline.begin() + static_cast<ptrdiff_t>(i + 1));
            }
            else
            {
                i++;
            }
        }
    }

    bool GlyphAtlas::Grow()
    {
        if (m_Width >= m_MaxSize && m_Height >= m_MaxSize)
            return false;

        const uint32_t newWidth = std::min(m_Width * 2, m_MaxSize);
        const uint32_t newHeight = std::min(m_Height * 2, m_MaxSize);
        std::vector<uint8_t> pixels(static_cast<size_t>(newWidth) * newHeight * m_BytesPerTexel, 0);
        for (uint32_t row = 0; row < m_Height; row++)
        {
            std::memcpy(pixels.data() + static_cast<size_t>(row) * newWidth * m_BytesPerTexel,
                        m_Pixels.data() + static_cast<size_t>(row) * m_Width * m_BytesPerTexel,
                        static_cast<size_t>(m_Width) * m_BytesPerTexel);
        }
        m_Pixels = std::move(pixels);

        // The area to the right of the old atlas is free down to the top; the area below opens up through the
        // larger height.
        if (newWidth > m_Width)
        {
            m_Skyline.push_back(SkylineNode{static_cast<int32_t>(m_Width), static_cast<int32_t>(Gutter),
                                            static_cast<int32_t>(newWidth - m_Width)});
        }
        m_Width = newWidth;
        m_Height = newHeight;
        m_Generation++;
        m_DirtyMinY = 0;
        m_DirtyMaxY = 0;
        return true;
    }
} // namespace Carbon::Internal
