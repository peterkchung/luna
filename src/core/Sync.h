// About: Per-frame synchronization primitives (fences, semaphores).

#pragma once

#include <vulkan/vulkan.h>

namespace luna::core {

class VulkanContext;

static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

class Sync {
public:
    Sync(const VulkanContext& ctx);
    ~Sync();

    Sync(const Sync&) = delete;
    Sync& operator=(const Sync&) = delete;

    VkSemaphore imageAvailable(uint32_t frame) const { return imageAvailable_[frame]; }
    VkSemaphore renderFinished(uint32_t frame) const { return renderFinished_[frame]; }
    VkFence     inFlight(uint32_t frame)       const { return inFlight_[frame]; }

private:
    VkDevice    device_ = VK_NULL_HANDLE;
    VkSemaphore imageAvailable_[MAX_FRAMES_IN_FLIGHT] = {};
    VkSemaphore renderFinished_[MAX_FRAMES_IN_FLIGHT] = {};
    VkFence     inFlight_[MAX_FRAMES_IN_FLIGHT]       = {};
};

} // namespace luna::core
