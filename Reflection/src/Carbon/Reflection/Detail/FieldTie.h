#pragma once

#include <cstddef>
#include <tuple>

// Structured bindings for aggregates of up to MaxReflectedFieldCount fields. A structured binding needs the number
// of names written out, so there is one branch per count. Written by a script; regenerate rather than edit by hand.

namespace Carbon::Internal
{
    /// The most fields an aggregate can have to be reflected automatically.
    inline constexpr size_t MaxReflectedFieldCount = 64;

    /// References to the `Count` fields of an aggregate, as a tuple. `value` may be const.
    template <size_t Count, typename T>
    constexpr auto TieFields(T& value) noexcept
    {
        static_assert(Count <= MaxReflectedFieldCount, "Too many fields for automatic reflection");
        if constexpr (Count == 0)
        {
            (void)value;
            return std::tuple<>();
        }
        else if constexpr (Count == 1)
        {
            auto& [f0] = value;
            return std::tie(f0);
        }
        else if constexpr (Count == 2)
        {
            auto& [f0, f1] = value;
            return std::tie(f0, f1);
        }
        else if constexpr (Count == 3)
        {
            auto& [f0, f1, f2] = value;
            return std::tie(f0, f1, f2);
        }
        else if constexpr (Count == 4)
        {
            auto& [f0, f1, f2, f3] = value;
            return std::tie(f0, f1, f2, f3);
        }
        else if constexpr (Count == 5)
        {
            auto& [f0, f1, f2, f3, f4] = value;
            return std::tie(f0, f1, f2, f3, f4);
        }
        else if constexpr (Count == 6)
        {
            auto& [f0, f1, f2, f3, f4, f5] = value;
            return std::tie(f0, f1, f2, f3, f4, f5);
        }
        else if constexpr (Count == 7)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6);
        }
        else if constexpr (Count == 8)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7);
        }
        else if constexpr (Count == 9)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8);
        }
        else if constexpr (Count == 10)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9);
        }
        else if constexpr (Count == 11)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10);
        }
        else if constexpr (Count == 12)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11);
        }
        else if constexpr (Count == 13)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12);
        }
        else if constexpr (Count == 14)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13);
        }
        else if constexpr (Count == 15)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14);
        }
        else if constexpr (Count == 16)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15);
        }
        else if constexpr (Count == 17)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16);
        }
        else if constexpr (Count == 18)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17);
        }
        else if constexpr (Count == 19)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18);
        }
        else if constexpr (Count == 20)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19);
        }
        else if constexpr (Count == 21)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20] =
                value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20);
        }
        else if constexpr (Count == 22)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21] =
                value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21);
        }
        else if constexpr (Count == 23)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22);
        }
        else if constexpr (Count == 24)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23);
        }
        else if constexpr (Count == 25)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24);
        }
        else if constexpr (Count == 26)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25);
        }
        else if constexpr (Count == 27)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26);
        }
        else if constexpr (Count == 28)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27);
        }
        else if constexpr (Count == 29)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28);
        }
        else if constexpr (Count == 30)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29);
        }
        else if constexpr (Count == 31)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30);
        }
        else if constexpr (Count == 32)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31);
        }
        else if constexpr (Count == 33)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32);
        }
        else if constexpr (Count == 34)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33);
        }
        else if constexpr (Count == 35)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34);
        }
        else if constexpr (Count == 36)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35);
        }
        else if constexpr (Count == 37)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36);
        }
        else if constexpr (Count == 38)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37);
        }
        else if constexpr (Count == 39)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38);
        }
        else if constexpr (Count == 40)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39);
        }
        else if constexpr (Count == 41)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40] =
                value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40);
        }
        else if constexpr (Count == 42)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41] =
                value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41);
        }
        else if constexpr (Count == 43)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42);
        }
        else if constexpr (Count == 44)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43);
        }
        else if constexpr (Count == 45)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44);
        }
        else if constexpr (Count == 46)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45);
        }
        else if constexpr (Count == 47)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46);
        }
        else if constexpr (Count == 48)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47);
        }
        else if constexpr (Count == 49)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48);
        }
        else if constexpr (Count == 50)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49);
        }
        else if constexpr (Count == 51)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50);
        }
        else if constexpr (Count == 52)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50, f51] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51);
        }
        else if constexpr (Count == 53)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52);
        }
        else if constexpr (Count == 54)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53);
        }
        else if constexpr (Count == 55)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54);
        }
        else if constexpr (Count == 56)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55);
        }
        else if constexpr (Count == 57)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55,
                            f56);
        }
        else if constexpr (Count == 58)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55,
                            f56, f57);
        }
        else if constexpr (Count == 59)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55,
                            f56, f57, f58);
        }
        else if constexpr (Count == 60)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55,
                            f56, f57, f58, f59);
        }
        else if constexpr (Count == 61)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60] =
                value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55,
                            f56, f57, f58, f59, f60);
        }
        else if constexpr (Count == 62)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61] =
                value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55,
                            f56, f57, f58, f59, f60, f61);
        }
        else if constexpr (Count == 63)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61,
                   f62] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55,
                            f56, f57, f58, f59, f60, f61, f62);
        }
        else if constexpr (Count == 64)
        {
            auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                   f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                   f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61,
                   f62, f63] = value;
            return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19,
                            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37,
                            f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55,
                            f56, f57, f58, f59, f60, f61, f62, f63);
        }
    }
} // namespace Carbon::Internal
