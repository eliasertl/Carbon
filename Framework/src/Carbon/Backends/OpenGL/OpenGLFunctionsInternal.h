#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

// The OpenGL types, constants and functions the OpenGL and OpenGL ES backends use. Carbon includes no GL header and
// links no loader: the host loads OpenGL however it likes, and the backends resolve these functions through the
// host's GetProcAddress into a table of their own. Values are from the OpenGL 3.3 core and OpenGL ES 3.0
// specifications, which agree on every one used by both.

namespace Carbon::Internal
{
    using GLenum = unsigned int;
    using GLboolean = unsigned char;
    using GLbitfield = unsigned int;
    using GLint = int;
    using GLuint = unsigned int;
    using GLsizei = int;
    using GLfloat = float;
    using GLchar = char;
    using GLubyte = unsigned char;
    using GLintptr = std::ptrdiff_t;
    using GLsizeiptr = std::ptrdiff_t;

    namespace GL
    {
        inline constexpr GLenum False = 0;
        inline constexpr GLenum True = 1;
        inline constexpr GLenum NoError = 0;

        inline constexpr GLenum Triangles = 0x0004;
        inline constexpr GLenum UnsignedByte = 0x1401;
        inline constexpr GLenum UnsignedInt = 0x1405;
        inline constexpr GLenum Float = 0x1406;

        inline constexpr GLenum CullFace = 0x0B44;
        inline constexpr GLenum DepthTest = 0x0B71;
        inline constexpr GLenum StencilTest = 0x0B90;
        inline constexpr GLenum Viewport = 0x0BA2;
        inline constexpr GLenum Blend = 0x0BE2;
        inline constexpr GLenum ColorLogicOp = 0x0BF2;
        inline constexpr GLenum ScissorBox = 0x0C10;
        inline constexpr GLenum ScissorTest = 0x0C11;
        inline constexpr GLenum ColorWritemask = 0x0C23;
        inline constexpr GLenum PolygonMode = 0x0B40;
        inline constexpr GLenum FrontAndBack = 0x0408;
        inline constexpr GLenum Fill = 0x1B02;
        inline constexpr GLenum PrimitiveRestart = 0x8F9D;
        inline constexpr GLenum FramebufferSrgb = 0x8DB9;
        inline constexpr GLenum MaxTextureSize = 0x0D33;
        inline constexpr GLenum MajorVersion = 0x821B;
        inline constexpr GLenum MinorVersion = 0x821C;
        inline constexpr GLenum Version = 0x1F02;

        inline constexpr GLenum UnpackSwapBytes = 0x0CF0;
        inline constexpr GLenum UnpackRowLength = 0x0CF2;
        inline constexpr GLenum UnpackSkipRows = 0x0CF3;
        inline constexpr GLenum UnpackSkipPixels = 0x0CF4;
        inline constexpr GLenum UnpackAlignment = 0x0CF5;

        inline constexpr GLenum Texture2D = 0x0DE1;
        inline constexpr GLenum TextureBuffer = 0x8C2A;
        inline constexpr GLenum TextureBinding2D = 0x8069;
        inline constexpr GLenum TextureBindingBuffer = 0x8C2C;
        inline constexpr GLenum Texture0 = 0x84C0;
        inline constexpr GLenum ActiveTexture = 0x84E0;
        inline constexpr GLenum SamplerBinding = 0x8919;
        inline constexpr GLenum TextureMinFilter = 0x2801;
        inline constexpr GLenum TextureMagFilter = 0x2800;
        inline constexpr GLenum TextureWrapS = 0x2802;
        inline constexpr GLenum TextureWrapT = 0x2803;
        inline constexpr GLenum TextureMaxLevel = 0x813D;
        inline constexpr GLenum Linear = 0x2601;
        inline constexpr GLenum ClampToEdge = 0x812F;
        inline constexpr GLenum Red = 0x1903;
        inline constexpr GLenum R8 = 0x8229;
        inline constexpr GLenum Rgba32ui = 0x8D70;
        inline constexpr GLenum RgbaInteger = 0x8D99;
        inline constexpr GLenum Nearest = 0x2600;

        inline constexpr GLenum ArrayBuffer = 0x8892;
        inline constexpr GLenum ArrayBufferBinding = 0x8894;
        inline constexpr GLenum ElementArrayBuffer = 0x8893;
        inline constexpr GLenum PixelUnpackBuffer = 0x88EC;
        inline constexpr GLenum PixelUnpackBufferBinding = 0x88EF;
        inline constexpr GLenum StreamDraw = 0x88E0;
        inline constexpr GLenum VertexArrayBinding = 0x85B5;
        inline constexpr GLenum CurrentProgram = 0x8B8D;

        inline constexpr GLenum FuncAdd = 0x8006;
        inline constexpr GLenum BlendEquationRgb = 0x8009;
        inline constexpr GLenum BlendEquationAlpha = 0x883D;
        inline constexpr GLenum BlendSrcRgb = 0x80C9;
        inline constexpr GLenum BlendDstRgb = 0x80C8;
        inline constexpr GLenum BlendSrcAlpha = 0x80CB;
        inline constexpr GLenum BlendDstAlpha = 0x80CA;
        inline constexpr GLenum One = 1;
        inline constexpr GLenum OneMinusSrcAlpha = 0x0303;

        inline constexpr GLenum FragmentShader = 0x8B30;
        inline constexpr GLenum VertexShader = 0x8B31;
        inline constexpr GLenum CompileStatus = 0x8B81;
        inline constexpr GLenum LinkStatus = 0x8B82;
        inline constexpr GLenum InfoLogLength = 0x8B84;
    } // namespace GL

