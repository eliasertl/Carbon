#include "Carbon/Core/ID.h"

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/Hash.h"

namespace Carbon
{
    namespace
    {
        ID Finish(uint64_t hash)
        {
            return ID{hash == 0 ? 1 : hash};
        }

        uint64_t Begin(ID seed)
        {
            return seed.IsValid() ? seed.Value : HashSeed;
        }
    } // namespace

    ID HashID(std::string_view label, ID seed)
    {
        const size_t stable = label.find("###");
        if (stable != std::string_view::npos)
            label = label.substr(stable);
        return Finish(HashBytes(label, Begin(seed)));
    }

    ID HashID(int64_t value, ID seed)
    {
        // A marker byte keeps integer IDs distinct from one-character string IDs.
        const uint64_t marked = HashBytes(std::string_view("\xFF", 1), Begin(seed));
        return Finish(HashCombine(marked, static_cast<uint64_t>(value)));
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
