#include "Carbon/Core/ID.h"

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"

namespace Carbon
{
    namespace
    {
        // 64-bit FNV-1a.
        constexpr uint64_t FnvOffsetBasis = 14695981039346656037ull;
        constexpr uint64_t FnvPrime = 1099511628211ull;

        uint64_t HashBytes(const unsigned char* data, size_t size, uint64_t hash)
        {
            for (size_t i = 0; i < size; i++)
            {
                hash ^= data[i];
                hash *= FnvPrime;
            }
            return hash;
        }

        ID Finish(uint64_t hash)
        {
            return ID{hash == 0 ? 1 : hash};
        }

        uint64_t Begin(ID seed)
        {
            return seed.IsValid() ? seed.Value : FnvOffsetBasis;
        }
    } // namespace

    ID HashID(std::string_view label, ID seed)
    {
        const size_t stable = label.find("###");
        if (stable != std::string_view::npos)
            label = label.substr(stable);
        return Finish(HashBytes(reinterpret_cast<const unsigned char*>(label.data()), label.size(), Begin(seed)));
    }

    ID HashID(int64_t value, ID seed)
    {
        unsigned char bytes[sizeof(int64_t)];
        for (size_t i = 0; i < sizeof(int64_t); i++)
            bytes[i] = static_cast<unsigned char>((static_cast<uint64_t>(value) >> (i * 8)) & 0xFF);
        // A marker byte keeps integer IDs distinct from one-character string IDs.
        const unsigned char marker = 0xFF;
        const uint64_t hash = HashBytes(&marker, 1, Begin(seed));
        return Finish(HashBytes(bytes, sizeof(bytes), hash));
    }

    std::string_view GetDisplayLabel(std::string_view label)
    {
        const size_t hidden = label.find("##");
        return hidden == std::string_view::npos ? label : label.substr(0, hidden);
    }

    void PushID(std::string_view id)
    {
        PushID(GetID(id));
    }

    void PushID(int64_t id)
    {
        PushID(GetID(id));
    }

    void PushID(ID id)
    {
        Context& context = Internal::GetContext();
        context.IDStack.push_back(id);
    }

    void PopID()
    {
        Context& context = Internal::GetContext();
        CB_VERIFY(context.IDStack.size() > 1, "PopID called without a matching PushID");
        if (context.IDStack.size() > 1)
            context.IDStack.pop_back();
    }

    ID GetID(std::string_view label)
    {
        const Context& context = Internal::GetContext();
        return HashID(label, context.IDStack.back());
    }

    ID GetID(int64_t value)
    {
        const Context& context = Internal::GetContext();
        return HashID(value, context.IDStack.back());
    }
} // namespace Carbon
