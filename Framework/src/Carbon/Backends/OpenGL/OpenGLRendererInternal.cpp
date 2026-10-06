#include "Carbon/Backends/OpenGL/OpenGLRendererInternal.h"

#include <algorithm>
#include <cmath>
#include <memory>
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
        // Texture units: the draw command's texture, and the primitives.
        constexpr GLuint ColorUnit = 0;
        constexpr GLuint PrimitiveUnit = 1;
        // Primitives per row of the primitive texture; two RGBA32UI texels each. 2048 texels is the smallest
        // maximum texture size OpenGL ES 3.0 allows. Keep in step with the shader.
        constexpr GLsizei PrimitivesPerRow = 1024;
        constexpr GLsizei PrimitiveTextureWidth = PrimitivesPerRow * 2;

        // The shaders' first lines: the GLSL version, and for ES the default precisions.
        constexpr std::string_view DesktopHeader = "#version 330 core\n";
        constexpr std::string_view ESHeader =
            "#version 300 es\n"
            "precision highp float;\n"
            "precision highp int;\n"
            "precision highp sampler2D;\n"
            "precision highp usampler2D;\n";

        std::string_view GetSource(const unsigned char* data, unsigned long long size)
        {
            return {reinterpret_cast<const char*>(data), static_cast<size_t>(size)};
        }

        GLuint CompileShader(const OpenGLFunctions& gl, GLenum type, std::string_view header, std::string_view body)
        {
            const GLuint shader = gl.CreateShader(type);
            const GLchar* texts[2] = {header.data(), body.data()};
            const GLint lengths[2] = {static_cast<GLint>(header.size()), static_cast<GLint>(body.size())};
            gl.ShaderSource(shader, 2, texts, lengths);
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

    bool OpenGLRenderer::Install(OpenGLProcLoader getProcAddress, TextureFormat colorFormat, bool isES)
    {
        const char* const api = isES ? "OpenGL ES" : "OpenGL";
        if (getProcAddress == nullptr)
        {
            CB_LOG_ERROR("OpenGL", "{} needs a GetProcAddress function", isES ? "OpenGLESInit" : "OpenGLInit");
            return false;
        }
        if (!IsColorFormat(colorFormat))
        {
            CB_LOG_ERROR("OpenGL", "Unsupported color format {}", ToString(colorFormat));
            return false;
        }

        OpenGLFunctions functions;
        std::string_view missing;
        if (!functions.Load(getProcAddress, isES, missing))
        {
            CB_LOG_ERROR("OpenGL", "The {} function {} could not be resolved; is a {} {} context current?", api,
                         missing, api, isES ? "3.0" : "3.3");
            return false;
        }
        GLint major = 0;
        GLint minor = 0;
        functions.GetIntegerv(GL::MajorVersion, &major);
        functions.GetIntegerv(GL::MinorVersion, &minor);
        const GLint required = isES ? 30 : 33;
        if (major * 10 + minor < required)
        {
            CB_LOG_ERROR("OpenGL", "The {} backend needs {} {}.{}; the context has {}.{}", api, api, required / 10,
                         required % 10, major, minor);
            return false;
        }

        if (isES)
        {
            std::unique_ptr<OpenGLESRenderer> renderer =
                std::make_unique<OpenGLESRenderer>(functions, colorFormat, true);
            return renderer->IsValid() && InstallRendererBackend(std::move(renderer));
        }
        std::unique_ptr<OpenGLRenderer> renderer = std::make_unique<OpenGLRenderer>(functions, colorFormat, false);
        return renderer->IsValid() && InstallRendererBackend(std::move(renderer));
    }

    OpenGLRenderer::OpenGLRenderer(const OpenGLFunctions& gl, TextureFormat colorFormat, bool isES)
        : m_GL(gl), m_IsLinearOutput(IsSrgbFormat(colorFormat)), m_IsES(isES)
    {
        if (!CreateProgram())
            return;

        // The objects are created with the host's bindings saved and restored around them.
        SavedState state;
        SaveState(state);

        m_GL.GenVertexArrays(1, &m_VertexArray);
        GLuint buffers[2] = {};
        m_GL.GenBuffers(2, buffers);
        m_VertexBuffer = buffers[0];
        m_IndexBuffer = buffers[1];

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

        // An integer texture is complete only with nearest filtering; texelFetch ignores filters otherwise.
        m_GL.ActiveTexture(GL::Texture0 + PrimitiveUnit);
        m_GL.GenTextures(1, &m_PrimitiveTexture);
        m_GL.BindTexture(GL::Texture2D, m_PrimitiveTexture);
        m_GL.TexParameteri(GL::Texture2D, GL::TextureMinFilter, static_cast<GLint>(GL::Nearest));
        m_GL.TexParameteri(GL::Texture2D, GL::TextureMagFilter, static_cast<GLint>(GL::Nearest));
        m_GL.TexParameteri(GL::Texture2D, GL::TextureMaxLevel, 0);

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
        const GLuint buffers[2] = {m_VertexBuffer, m_IndexBuffer};
        m_GL.DeleteBuffers(2, buffers);
        m_GL.DeleteTextures(1, &m_PrimitiveTexture);
        if (m_AtlasTexture != 0)
            m_GL.DeleteTextures(1, &m_AtlasTexture);
        m_GL.DeleteSamplers(1, &m_Sampler);
    }

    bool OpenGLRenderer::CreateProgram()
    {
        const std::string_view header = m_IsES ? ESHeader : DesktopHeader;
        const GLuint vertex = CompileShader(m_GL, GL::VertexShader, header,
                                            GetSource(g_OpenGLVertexShaderData, g_OpenGLVertexShaderSize));
        const GLuint fragment = CompileShader(m_GL, GL::FragmentShader, header,
                                              GetSource(g_OpenGLFragmentShaderData, g_OpenGLFragmentShaderSize));
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

        // GLSL 3.30 and GLSL ES 3.00 cannot name texture units in the shader; they are set once here.
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
        m_GL.GetIntegerv(GL::ActiveTexture, &state.ActiveTexture);
        for (GLuint unit = 0; unit < 2; unit++)
        {
            m_GL.ActiveTexture(GL::Texture0 + unit);
            m_GL.GetIntegerv(GL::TextureBinding2D, &state.Texture2D[unit]);
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
        m_GL.GetBooleanv(GL::ColorWritemask, state.ColorMask);
        state.Blend = m_GL.IsEnabled(GL::Blend);
        state.ScissorTest = m_GL.IsEnabled(GL::ScissorTest);
        state.CullFace = m_GL.IsEnabled(GL::CullFace);
        state.DepthTest = m_GL.IsEnabled(GL::DepthTest);
        state.StencilTest = m_GL.IsEnabled(GL::StencilTest);
        // OpenGL ES has none of these.
        if (!m_IsES)
        {
            m_GL.GetIntegerv(GL::PolygonMode, state.PolygonMode);
            state.PrimitiveRestart = m_GL.IsEnabled(GL::PrimitiveRestart);
            state.ColorLogicOp = m_GL.IsEnabled(GL::ColorLogicOp);
            state.FramebufferSrgb = m_GL.IsEnabled(GL::FramebufferSrgb);
        }
    }

    void OpenGLRenderer::RestoreState(const SavedState& state) const
    {
        m_GL.UseProgram(static_cast<GLuint>(state.Program));
        m_GL.BindVertexArray(static_cast<GLuint>(state.VertexArray));
        for (GLuint unit = 0; unit < 2; unit++)
        {
            m_GL.ActiveTexture(GL::Texture0 + unit);
            m_GL.BindTexture(GL::Texture2D, static_cast<GLuint>(state.Texture2D[unit]));
            m_GL.BindSampler(unit, static_cast<GLuint>(state.Sampler[unit]));
        }
        m_GL.ActiveTexture(static_cast<GLenum>(state.ActiveTexture));
        m_GL.Viewport(state.Viewport[0], state.Viewport[1], state.Viewport[2], state.Viewport[3]);
        m_GL.Scissor(state.ScissorBox[0], state.ScissorBox[1], state.ScissorBox[2], state.ScissorBox[3]);
        m_GL.BlendEquationSeparate(static_cast<GLenum>(state.BlendEquationRgb),
                                   static_cast<GLenum>(state.BlendEquationAlpha));
        m_GL.BlendFuncSeparate(static_cast<GLenum>(state.BlendSrcRgb), static_cast<GLenum>(state.BlendDstRgb),
                               static_cast<GLenum>(state.BlendSrcAlpha), static_cast<GLenum>(state.BlendDstAlpha));
        m_GL.ColorMask(state.ColorMask[0], state.ColorMask[1], state.ColorMask[2], state.ColorMask[3]);
        SetEnabled(GL::Blend, state.Blend != GL::False);
        SetEnabled(GL::ScissorTest, state.ScissorTest != GL::False);
        SetEnabled(GL::CullFace, state.CullFace != GL::False);
        SetEnabled(GL::DepthTest, state.DepthTest != GL::False);
        SetEnabled(GL::StencilTest, state.StencilTest != GL::False);
        if (!m_IsES)
        {
            // Core profiles only have one polygon mode for both faces.
            m_GL.PolygonMode(GL::FrontAndBack, static_cast<GLenum>(state.PolygonMode[0]));
            SetEnabled(GL::PrimitiveRestart, state.PrimitiveRestart != GL::False);
            SetEnabled(GL::ColorLogicOp, state.ColorLogicOp != GL::False);
            SetEnabled(GL::FramebufferSrgb, state.FramebufferSrgb != GL::False);
        }
    }

    void OpenGLRenderer::SaveUnpackState(SavedUnpackState& state) const
    {
        m_GL.GetIntegerv(GL::PixelUnpackBufferBinding, &state.Buffer);
        m_GL.GetIntegerv(GL::UnpackAlignment, &state.Alignment);
        m_GL.GetIntegerv(GL::UnpackRowLength, &state.RowLength);
        m_GL.GetIntegerv(GL::UnpackSkipRows, &state.SkipRows);
        m_GL.GetIntegerv(GL::UnpackSkipPixels, &state.SkipPixels);
        if (!m_IsES)
            m_GL.GetIntegerv(GL::UnpackSwapBytes, &state.SwapBytes);
    }

    void OpenGLRenderer::SetUnpackDefaults() const
    {
        m_GL.BindBuffer(GL::PixelUnpackBuffer, 0);
        m_GL.PixelStorei(GL::UnpackAlignment, 1);
        m_GL.PixelStorei(GL::UnpackRowLength, 0);
        m_GL.PixelStorei(GL::UnpackSkipRows, 0);
        m_GL.PixelStorei(GL::UnpackSkipPixels, 0);
        if (!m_IsES)
            m_GL.PixelStorei(GL::UnpackSwapBytes, 0);
    }

    void OpenGLRenderer::RestoreUnpackState(const SavedUnpackState& state) const
    {
        m_GL.BindBuffer(GL::PixelUnpackBuffer, static_cast<GLuint>(state.Buffer));
        m_GL.PixelStorei(GL::UnpackAlignment, state.Alignment);
        m_GL.PixelStorei(GL::UnpackRowLength, state.RowLength);
        m_GL.PixelStorei(GL::UnpackSkipRows, state.SkipRows);
        m_GL.PixelStorei(GL::UnpackSkipPixels, state.SkipPixels);
        if (!m_IsES)
            m_GL.PixelStorei(GL::UnpackSwapBytes, state.SwapBytes);
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

    void OpenGLRenderer::UploadPrimitives(const DrawData& drawData)
    {
        // Rows of PrimitivesPerRow primitives; the texture grows by doubling its rows.
        const GLsizei count = static_cast<GLsizei>(drawData.Primitives.size());
        const GLsizei rows = std::max<GLsizei>((count + PrimitivesPerRow - 1) / PrimitivesPerRow, 1);
        m_GL.ActiveTexture(GL::Texture0 + PrimitiveUnit);
        m_GL.BindTexture(GL::Texture2D, m_PrimitiveTexture);
        m_GL.BindSampler(PrimitiveUnit, 0);
        if (m_PrimitiveRows < rows)
        {
            m_PrimitiveRows = std::max(m_PrimitiveRows * 2, 1);
            while (m_PrimitiveRows < rows)
                m_PrimitiveRows *= 2;
            m_GL.TexImage2D(GL::Texture2D, 0, static_cast<GLint>(GL::Rgba32ui), PrimitiveTextureWidth, m_PrimitiveRows,
                            0, GL::RgbaInteger, GL::UnsignedInt, nullptr);
        }
        if (count == 0)
            return;

        // Whole rows, then what is left of the last one.
        const GLsizei fullRows = count / PrimitivesPerRow;
        const GLsizei rest = count % PrimitivesPerRow;
        const DrawPrimitive* primitives = drawData.Primitives.data();
        if (fullRows > 0)
        {
            m_GL.TexSubImage2D(GL::Texture2D, 0, 0, 0, PrimitiveTextureWidth, fullRows, GL::RgbaInteger,
                               GL::UnsignedInt, primitives);
        }
        if (rest > 0)
        {
            m_GL.TexSubImage2D(GL::Texture2D, 0, 0, fullRows, rest * 2, 1, GL::RgbaInteger, GL::UnsignedInt,
                               primitives + static_cast<size_t>(fullRows) * PrimitivesPerRow);
        }
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
        SavedUnpackState unpack;
        SaveUnpackState(unpack);
        GLint activeTexture = 0;
        m_GL.GetIntegerv(GL::ActiveTexture, &activeTexture);
        m_GL.ActiveTexture(GL::Texture0 + ColorUnit);
        GLint boundTexture = 0;
        m_GL.GetIntegerv(GL::TextureBinding2D, &boundTexture);
        SetUnpackDefaults();

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

        m_GL.BindTexture(GL::Texture2D, static_cast<GLuint>(boundTexture));
        m_GL.ActiveTexture(static_cast<GLenum>(activeTexture));
        RestoreUnpackState(unpack);
    }

    void OpenGLRenderer::Render(const DrawData& drawData)
    {
        SavedState state;
        SaveState(state);

        // Vertices and indices through the vertex array, primitives through the primitive texture. They are
        // uploaded once per frame: a frame that is rendered again draws from what is there.
        m_GL.BindVertexArray(m_VertexArray);
        if (m_HasNewDrawData)
        {
            SavedUnpackState unpack;
            SaveUnpackState(unpack);
            SetUnpackDefaults();
            // The array buffer binding is no part of the vertex array's state, and only an upload changes it.
            GLint arrayBuffer = 0;
            m_GL.GetIntegerv(GL::ArrayBufferBinding, &arrayBuffer);
            const GLsizeiptr vertexBytes = static_cast<GLsizeiptr>(drawData.Vertices.size_bytes());
            const GLsizeiptr indexBytes = static_cast<GLsizeiptr>(drawData.Indices.size_bytes());
            EnsureBuffer(m_VertexBuffer, m_VertexCapacity, vertexBytes);
            m_GL.BufferSubData(GL::ArrayBuffer, 0, vertexBytes, drawData.Vertices.data());
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
            UploadPrimitives(drawData);
            m_GL.BindBuffer(GL::ArrayBuffer, static_cast<GLuint>(arrayBuffer));
            RestoreUnpackState(unpack);
            m_HasNewDrawData = false;
        }
        else
        {
            m_GL.ActiveTexture(GL::Texture0 + PrimitiveUnit);
            m_GL.BindTexture(GL::Texture2D, m_PrimitiveTexture);
            m_GL.BindSampler(PrimitiveUnit, 0);
        }

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
        m_GL.ColorMask(GL::True, GL::True, GL::True, GL::True);
        if (!m_IsES)
        {
            m_GL.Disable(GL::PrimitiveRestart);
            m_GL.Disable(GL::ColorLogicOp);
            m_GL.PolygonMode(GL::FrontAndBack, GL::Fill);
            // sRGB targets convert Carbon's linear output back to sRGB; others take the values as they are.
            // OpenGL ES always encodes into sRGB framebuffers, and never into others.
            SetEnabled(GL::FramebufferSrgb, m_IsLinearOutput);
        }

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
