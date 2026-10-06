#include "Carbon/Backends/OpenGL/OpenGLRendererInternal.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

#include "Carbon/Core/Log.h"

namespace Carbon::Internal
{
    // Generated from Shaders/Carbon.vert and Shaders/Carbon.frag by EmbedAsset.cmake.
    extern const unsigned char g_OpenGLVertexShaderData[];
    extern const unsigned long long g_OpenGLVertexShaderSize;
    extern const unsigned char g_OpenGLFragmentShaderData[];
    extern const unsigned long long g_OpenGLFragmentShaderSize;

    namespace
    {
        constexpr GLsizeiptr MinimumBufferSize = 16 * 1024;
        // Texture units: the draw command's texture, and the primitive buffer.
        constexpr GLuint ColorUnit = 0;
        constexpr GLuint PrimitiveUnit = 1;

        std::string_view GetSource(const unsigned char* data, unsigned long long size)
        {
            return {reinterpret_cast<const char*>(data), static_cast<size_t>(size)};
        }

        GLuint CompileShader(const OpenGLFunctions& gl, GLenum type, std::string_view source)
        {
            const GLuint shader = gl.CreateShader(type);
            const GLchar* text = source.data();
            const GLint length = static_cast<GLint>(source.size());
            gl.ShaderSource(shader, 1, &text, &length);
            gl.CompileShader(shader);
            GLint isCompiled = GL::False;
            gl.GetShaderiv(shader, GL::CompileStatus, &isCompiled);
            if (isCompiled == GL::False)
            {
                GLint logLength = 0;
                gl.GetShaderiv(shader, GL::InfoLogLength, &logLength);
                std::string log(static_cast<size_t>(std::max(logLength, 1)), '\0');
                gl.GetShaderInfoLog(shader, logLength, nullptr, log.data());
                CB_LOG_ERROR("OpenGL", "The {} shader did not compile: {}",
                             type == GL::VertexShader ? "vertex" : "fragment", log);
                gl.DeleteShader(shader);
                return 0;
            }
            return shader;
        }
    } // namespace

    OpenGLRenderer::OpenGLRenderer(const OpenGLFunctions& gl, TextureFormat colorFormat)
        : m_GL(gl), m_IsLinearOutput(IsSrgbFormat(colorFormat))
    {
        if (!CreateProgram())
            return;

        // The objects are created with the host's bindings saved and restored around them.
        SavedState state;
        SaveState(state);

        m_GL.GenVertexArrays(1, &m_VertexArray);
        GLuint buffers[3] = {};
        m_GL.GenBuffers(3, buffers);
        m_VertexBuffer = buffers[0];
        m_IndexBuffer = buffers[1];
        m_PrimitiveBuffer = buffers[2];

        // Vertex layout: mirrors DrawVertex. The element buffer binding is part of the vertex array.
        static_assert(sizeof(DrawVertex) == 32, "DrawVertex must match the vertex layout below");
        static_assert(sizeof(DrawPrimitive) == 32, "DrawPrimitive must be two RGBA32UI texels");
        m_GL.BindVertexArray(m_VertexArray);
        m_GL.BindBuffer(GL::ArrayBuffer, m_VertexBuffer);
        m_GL.BindBuffer(GL::ElementArrayBuffer, m_IndexBuffer);
        for (GLuint attribute = 0; attribute < 5; attribute++)
            m_GL.EnableVertexAttribArray(attribute);
        const GLsizei stride = sizeof(DrawVertex);
        m_GL.VertexAttribPointer(0, 2, GL::Float, GL::False, stride,
                                 reinterpret_cast<const void*>(offsetof(DrawVertex, Position)));
        m_GL.VertexAttribPointer(1, 2, GL::Float, GL::False, stride,
                                 reinterpret_cast<const void*>(offsetof(DrawVertex, Local)));
        m_GL.VertexAttribPointer(2, 2, GL::Float, GL::False, stride,
                                 reinterpret_cast<const void*>(offsetof(DrawVertex, UV)));
        m_GL.VertexAttribPointer(3, 4, GL::UnsignedByte, GL::True, stride,
                                 reinterpret_cast<const void*>(offsetof(DrawVertex, Color)));
        m_GL.VertexAttribIPointer(4, 1, GL::UnsignedInt, stride,
                                  reinterpret_cast<const void*>(offsetof(DrawVertex, Primitive)));
        m_GL.BindVertexArray(0);

        m_GL.ActiveTexture(GL::Texture0 + PrimitiveUnit);
        m_GL.GenTextures(1, &m_PrimitiveTexture);

        m_GL.GenSamplers(1, &m_Sampler);
        m_GL.SamplerParameteri(m_Sampler, GL::TextureMinFilter, static_cast<GLint>(GL::Linear));
        m_GL.SamplerParameteri(m_Sampler, GL::TextureMagFilter, static_cast<GLint>(GL::Linear));
        m_GL.SamplerParameteri(m_Sampler, GL::TextureWrapS, static_cast<GLint>(GL::ClampToEdge));
        m_GL.SamplerParameteri(m_Sampler, GL::TextureWrapT, static_cast<GLint>(GL::ClampToEdge));

        RestoreState(state);
    }

