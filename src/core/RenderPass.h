// About: Vulkan render pass with color and reversed-Z depth attachments.

#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace luna::core {

class VulkanContext;
class Swapchain;

class RenderPass {
public:
    RenderPass(const VulkanContext& ctx, const Swapchain& swapchain);
    ~RenderPass();

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    VkRenderPass handle() const { return renderPass_; }
    VkFramebuffer framebuffer(uint32_t i) const { return framebuffers_[i]; }

    void recreateFramebuffers(const Swapchain& swapchain);

private:
    void createRenderPass(VkFormat colorFormat);
    void createFramebuffers(const Swapchain& swapchain);

    VkDevice                    device_     = VK_NULL_HANDLE;
    VkRenderPass                renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer>  framebuffers_;
};

} // namespace luna::core
