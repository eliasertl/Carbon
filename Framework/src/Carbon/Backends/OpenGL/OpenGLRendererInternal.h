#pragma once

#include <cstdint>
#include <unordered_set>

#include "Carbon/Backends/OpenGL/OpenGLBackend.h"
#include "Carbon/Backends/OpenGL/OpenGLFunctionsInternal.h"
#include "Carbon/Renderer/RendererBackend.h"

namespace Carbon::Internal
{
    /// The OpenGL 3.3 core renderer backend.
    ///
    /// Every call that touches OpenGL saves the state it changes and restores it before returning, so the host's
    /// own rendering is never disturbed. Primitives reach the fragment shader through an RGBA32UI texture buffer.
    class OpenGLRenderer : public RendererBackend
    {
    public:
        OpenGLRenderer(const OpenGLFunctions& gl, TextureFormat colorFormat);
        ~OpenGLRenderer() override;

        /// False when a shader did not compile or link; the reason was logged.
        bool IsValid() const { return m_Program != 0; }

        std::string_view GetName() const override { return "OpenGL"; }
        RendererBackendCapabilities GetCapabilities() const override;
        void UpdateGlyphAtlas(const GlyphAtlasUpdate& update) override;
        void Render(const DrawData& drawData) override;
        void ReleaseTexture(TextureID texture) override;

        /// Registers a host texture for the current frame and returns its ID.
        TextureID RegisterTexture(GLuint texture);

    private:
        /// The OpenGL state Carbon changes, as found before it does.
        struct SavedState
        {
            GLint Program = 0;
            GLint VertexArray = 0;
            GLint ArrayBuffer = 0;
            GLint ActiveTexture = 0;
            GLint Texture2D[2] = {};
            GLint TextureBuffer[2] = {};
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
            GLboolean DepthMask = 0;
            GLboolean Blend = 0;
            GLboolean ScissorTest = 0;
            GLboolean CullFace = 0;
            GLboolean DepthTest = 0;
            GLboolean StencilTest = 0;
            GLboolean PrimitiveRestart = 0;
            GLboolean ColorLogicOp = 0;
            GLboolean FramebufferSrgb = 0;
        };

        /// Pixel-unpack state, changed only while the atlas is uploaded.
        struct SavedUnpackState
        {
            GLint Buffer = 0;
            GLint Alignment = 0;
            GLint RowLength = 0;
            GLint SkipRows = 0;
            GLint SkipPixels = 0;
            GLint ImageHeight = 0;
            GLint SkipImages = 0;
            GLint SwapBytes = 0;
            GLint LsbFirst = 0;
            GLint ActiveTexture = 0;
            GLint Texture2D = 0;
        };

        bool CreateProgram();
        void SaveState(SavedState& state) const;
        void RestoreState(const SavedState& state) const;
        void SetEnabled(GLenum capability, bool enabled) const;
        void EnsureBuffer(GLuint buffer, GLsizeiptr& capacity, GLsizeiptr requiredSize) const;

    private:
        OpenGLFunctions m_GL;
        bool m_IsLinearOutput = false;

        GLuint m_Program = 0;
        GLint m_FrameLocation = -1;
        GLuint m_VertexArray = 0;
        GLuint m_VertexBuffer = 0;
        GLuint m_IndexBuffer = 0;
        GLuint m_PrimitiveBuffer = 0;
        GLuint m_PrimitiveTexture = 0;
        GLsizeiptr m_VertexCapacity = 0;
        GLsizeiptr m_IndexCapacity = 0;
        GLsizeiptr m_PrimitiveCapacity = 0;
        GLuint m_Sampler = 0;

        GLuint m_AtlasTexture = 0;
        uint32_t m_AtlasWidth = 0;
        uint32_t m_AtlasHeight = 0;

        std::unordered_set<GLuint> m_HostTextures;
    };
} // namespace Carbon::Internal