    OpenGLRenderer::~OpenGLRenderer()
    {
        if (m_Program == 0)
            return;
        m_GL.DeleteProgram(m_Program);
        m_GL.DeleteVertexArrays(1, &m_VertexArray);
        const GLuint buffers[3] = {m_VertexBuffer, m_IndexBuffer, m_PrimitiveBuffer};
        m_GL.DeleteBuffers(3, buffers);
        m_GL.DeleteTextures(1, &m_PrimitiveTexture);
        if (m_AtlasTexture != 0)
            m_GL.DeleteTextures(1, &m_AtlasTexture);
        m_GL.DeleteSamplers(1, &m_Sampler);
    }

    bool OpenGLRenderer::CreateProgram()
    {
        const GLuint vertex =
            CompileShader(m_GL, GL::VertexShader, GetSource(g_OpenGLVertexShaderData, g_OpenGLVertexShaderSize));
        const GLuint fragment =
            CompileShader(m_GL, GL::FragmentShader, GetSource(g_OpenGLFragmentShaderData, g_OpenGLFragmentShaderSize));
        if (vertex == 0 || fragment == 0)
        {
            if (vertex != 0)
                m_GL.DeleteShader(vertex);
            if (fragment != 0)
                m_GL.DeleteShader(fragment);
            return false;
        }

        const GLuint program = m_GL.CreateProgram();
        m_GL.AttachShader(program, vertex);
        m_GL.AttachShader(program, fragment);
        m_GL.LinkProgram(program);
        m_GL.DeleteShader(vertex);
        m_GL.DeleteShader(fragment);
        GLint isLinked = GL::False;
        m_GL.GetProgramiv(program, GL::LinkStatus, &isLinked);
        if (isLinked == GL::False)
        {
            GLint logLength = 0;
            m_GL.GetProgramiv(program, GL::InfoLogLength, &logLength);
            std::string log(static_cast<size_t>(std::max(logLength, 1)), '\0');
            m_GL.GetProgramInfoLog(program, logLength, nullptr, log.data());
            CB_LOG_ERROR("OpenGL", "The shader program did not link: {}", log);
            m_GL.DeleteProgram(program);
            return false;
        }

        // GLSL 3.30 cannot name texture units in the shader; they are set once here.
        GLint previousProgram = 0;
        m_GL.GetIntegerv(GL::CurrentProgram, &previousProgram);
        m_GL.UseProgram(program);
        m_GL.Uniform1i(m_GL.GetUniformLocation(program, "ColorTexture"), static_cast<GLint>(ColorUnit));
        m_GL.Uniform1i(m_GL.GetUniformLocation(program, "Primitives"), static_cast<GLint>(PrimitiveUnit));
        m_GL.UseProgram(static_cast<GLuint>(previousProgram));
        m_FrameLocation = m_GL.GetUniformLocation(program, "Frame");
        m_Program = program;
        return true;
    }