    // Every function the backend calls: name, return type, parameters.
#define CB_OPENGL_FUNCTIONS(X)                                                                                       \
    X(GetError, GLenum, (void))                                                                                      \
    X(GetIntegerv, void, (GLenum pname, GLint * data))                                                               \
    X(GetBooleanv, void, (GLenum pname, GLboolean * data))                                                           \
    X(GetString, const GLubyte*, (GLenum name))                                                                      \
    X(IsEnabled, GLboolean, (GLenum cap))                                                                            \
    X(Enable, void, (GLenum cap))                                                                                    \
    X(Disable, void, (GLenum cap))                                                                                   \
    X(BlendEquationSeparate, void, (GLenum modeRgb, GLenum modeAlpha))                                               \
    X(BlendFuncSeparate, void, (GLenum srcRgb, GLenum dstRgb, GLenum srcAlpha, GLenum dstAlpha))                     \
    X(ColorMask, void, (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha))                            \
    X(Viewport, void, (GLint x, GLint y, GLsizei width, GLsizei height))                                             \
    X(Scissor, void, (GLint x, GLint y, GLsizei width, GLsizei height))                                              \
    X(PixelStorei, void, (GLenum pname, GLint param))                                                                \
    X(ActiveTexture, void, (GLenum texture))                                                                         \
    X(BindTexture, void, (GLenum target, GLuint texture))                                                            \
    X(GenTextures, void, (GLsizei n, GLuint * textures))                                                             \
    X(DeleteTextures, void, (GLsizei n, const GLuint* textures))                                                     \
    X(TexImage2D, void,                                                                                              \
      (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, \
       GLenum type, const void* pixels))                                                                             \
    X(TexSubImage2D, void,                                                                                           \
      (GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,      \
       const void* pixels))                                                                                          \
    X(TexParameteri, void, (GLenum target, GLenum pname, GLint param))                                               \
    X(GenSamplers, void, (GLsizei n, GLuint * samplers))                                                             \
    X(DeleteSamplers, void, (GLsizei n, const GLuint* samplers))                                                     \
    X(BindSampler, void, (GLuint unit, GLuint sampler))                                                              \
    X(SamplerParameteri, void, (GLuint sampler, GLenum pname, GLint param))                                          \
    X(GenBuffers, void, (GLsizei n, GLuint * buffers))                                                               \
    X(DeleteBuffers, void, (GLsizei n, const GLuint* buffers))                                                       \
    X(BindBuffer, void, (GLenum target, GLuint buffer))                                                              \
    X(BufferData, void, (GLenum target, GLsizeiptr size, const void* data, GLenum usage))                            \
    X(BufferSubData, void, (GLenum target, GLintptr offset, GLsizeiptr size, const void* data))                      \
    X(GenVertexArrays, void, (GLsizei n, GLuint * arrays))                                                           \
    X(DeleteVertexArrays, void, (GLsizei n, const GLuint* arrays))                                                   \
    X(BindVertexArray, void, (GLuint array))                                                                         \
    X(EnableVertexAttribArray, void, (GLuint index))                                                                 \
    X(VertexAttribPointer, void,                                                                                     \
      (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer))            \
    X(VertexAttribIPointer, void, (GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer))      \
    X(CreateShader, GLuint, (GLenum type))                                                                           \
    X(ShaderSource, void, (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length))          \
    X(CompileShader, void, (GLuint shader))                                                                          \
    X(GetShaderiv, void, (GLuint shader, GLenum pname, GLint * params))                                              \
    X(GetShaderInfoLog, void, (GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * infoLog))                  \
    X(DeleteShader, void, (GLuint shader))                                                                           \
    X(CreateProgram, GLuint, (void))                                                                                 \
    X(AttachShader, void, (GLuint program, GLuint shader))                                                           \
    X(LinkProgram, void, (GLuint program))                                                                           \
    X(GetProgramiv, void, (GLuint program, GLenum pname, GLint * params))                                            \
    X(GetProgramInfoLog, void, (GLuint program, GLsizei bufSize, GLsizei * length, GLchar * infoLog))                \
    X(DeleteProgram, void, (GLuint program))                                                                         \
    X(UseProgram, void, (GLuint program))                                                                            \
    X(GetUniformLocation, GLint, (GLuint program, const GLchar* name))                                               \
    X(Uniform1i, void, (GLint location, GLint v0))                                                                   \
    X(Uniform4f, void, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3))                             \
    X(DrawElements, void, (GLenum mode, GLsizei count, GLenum type, const void* indices))

    // On 32-bit Windows, OpenGL functions use the stdcall convention.
#if defined(_WIN32) && !defined(_WIN64)
#define CB_OPENGL_CALL __stdcall
#else
#define CB_OPENGL_CALL
#endif

    // Functions only desktop OpenGL has; for OpenGL ES they stay null.
#define CB_OPENGL_DESKTOP_FUNCTIONS(X) X(PolygonMode, void, (GLenum face, GLenum mode))

    /// A GetProcAddress-style loader, as the public headers of both GL backends declare it.
    using OpenGLProcLoader = void (*(*)(const char* name))();

    /// The table of OpenGL functions the backend resolved through the host's GetProcAddress.
    struct OpenGLFunctions
    {
#define CB_OPENGL_DECLARE(name, result, parameters) result(CB_OPENGL_CALL* name) parameters = nullptr;
        CB_OPENGL_FUNCTIONS(CB_OPENGL_DECLARE)
        CB_OPENGL_DESKTOP_FUNCTIONS(CB_OPENGL_DECLARE)
#undef CB_OPENGL_DECLARE

        /// Resolves every function the API has; for OpenGL ES (`isES`) the desktop-only ones stay null. Returns
        /// false and names the first missing one in `missing`.
        bool Load(OpenGLProcLoader getProcAddress, bool isES, std::string_view& missing);
    };
} // namespace Carbon::Internal
