#include <gtest/gtest.h>

#include <initializer_list>
#include <set>
#include <string>

#include "Carbon/Carbon.h"

namespace Carbon
{
    namespace
    {
        constexpr std::initializer_list<TextureFormat> ColorFormats = {
            TextureFormat::RGBA8Unorm,     TextureFormat::RGBA8UnormSrgb, TextureFormat::BGRA8Unorm,
            TextureFormat::BGRA8UnormSrgb, TextureFormat::RGB10A2Unorm,   TextureFormat::RGBA16Float};
        constexpr std::initializer_list<TextureFormat> DepthStencilFormats = {
            TextureFormat::Depth16Unorm, TextureFormat::Depth24Unorm,         TextureFormat::Depth24UnormStencil8,
            TextureFormat::Depth32Float, TextureFormat::Depth32FloatStencil8, TextureFormat::Stencil8};
    } // namespace

    TEST(TextureFormatTests, UndefinedIsTheDefaultAndBelongsToNoGroup)
    {
        EXPECT_EQ(TextureFormat(), TextureFormat::Undefined);
        EXPECT_FALSE(IsColorFormat(TextureFormat::Undefined));
        EXPECT_FALSE(IsDepthStencilFormat(TextureFormat::Undefined));
        EXPECT_FALSE(IsSrgbFormat(TextureFormat::Undefined));
        EXPECT_FALSE(HasDepth(TextureFormat::Undefined));
        EXPECT_FALSE(HasStencil(TextureFormat::Undefined));
    }

    TEST(TextureFormatTests, EveryFormatIsEitherColorOrDepthStencil)
    {
        for (const TextureFormat format : ColorFormats)
        {
            EXPECT_TRUE(IsColorFormat(format)) << ToString(format);
            EXPECT_FALSE(IsDepthStencilFormat(format)) << ToString(format);
            EXPECT_FALSE(HasDepth(format)) << ToString(format);
            EXPECT_FALSE(HasStencil(format)) << ToString(format);
        }
        for (const TextureFormat format : DepthStencilFormats)
        {
            EXPECT_FALSE(IsColorFormat(format)) << ToString(format);
            EXPECT_TRUE(IsDepthStencilFormat(format)) << ToString(format);
            EXPECT_FALSE(IsSrgbFormat(format)) << ToString(format);
            EXPECT_TRUE(HasDepth(format) || HasStencil(format)) << ToString(format);
        }
    }

    TEST(TextureFormatTests, OnlyTheSrgbFormatsAreSrgb)
    {
        EXPECT_TRUE(IsSrgbFormat(TextureFormat::RGBA8UnormSrgb));
        EXPECT_TRUE(IsSrgbFormat(TextureFormat::BGRA8UnormSrgb));
        EXPECT_FALSE(IsSrgbFormat(TextureFormat::RGBA8Unorm));
        EXPECT_FALSE(IsSrgbFormat(TextureFormat::BGRA8Unorm));
        EXPECT_FALSE(IsSrgbFormat(TextureFormat::RGB10A2Unorm));
        EXPECT_FALSE(IsSrgbFormat(TextureFormat::RGBA16Float));
    }

    TEST(TextureFormatTests, DepthAndStencilAspects)
    {
        EXPECT_TRUE(HasDepth(TextureFormat::Depth16Unorm));
        EXPECT_FALSE(HasStencil(TextureFormat::Depth16Unorm));
        EXPECT_TRUE(HasDepth(TextureFormat::Depth24Unorm));
        EXPECT_FALSE(HasStencil(TextureFormat::Depth24Unorm));
        EXPECT_TRUE(HasDepth(TextureFormat::Depth24UnormStencil8));
        EXPECT_TRUE(HasStencil(TextureFormat::Depth24UnormStencil8));
        EXPECT_TRUE(HasDepth(TextureFormat::Depth32Float));
        EXPECT_FALSE(HasStencil(TextureFormat::Depth32Float));
        EXPECT_TRUE(HasDepth(TextureFormat::Depth32FloatStencil8));
        EXPECT_TRUE(HasStencil(TextureFormat::Depth32FloatStencil8));
        EXPECT_FALSE(HasDepth(TextureFormat::Stencil8));
        EXPECT_TRUE(HasStencil(TextureFormat::Stencil8));
    }

    TEST(TextureFormatTests, NamesAreUnique)
    {
        std::set<std::string> names;
        names.emplace(ToString(TextureFormat::Undefined));
        for (const TextureFormat format : ColorFormats)
            names.emplace(ToString(format));
        for (const TextureFormat format : DepthStencilFormats)
            names.emplace(ToString(format));
        EXPECT_EQ(names.size(), 1 + ColorFormats.size() + DepthStencilFormats.size());
        EXPECT_EQ(names.count("Unknown"), 0u);
        EXPECT_EQ(ToString(TextureFormat::BGRA8Unorm), "BGRA8Unorm");
    }

    TEST(TextureFormatTests, QueriesAreConstantExpressions)
    {
        static_assert(IsSrgbFormat(TextureFormat::BGRA8UnormSrgb));
        static_assert(!IsSrgbFormat(TextureFormat::BGRA8Unorm));
        static_assert(IsDepthStencilFormat(TextureFormat::Depth24UnormStencil8));
        static_assert(HasStencil(TextureFormat::Stencil8) && !HasDepth(TextureFormat::Stencil8));
        SUCCEED();
    }
} // namespace Carbon