    void OpenGLRenderer::SaveState(SavedState& state) const
    {
        m_GL.GetIntegerv(GL::CurrentProgram, &state.Program);
        m_GL.GetIntegerv(GL::VertexArrayBinding, &state.VertexArray);
        m_GL.GetIntegerv(GL::ArrayBufferBinding, &state.ArrayBuffer);
        m_GL.GetIntegerv(GL::ActiveTexture, &state.ActiveTexture);
        for (GLuint unit = 0; unit < 2; unit++)
        {
            m_GL.ActiveTexture(GL::Texture0 + unit);
            m_GL.GetIntegerv(GL::TextureBinding2D, &state.Texture2D[unit]);
            m_GL.GetIntegerv(GL::TextureBindingBuffer, &state.TextureBuffer[unit]);
            m_GL.GetIntegerv(GL::SamplerBinding, &state.Sampler[unit]);
        }
        m_GL.ActiveTexture(static_cast<GLenum>(state.ActiveTexture));
        m_GL.GetIntegerv(GL::Viewport, state.Viewport);
        m_GL.GetIntegerv(GL::ScissorBox, state.ScissorBox);
        m_GL.GetIntegerv(GL::BlendEquationRgb, &state.BlendEquationRgb);
        m_GL.GetIntegerv(GL::BlendEquationAlpha, &state.BlendEquationAlpha);
        m_GL.GetIntegerv(GL::BlendSrcRgb, &state.BlendSrcRgb);
        m_GL.GetIntegerv(GL::BlendDstRgb, &state.BlendDstRgb);
        m_GL.GetIntegerv(GL::BlendSrcAlpha, &state.BlendSrcAlpha);
        m_GL.GetIntegerv(GL::BlendDstAlpha, &state.BlendDstAlpha);
        m_GL.GetIntegerv(GL::PolygonMode, state.PolygonMode);
        m_GL.GetBooleanv(GL::ColorWritemask, state.ColorMask);
        m_GL.GetBooleanv(GL::DepthWritemask, &state.DepthMask);
        state.Blend = m_GL.IsEnabled(GL::Blend);
        state.ScissorTest = m_GL.IsEnabled(GL::ScissorTest);
        state.CullFace = m_GL.IsEnabled(GL::CullFace);
        state.DepthTest = m_GL.IsEnabled(GL::DepthTest);
        state.StencilTest = m_GL.IsEnabled(GL::StencilTest);
        state.PrimitiveRestart = m_GL.IsEnabled(GL::PrimitiveRestart);
        state.ColorLogicOp = m_GL.IsEnabled(GL::ColorLogicOp);
        state.FramebufferSrgb = m_GL.IsEnabled(GL::FramebufferSrgb);
    }

    void OpenGLRenderer::RestoreState(const SavedState& state) const
    {
        m_GL.UseProgram(static_cast<GLuint>(state.Program));
        m_GL.BindVertexArray(static_cast<GLuint>(state.VertexArray));
        m_GL.BindBuffer(GL::ArrayBuffer, static_cast<GLuint>(state.ArrayBuffer));
        for (GLuint unit = 0; unit < 2; unit++)
        {
            m_GL.ActiveTexture(GL::Texture0 + unit);
            m_GL.BindTexture(GL::Texture2D, static_cast<GLuint>(state.Texture2D[unit]));
            m_GL.BindTexture(GL::TextureBuffer, static_cast<GLuint>(state.TextureBuffer[unit]));
            m_GL.BindSampler(unit, static_cast<GLuint>(state.Sampler[unit]));
        }
        m_GL.ActiveTexture(static_cast<GLenum>(state.ActiveTexture));
        m_GL.Viewport(state.Viewport[0], state.Viewport[1], state.Viewport[2], state.Viewport[3]);
        m_GL.Scissor(state.ScissorBox[0], state.ScissorBox[1], state.ScissorBox[2], state.ScissorBox[3]);
        m_GL.BlendEquationSeparate(static_cast<GLenum>(state.BlendEquationRgb),
                                   static_cast<GLenum>(state.BlendEquationAlpha));
        m_GL.BlendFuncSeparate(static_cast<GLenum>(state.BlendSrcRgb), static_cast<GLenum>(state.BlendDstRgb),
                               static_cast<GLenum>(state.BlendSrcAlpha), static_cast<GLenum>(state.BlendDstAlpha));
        // Core profiles only have one polygon mode for both faces.
        m_GL.PolygonMode(GL::FrontAndBack, static_cast<GLenum>(state.PolygonMode[0]));
        m_GL.ColorMask(state.ColorMask[0], state.ColorMask[1], state.ColorMask[2], state.ColorMask[3]);
        m_GL.DepthMask(state.DepthMask);
        SetEnabled(GL::Blend, state.Blend != GL::False);
        SetEnabled(GL::ScissorTest, state.ScissorTest != GL::False);
        SetEnabled(GL::CullFace, state.CullFace != GL::False);
        SetEnabled(GL::DepthTest, state.DepthTest != GL::False);
        SetEnabled(GL::StencilTest, state.StencilTest != GL::False);
        SetEnabled(GL::PrimitiveRestart, state.PrimitiveRestart != GL::False);
        SetEnabled(GL::ColorLogicOp, state.ColorLogicOp != GL::False);
        SetEnabled(GL::FramebufferSrgb, state.FramebufferSrgb != GL::False);
    }

