#pragma once

#include <cstdint>

#include "Carbon/Backends/OpenGL/OpenGLFunctionsInternal.h"
#include "Carbon/Renderer/RendererBackend.h"
#include "Carbon/Renderer/TextureFormat.h"

namespace Carbon::Internal
{
    /// The renderer of the OpenGL 3.3 core and OpenGL ES 3.0 backends (ES also covers WebGL 2): the two APIs share
    /// everything Carbon needs except a few pieces of state, which ES mode leaves alone.
    ///
    /// Every call that touches OpenGL saves the state it changes and restores it before returning, so the host's
    /// own rendering is never disturbed. Primitives reach the fragment shader through an RGBA32UI 2D texture, two
    /// texels per primitive, which both APIs can fetch from exactly.
    class OpenGLRenderer : public RendererBackend
    {
    public:
        OpenGLRenderer(const OpenGLFunctions& gl, TextureFormat colorFormat, bool isES);
        ~OpenGLRenderer() override;

        /// False when a shader did not compile or link; the reason was logged.
        bool IsValid() const { return m_Program != 0; }

        std::string_view GetName() const override { return m_IsES ? "OpenGL ES" : "OpenGL"; }
        RendererBackendCapabilities GetCapabilities() const override;
        void EndFrame() override { m_HasNewDrawData = true; }
        void UpdateGlyphAtlas(const GlyphAtlasUpdate& update) override;
        void Render(const DrawData& drawData) override;
        void ReleaseTexture(TextureID texture) override;

        /// Registers a host texture for the current frame and returns its ID.
        TextureID RegisterTexture(GLuint texture);

        /// Resolves the OpenGL functions, checks the version and installs an OpenGLRenderer (desktop) or an
        /// OpenGLESRenderer (`isES`) into the current context. Logs and returns false on failure.
        static bool Install(OpenGLProcLoader getProcAddress, TextureFormat colorFormat, bool isES);

    private:
        /// The OpenGL state Carbon changes, as found before it does.
        struct SavedState
        {
            GLint Program = 0;
            GLint VertexArray = 0;
            GLint ActiveTexture = 0;
            GLint Texture2D[2] = {};
            GLint Sampler[2] = {};
            GLint Viewport[4] = {};
            GLint ScissorBox[4] = {};
            GLint BlendEquationRgb = 0;
            GLint BlendEquationAlpha = 0;
            GLint BlendSrcRgb = 0;
            GLint BlendDstRgb = 0;
            GLint BlendSrcAlpha = 0;
            GLint BlendDstAlpha = 0;
            GLint PolygonMode[2] = {};
            GLboolean ColorMask[4] = {};
            GLboolean Blend = 0;
            GLboolean ScissorTest = 0;
            GLboolean CullFace = 0;
            GLboolean DepthTest = 0;
            GLboolean StencilTest = 0;
            GLboolean PrimitiveRestart = 0;
            GLboolean ColorLogicOp = 0;
            GLboolean FramebufferSrgb = 0;
        };

        /// The pixel-unpack state that a two-dimensional upload of bytes or integers depends on, changed only
        /// while textures are uploaded. The state for three-dimensional images and for bitmaps is left alone:
        /// it has no effect on Carbon's uploads, and every state that is saved is a query, which is a round trip
        /// in a browser.
        struct SavedUnpackState
        {
            GLint Buffer = 0;
            GLint Alignment = 0;
            GLint RowLength = 0;
            GLint SkipRows = 0;
            GLint SkipPixels = 0;
            GLint SwapBytes = 0;
        };

        bool CreateProgram();
        void SaveState(SavedState& state) const;
        void RestoreState(const SavedState& state) const;
        void SaveUnpackState(SavedUnpackState& state) const;
        void SetUnpackDefaults() const;
        void RestoreUnpackState(const SavedUnpackState& state) const;
        void SetEnabled(GLenum capability, bool enabled) const;
        void EnsureBuffer(GLuint buffer, GLsizeiptr& capacity, GLsizeiptr requiredSize) const;
        void UploadPrimitives(const DrawData& drawData);

    private:
        OpenGLFunctions m_GL;
        bool m_IsLinearOutput = false;
        bool m_IsES = false;
        /// Set by EndFrame: the next Render has new draw data to upload. A frame that is rendered again (a window
        /// redrawn without a new frame) reuses what its first Render uploaded.
        bool m_HasNewDrawData = true;

        GLuint m_Program = 0;
        GLint m_FrameLocation = -1;
        GLuint m_VertexArray = 0;
        GLuint m_VertexBuffer = 0;
        GLuint m_IndexBuffer = 0;
        GLsizeiptr m_VertexCapacity = 0;
        GLsizeiptr m_IndexCapacity = 0;
        GLuint m_Sampler = 0;

        GLuint m_PrimitiveTexture = 0;
        GLsizei m_PrimitiveRows = 0;

        GLuint m_AtlasTexture = 0;
        uint32_t m_AtlasWidth = 0;
        uint32_t m_AtlasHeight = 0;
    };

    /// The same renderer installed by the OpenGL ES backend. A type of its own, so that OpenGLRender and
    /// OpenGLESRender each find only their own backend.
    class OpenGLESRenderer final : public OpenGLRenderer
    {
    public:
        using OpenGLRenderer::OpenGLRenderer;
    };
} // namespace Carbon::Internal
