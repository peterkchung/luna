// About: Per-frame synchronization primitives (fences, semaphores).

#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace luna::core {

class VulkanContext;

static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

class Sync {
public:
    Sync(const VulkanContext& ctx, uint32_t imageCount);
    ~Sync();

    Sync(const Sync&) = delete;
    Sync& operator=(const Sync&) = delete;

    // Recreate semaphores after swapchain recreation (call after vkDeviceWaitIdle)
    void recreateSemaphores(uint32_t imageCount);

    VkSemaphore imageAvailable(uint32_t index) const { return imageAvailable_[index]; }
    VkSemaphore renderFinished(uint32_t index) const { return renderFinished_[index]; }
    VkFence     inFlight(uint32_t frame)       const { return inFlight_[frame]; }

    uint32_t semaphoreCount() const { return static_cast<uint32_t>(imageAvailable_.size()); }

private:
    void createSemaphores(uint32_t count);
    void destroySemaphores();

    VkDevice device_ = VK_NULL_HANDLE;
    std::vector<VkSemaphore> imageAvailable_;
    std::vector<VkSemaphore> renderFinished_;
    VkFence inFlight_[MAX_FRAMES_IN_FLIGHT] = {};
};

} // namespace luna::core