    void OpenGLRenderer::SetEnabled(GLenum capability, bool enabled) const
    {
        if (enabled)
            m_GL.Enable(capability);
        else
            m_GL.Disable(capability);
    }

    void OpenGLRenderer::EnsureBuffer(GLuint buffer, GLsizeiptr& capacity, GLsizeiptr requiredSize) const
    {
        // Grow geometrically so steady-state frames never reallocate.
        if (capacity < requiredSize)
        {
            GLsizeiptr newCapacity = std::max(capacity * 2, MinimumBufferSize);
            while (newCapacity < requiredSize)
                newCapacity *= 2;
            capacity = newCapacity;
        }
        // Orphaning the storage every frame lets the driver hand out fresh memory instead of waiting until the
        // GPU has finished reading the previous frame's.
        m_GL.BindBuffer(GL::ArrayBuffer, buffer);
        m_GL.BufferData(GL::ArrayBuffer, capacity, nullptr, GL::StreamDraw);
    }

    RendererBackendCapabilities OpenGLRenderer::GetCapabilities() const
    {
        GLint size = 0;
        m_GL.GetIntegerv(GL::MaxTextureSize, &size);
        RendererBackendCapabilities capabilities;
        if (size > 0)
            capabilities.MaxTextureSize = static_cast<uint32_t>(size);
        return capabilities;
    }

