#pragma once

#include <cstdint>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon::Internal
{
    /// The scratch state of a component between its Begin and End calls: one per context, kept in the context's
    /// per-ID state under a name. Looking it up hashes the name, and the rows and cells of a long list ask for it
    /// thousands of times per frame, so the pointer is remembered for the frame it was looked up in.
    ///
    ///     BuildState<TableBuild> s_Build("Carbon.Table.Build");
    ///     TableBuild& build = s_Build.Get();
    template <typename T>
    class BuildState
    {
    public:
        explicit constexpr BuildState(std::string_view name) : m_Name(name) {}

        /// The state of the current context. Zero-initialized the first time a context asks for it.
        T& Get()
        {
            const Context* context = GetCurrentContext();
            const uint64_t frame = GetFrameCount();
            if (m_State == nullptr || m_Context != context || m_Frame != frame)
            {
                m_State = GetState<T>(HashID(m_Name), StateLifetime::Persistent);
                m_Context = context;
                m_Frame = frame;
            }
            return *m_State;
        }

    private:
        std::string_view m_Name;
        T* m_State = nullptr;
        const Context* m_Context = nullptr;
        uint64_t m_Frame = 0;
    };
} // namespace Carbon::Internal
