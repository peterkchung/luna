// About: Sync implementation — per-frame fences, per-swapchain-image semaphores.

#include "core/Sync.h"
#include "core/VulkanContext.h"

#include <stdexcept>

namespace luna::core {

Sync::Sync(const VulkanContext& ctx, uint32_t imageCount) : device_(ctx.device()) {
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateFence(device_, &fenceInfo, nullptr, &inFlight_[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create fence");
    }

    createSemaphores(imageCount);
}

Sync::~Sync() {
    destroySemaphores();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (inFlight_[i]) vkDestroyFence(device_, inFlight_[i], nullptr);
    }
}

void Sync::recreateSemaphores(uint32_t imageCount) {
    destroySemaphores();
    createSemaphores(imageCount);
}

void Sync::createSemaphores(uint32_t count) {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    imageAvailable_.resize(count);
    renderFinished_.resize(count);

    for (uint32_t i = 0; i < count; i++) {
        if (vkCreateSemaphore(device_, &semInfo, nullptr, &imageAvailable_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semInfo, nullptr, &renderFinished_[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create semaphores");
    }
}

void Sync::destroySemaphores() {
    for (auto sem : renderFinished_)
        if (sem) vkDestroySemaphore(device_, sem, nullptr);
    for (auto sem : imageAvailable_)
        if (sem) vkDestroySemaphore(device_, sem, nullptr);
    renderFinished_.clear();
    imageAvailable_.clear();
}

} // namespace luna::core