    void OpenGLRenderer::UpdateGlyphAtlas(const GlyphAtlasUpdate& update)
    {
        SavedUnpackState state;
        m_GL.GetIntegerv(GL::PixelUnpackBufferBinding, &state.Buffer);
        m_GL.GetIntegerv(GL::UnpackAlignment, &state.Alignment);
        m_GL.GetIntegerv(GL::UnpackRowLength, &state.RowLength);
        m_GL.GetIntegerv(GL::UnpackSkipRows, &state.SkipRows);
        m_GL.GetIntegerv(GL::UnpackSkipPixels, &state.SkipPixels);
        m_GL.GetIntegerv(GL::UnpackImageHeight, &state.ImageHeight);
        m_GL.GetIntegerv(GL::UnpackSkipImages, &state.SkipImages);
        m_GL.GetIntegerv(GL::UnpackSwapBytes, &state.SwapBytes);
        m_GL.GetIntegerv(GL::UnpackLsbFirst, &state.LsbFirst);
        m_GL.GetIntegerv(GL::ActiveTexture, &state.ActiveTexture);
        m_GL.ActiveTexture(GL::Texture0 + ColorUnit);
        m_GL.GetIntegerv(GL::TextureBinding2D, &state.Texture2D);

        m_GL.BindBuffer(GL::PixelUnpackBuffer, 0);
        m_GL.PixelStorei(GL::UnpackAlignment, 1);
        m_GL.PixelStorei(GL::UnpackRowLength, 0);
        m_GL.PixelStorei(GL::UnpackSkipRows, 0);
        m_GL.PixelStorei(GL::UnpackSkipPixels, 0);
        m_GL.PixelStorei(GL::UnpackImageHeight, 0);
        m_GL.PixelStorei(GL::UnpackSkipImages, 0);
        m_GL.PixelStorei(GL::UnpackSwapBytes, 0);
        m_GL.PixelStorei(GL::UnpackLsbFirst, 0);

        // A full update may come with a new size; the changed rows of a partial one fit the texture there is.
        if (m_AtlasTexture == 0 || m_AtlasWidth != update.Width || m_AtlasHeight != update.Height)
        {
            if (m_AtlasTexture == 0)
                m_GL.GenTextures(1, &m_AtlasTexture);
            m_GL.BindTexture(GL::Texture2D, m_AtlasTexture);
            m_GL.TexParameteri(GL::Texture2D, GL::TextureMaxLevel, 0);
            m_GL.TexImage2D(GL::Texture2D, 0, static_cast<GLint>(GL::R8), static_cast<GLsizei>(update.Width),
                            static_cast<GLsizei>(update.Height), 0, GL::Red, GL::UnsignedByte, update.Pixels.data());
            m_AtlasWidth = update.Width;
            m_AtlasHeight = update.Height;
        }
        else if (update.RowCount > 0)
        {
            m_GL.BindTexture(GL::Texture2D, m_AtlasTexture);
            const uint8_t* rows = update.Pixels.data() + static_cast<size_t>(update.FirstRow) * update.Width;
            m_GL.TexSubImage2D(GL::Texture2D, 0, 0, static_cast<GLint>(update.FirstRow),
                               static_cast<GLsizei>(update.Width), static_cast<GLsizei>(update.RowCount), GL::Red,
                               GL::UnsignedByte, rows);
        }

        m_GL.BindTexture(GL::Texture2D, static_cast<GLuint>(state.Texture2D));
        m_GL.ActiveTexture(static_cast<GLenum>(state.ActiveTexture));
        m_GL.BindBuffer(GL::PixelUnpackBuffer, static_cast<GLuint>(state.Buffer));
        m_GL.PixelStorei(GL::UnpackAlignment, state.Alignment);
        m_GL.PixelStorei(GL::UnpackRowLength, state.RowLength);
        m_GL.PixelStorei(GL::UnpackSkipRows, state.SkipRows);
        m_GL.PixelStorei(GL::UnpackSkipPixels, state.SkipPixels);
        m_GL.PixelStorei(GL::UnpackImageHeight, state.ImageHeight);
        m_GL.PixelStorei(GL::UnpackSkipImages, state.SkipImages);
        m_GL.PixelStorei(GL::UnpackSwapBytes, state.SwapBytes);
        m_GL.PixelStorei(GL::UnpackLsbFirst, state.LsbFirst);
    }

