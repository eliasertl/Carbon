#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Carbon/Core/Vec2.h"
#include "Carbon/Draw/DrawTypes.h"
#include "Carbon/Renderer/TextureFormat.h"
#include "Carbon/Widgets/Image.h"

/// The Vulkan renderer backend. Available when Carbon was built with CARBON_BACKEND_VULKAN (then
/// CARBON_HAS_BACKEND_VULKAN is defined). See Docs/Backends.md.

namespace Carbon
{
    /// What the Vulkan backend needs from the host.
    ///
    /// Carbon draws inside a render pass instance the host begins. With `RenderPass` set, the pipeline is built for
    /// that render pass and subpass (any render pass compatible with it may be used later). With `RenderPass` null,
    /// it is built for dynamic rendering with the formats below: the host must have enabled the `dynamicRendering`
    /// feature (core in Vulkan 1.3, or VK_KHR_dynamic_rendering on 1.2) and begins rendering with
    /// vkCmdBeginRendering.
    struct VulkanInitInfo
    {
        VkInstance Instance = VK_NULL_HANDLE;
        VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
        VkDevice Device = VK_NULL_HANDLE;
        /// The queue the host submits Carbon's command buffers to. Carbon submits its glyph-atlas uploads to it as
        /// well, from inside VulkanRender, so the host must not use the queue from another thread meanwhile.
        VkQueue Queue = VK_NULL_HANDLE;
        uint32_t QueueFamily = 0;
        /// How many frames the GPU may work on at once. Carbon keeps a set of buffers per frame and assumes that
        /// when VulkanRender is called for a frame, the frame FramesInFlight frames earlier has finished on the GPU
        /// (the host waited for its fence). At least 1.
        uint32_t FramesInFlight = 2;
        /// The render pass Carbon draws in, or VK_NULL_HANDLE for dynamic rendering.
        VkRenderPass RenderPass = VK_NULL_HANDLE;
        /// The subpass of RenderPass Carbon draws in.
        uint32_t Subpass = 0;
        /// Format of the color attachment. With an sRGB format Carbon writes linear values. With a render pass,
        /// only whether it is sRGB matters.
        TextureFormat ColorFormat = TextureFormat::BGRA8Unorm;
        /// Format of the depth-stencil attachment, or Undefined. Used for dynamic rendering; Carbon neither tests
        /// nor writes depth.
        TextureFormat DepthStencilFormat = TextureFormat::Undefined;
        /// Sample count of the attachments.
        uint32_t SampleCount = 1;
        /// Optional: a pipeline cache for Carbon's pipeline.
        VkPipelineCache PipelineCache = VK_NULL_HANDLE;
        /// Optional: the pool Carbon allocates its descriptor sets from. It must have been created with
        /// VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT and room for one storage buffer per frame in flight and
        /// one combined image sampler per texture drawn. Without one, Carbon creates its own.
        VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
        /// Optional: host memory callbacks passed to every Vulkan call that takes them.
        const VkAllocationCallbacks* Allocator = nullptr;
    };

    /// Installs the Vulkan backend into the current context. Returns false, and logs why, when a handle is missing,
    /// a format is not supported, an object could not be created, or the context has a backend already.
    bool VulkanInit(const VulkanInitInfo& info);

    /// Removes the Vulkan backend from the current context and destroys everything it created. The GPU must be
    /// done with every frame Carbon recorded (vkDeviceWaitIdle or the frames' fences).
    void VulkanShutdown();

    /// Records the last finished frame into `commandBuffer`, inside the render pass or rendering scope the host
    /// began, whose size is the display size times the content scale. Call it after EndFrame, once per frame the
    /// host submits; more calls in the same frame (for several targets) reuse the frame's buffers.
    ///
    /// Carbon binds its own pipeline, descriptor sets, vertex and index buffers, and sets viewport, scissor and push
    /// constants; it does not restore the previous ones. Glyph-atlas changes are uploaded with a command buffer of
    /// Carbon's own, submitted to the queue right away, so they reach the GPU before the host's submission.
    void VulkanRender(VkCommandBuffer commandBuffer);

    /// Returns the TextureID of a host image view, to draw it with Image or DrawList::AddImage. `layout` is the
    /// layout the image is in whenever Carbon's commands sample it. Carbon creates a descriptor set for the view
    /// and releases it when a whole frame passes in which the view was neither registered nor drawn; keep the view
    /// alive until then, or call VulkanReleaseTexture before destroying it.
    TextureID VulkanGetTextureID(VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    /// Forgets a view registered with VulkanGetTextureID right away. Call it before destroying a view sooner than a
    /// frame after its last use: Vulkan may give a new view the same handle. The descriptor set is destroyed once
    /// frames in flight are done with it.
    void VulkanReleaseTexture(VkImageView view);

    /// Displays one of the host's image views at `size` points: Image(VulkanGetTextureID(view, layout), ...).
    void VulkanImage(VkImageView view, Vec2 size, const ImageOptions& options = {},
                     VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
} // namespace Carbon
