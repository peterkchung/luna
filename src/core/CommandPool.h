// About: Vulkan command pool and command buffer management.

#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace luna::core {

class VulkanContext;

class CommandPool {
public:
    CommandPool(const VulkanContext& ctx, uint32_t count);
    ~CommandPool();

    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;

    VkCommandPool    pool() const { return pool_; }
    VkCommandBuffer  buffer(uint32_t i) const { return buffers_[i]; }

    VkCommandBuffer beginOneShot() const;
    void            endOneShot(VkCommandBuffer cmd, VkQueue queue) const;

    // Submit without blocking. Returns a fence the caller must wait on and destroy.
    VkFence         endOneShotWithFence(VkCommandBuffer cmd, VkQueue queue) const;

private:
    VkDevice                      device_ = VK_NULL_HANDLE;
    VkCommandPool                 pool_   = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer>  buffers_;
};

} // namespace luna::core