    void OpenGLRenderer::Render(const DrawData& drawData)
    {
        SavedState state;
        SaveState(state);

        // Buffers: vertices and indices through the vertex array, primitives through the texture buffer.
        const GLsizeiptr vertexBytes = static_cast<GLsizeiptr>(drawData.Vertices.size_bytes());
        const GLsizeiptr indexBytes = static_cast<GLsizeiptr>(drawData.Indices.size_bytes());
        const GLsizeiptr primitiveBytes =
            static_cast<GLsizeiptr>(std::max(drawData.Primitives.size_bytes(), sizeof(DrawPrimitive)));
        m_GL.BindVertexArray(m_VertexArray);
        EnsureBuffer(m_VertexBuffer, m_VertexCapacity, vertexBytes);
        m_GL.BufferSubData(GL::ArrayBuffer, 0, vertexBytes, drawData.Vertices.data());
        EnsureBuffer(m_PrimitiveBuffer, m_PrimitiveCapacity, primitiveBytes);
        if (!drawData.Primitives.empty())
        {
            m_GL.BufferSubData(GL::ArrayBuffer, 0, static_cast<GLsizeiptr>(drawData.Primitives.size_bytes()),
                               drawData.Primitives.data());
        }
        // The element buffer is bound to the vertex array, so binding it here changes nothing of the host's.
        m_GL.BindBuffer(GL::ElementArrayBuffer, m_IndexBuffer);
        if (m_IndexCapacity < indexBytes)
        {
            m_IndexCapacity = std::max(m_IndexCapacity * 2, MinimumBufferSize);
            while (m_IndexCapacity < indexBytes)
                m_IndexCapacity *= 2;
        }
        m_GL.BufferData(GL::ElementArrayBuffer, m_IndexCapacity, nullptr, GL::StreamDraw);
        m_GL.BufferSubData(GL::ElementArrayBuffer, 0, indexBytes, drawData.Indices.data());

        m_GL.ActiveTexture(GL::Texture0 + PrimitiveUnit);
        m_GL.BindTexture(GL::TextureBuffer, m_PrimitiveTexture);
        m_GL.TexBuffer(GL::TextureBuffer, GL::Rgba32ui, m_PrimitiveBuffer);
        m_GL.BindSampler(PrimitiveUnit, 0);
        m_GL.ActiveTexture(GL::Texture0 + ColorUnit);
        m_GL.BindSampler(ColorUnit, m_Sampler);

        // Fixed state for Carbon's draws.
        const float scale = drawData.ContentScale;
        const GLsizei targetWidth = static_cast<GLsizei>(std::lround(drawData.DisplaySize.X * scale));
        const GLsizei targetHeight = static_cast<GLsizei>(std::lround(drawData.DisplaySize.Y * scale));
        m_GL.UseProgram(m_Program);
        m_GL.Uniform4f(m_FrameLocation, drawData.DisplaySize.X, drawData.DisplaySize.Y, scale,
                       m_IsLinearOutput ? 1.0f : 0.0f);
        m_GL.Viewport(0, 0, targetWidth, targetHeight);
        m_GL.Enable(GL::Blend);
        m_GL.BlendEquationSeparate(GL::FuncAdd, GL::FuncAdd);
        m_GL.BlendFuncSeparate(GL::One, GL::OneMinusSrcAlpha, GL::One, GL::OneMinusSrcAlpha);
        m_GL.Enable(GL::ScissorTest);
        m_GL.Disable(GL::CullFace);
        m_GL.Disable(GL::DepthTest);
        m_GL.Disable(GL::StencilTest);
        m_GL.Disable(GL::PrimitiveRestart);
        m_GL.Disable(GL::ColorLogicOp);
        m_GL.PolygonMode(GL::FrontAndBack, GL::Fill);
        m_GL.ColorMask(GL::True, GL::True, GL::True, GL::True);
        // sRGB targets convert Carbon's linear output back to sRGB; others take the values as they are.
        SetEnabled(GL::FramebufferSrgb, m_IsLinearOutput);

        GLuint boundTexture = 0;
        for (const DrawCommand& command : drawData.Commands)
        {
            // Clip rectangles are in points with y down; OpenGL's scissor is in pixels with y up.
            const float left = std::clamp(std::floor(command.ClipRect.X * scale + 0.5f), 0.0f, float(targetWidth));
            const float top = std::clamp(std::floor(command.ClipRect.Y * scale + 0.5f), 0.0f, float(targetHeight));
            const float right =
                std::clamp(std::floor(command.ClipRect.GetRight() * scale + 0.5f), 0.0f, float(targetWidth));
            const float bottom =
                std::clamp(std::floor(command.ClipRect.GetBottom() * scale + 0.5f), 0.0f, float(targetHeight));
            if (right <= left || bottom <= top)
                continue;

            GLuint texture = m_AtlasTexture;
            if (command.Texture != TextureID())
            {
                // Registered or not (MakeTextureID), a host texture is its GLuint name; nothing else is kept.
                if (command.Texture.Value > UINT32_MAX)
                {
                    CB_LOG_WARNING("Renderer",
                                   "Draw command uses a texture ID that is no OpenGL texture name; skipped");
                    continue;
                }
                texture = static_cast<GLuint>(command.Texture.Value);
            }
            if (texture != boundTexture)
            {
                m_GL.BindTexture(GL::Texture2D, texture);
                boundTexture = texture;
            }

            m_GL.Scissor(static_cast<GLint>(left), targetHeight - static_cast<GLint>(bottom),
                         static_cast<GLsizei>(right - left), static_cast<GLsizei>(bottom - top));
            m_GL.DrawElements(
                GL::Triangles, static_cast<GLsizei>(command.IndexCount), GL::UnsignedInt,
                reinterpret_cast<const void*>(static_cast<uintptr_t>(command.IndexOffset) * sizeof(DrawIndex)));
        }

        RestoreState(state);
    }

    TextureID OpenGLRenderer::RegisterTexture(GLuint texture)
    {
        return RegisterHostTexture(MakeTextureID(texture).Value);
    }

    void OpenGLRenderer::ReleaseTexture(TextureID texture)
    {
        // The texture belongs to the host, and Carbon keeps nothing for it.
        static_cast<void>(texture);
    }
} // namespace Carbon::Internal
