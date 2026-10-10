#include "DocImageCache.h"

#include <array>
#include <fstream>

#include <GLES3/gl3.h>

#include "DocLibrary.h"
#include "StbImage.h"

namespace WebApp
{
    namespace
    {
        // A PNG starts with an 8-byte signature and the IHDR chunk: length, type, then width and height as big-endian
        // 32-bit numbers.
        constexpr size_t PngHeaderSize = 24;

        uint32_t ReadBigEndian(const std::array<unsigned char, PngHeaderSize>& bytes, size_t offset)
        {
            return (uint32_t(bytes[offset]) << 24) | (uint32_t(bytes[offset + 1]) << 16) |
                   (uint32_t(bytes[offset + 2]) << 8) | uint32_t(bytes[offset + 3]);
        }
    } // namespace

    DocImageCache::~DocImageCache()
    {
        Clear();
    }

    Carbon::Vec2 DocImageCache::GetSize(std::string_view path)
    {
        return GetEntry(path).Size;
    }

    Carbon::TextureID DocImageCache::GetTexture(std::string_view path)
    {
        Entry& entry = GetEntry(path);
        if (!entry.IsDecoded)
        {
            entry.IsDecoded = true;
            const std::string data = ReadFile(m_Root / entry.Path);
            int width = 0;
            int height = 0;
            unsigned char* pixels = DecodeImage(data, width, height);
            if (pixels != nullptr)
            {
                GLuint texture = 0;
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                glBindTexture(GL_TEXTURE_2D, 0);
                FreeImage(pixels);
                entry.Texture = texture;
            }
        }
        return entry.Texture != 0 ? Carbon::MakeTextureID(entry.Texture) : Carbon::TextureID();
    }

    void DocImageCache::Clear()
    {
        for (Entry& entry : m_Entries)
        {
            if (entry.Texture != 0)
                glDeleteTextures(1, &entry.Texture);
        }
        m_Entries.clear();
    }

    DocImageCache::Entry& DocImageCache::GetEntry(std::string_view path)
    {
        for (Entry& entry : m_Entries)
        {
            if (entry.Path == path)
                return entry;
        }
        Entry& entry = m_Entries.emplace_back();
        entry.Path = std::string(path);

        std::array<unsigned char, PngHeaderSize> header = {};
        std::ifstream file(m_Root / entry.Path, std::ios::binary);
        if (file.read(reinterpret_cast<char*>(header.data()), header.size()) && header[1] == 'P' && header[2] == 'N' &&
            header[3] == 'G')
            entry.Size = Carbon::Vec2(float(ReadBigEndian(header, 16)), float(ReadBigEndian(header, 20)));
        return entry;
    }
} // namespace WebApp
