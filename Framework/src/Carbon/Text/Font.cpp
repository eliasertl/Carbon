#include "Carbon/Text/Font.h"

#include <fstream>
#include <vector>

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/Log.h"
#include "Carbon/Text/Internal/TextSystem.h"

namespace Carbon
{
    Font* AddFontFromMemory(std::span<const uint8_t> data, const FontDescription& description)
    {
        Context& context = Internal::GetContext();
        return context.Text->AddFont(data, description.ItalicData, description.Name, true);
    }

    Font* AddFontFromFile(const std::filesystem::path& path, const FontDescription& description)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
        {
            CB_LOG_ERROR("Text", "Font file '{}' could not be opened", path.string());
            return nullptr;
        }
        const std::streamsize size = file.tellg();
        std::vector<uint8_t> data(static_cast<size_t>(size > 0 ? size : 0));
        file.seekg(0);
        if (data.empty() || !file.read(reinterpret_cast<char*>(data.data()), size))
        {
            CB_LOG_ERROR("Text", "Font file '{}' could not be read", path.string());
            return nullptr;
        }

        FontDescription named = description;
        const std::string fileName = path.filename().string();
        if (named.Name.empty())
            named.Name = fileName;
        return AddFontFromMemory(data, named);
    }

    Font* GetDefaultFont()
    {
        return Internal::GetContext().Text->GetDefaultFont();
    }

    Font* GetMonospacedFont()
    {
        return Internal::GetContext().Text->GetMonospacedFont();
    }
} // namespace Carbon
