#pragma once

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace Carbon
{
    template <typename Signature>
    class FunctionRef;

    /// A non-owning reference to something callable, like C++26's std::function_ref: two pointers, never
    /// allocates. Carbon takes callbacks that are only called during the call that receives them as a FunctionRef,
    /// so passing a lambda costs nothing however much it captures.
    ///
    /// It refers to the callable, it does not copy it: keep the callable alive while the FunctionRef is used.
    /// A lambda written in the argument list lives until the call returns, which is all Carbon needs. Functions
    /// are referred to by their address and can be passed by name.
    template <typename Result, typename... Args>
    class FunctionRef<Result(Args...)>
    {
    public:
        /// Refers to a lambda or another callable object.
        template <typename Callable>
            requires(!std::same_as<std::remove_cvref_t<Callable>, FunctionRef> &&
                     !std::is_pointer_v<std::remove_cvref_t<Callable>> &&
                     !std::is_function_v<std::remove_cvref_t<Callable>> &&
                     std::is_invocable_r_v<Result, Callable&, Args...>)
        FunctionRef(Callable&& callable) noexcept
            : m_Invoke(
                  [](Storage storage, Args... args) -> Result
                  {
                      using Pointer = std::add_pointer_t<std::remove_reference_t<Callable>>;
                      return static_cast<Result>((*static_cast<Pointer>(storage.Object))(std::forward<Args>(args)...));
                  })
        {
            m_Storage.Object = const_cast<void*>(static_cast<const void*>(std::addressof(callable)));
        }

        /// Refers to a function.
        template <typename Function>
            requires(std::is_function_v<Function> && std::is_invocable_r_v<Result, Function*, Args...>)
        FunctionRef(Function* function) noexcept
            : m_Invoke(
                  [](Storage storage, Args... args) -> Result
                  {
                      return static_cast<Result>(
                          reinterpret_cast<Function*>(storage.Function)(std::forward<Args>(args)...));
                  })
        {
            m_Storage.Function = reinterpret_cast<void (*)()>(function);
        }

        /// Calls the referenced callable.
        Result operator()(Args... args) const { return m_Invoke(m_Storage, std::forward<Args>(args)...); }

    private:
        union Storage
        {
            void* Object;
            void (*Function)();
        };

    private:
        Storage m_Storage = {nullptr};
        Result (*m_Invoke)(Storage, Args...) = nullptr;
    };
} // namespace Carbon
