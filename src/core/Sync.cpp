// About: Sync implementation — creates per-frame fences and semaphores.

#include "core/Sync.h"
#include "core/VulkanContext.h"

#include <stdexcept>

namespace luna::core {

Sync::Sync(const VulkanContext& ctx) : device_(ctx.device()) {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device_, &semInfo, nullptr, &imageAvailable_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semInfo, nullptr, &renderFinished_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fenceInfo, nullptr, &inFlight_[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create sync objects");
    }
}

Sync::~Sync() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (inFlight_[i])       vkDestroyFence(device_, inFlight_[i], nullptr);
        if (renderFinished_[i]) vkDestroySemaphore(device_, renderFinished_[i], nullptr);
        if (imageAvailable_[i]) vkDestroySemaphore(device_, imageAvailable_[i], nullptr);
    }
}

} // namespace luna::core
