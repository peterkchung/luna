// About: CommandPool implementation — pool creation, buffer allocation, one-shot commands.

#include "core/CommandPool.h"
#include "core/VulkanContext.h"

#include <stdexcept>

namespace luna::core {

CommandPool::CommandPool(const VulkanContext& ctx, uint32_t count)
    : device_(ctx.device())
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = ctx.queueFamilies().graphics;

    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &pool_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");

    buffers_.resize(count);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = pool_;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = count;

    if (vkAllocateCommandBuffers(device_, &allocInfo, buffers_.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers");
}

CommandPool::~CommandPool() {
    if (pool_) vkDestroyCommandPool(device_, pool_, nullptr);
}

VkCommandBuffer CommandPool::beginOneShot() const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = pool_;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device_, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    return cmd;
}

void CommandPool::endOneShot(VkCommandBuffer cmd, VkQueue queue) const {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device_, pool_, 1, &cmd);
}

VkFence CommandPool::endOneShotWithFence(VkCommandBuffer cmd, VkQueue queue) const {
    vkEndCommandBuffer(cmd);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    if (vkCreateFence(device_, &fenceInfo, nullptr, &fence) != VK_SUCCESS)
        throw std::runtime_error("Failed to create transfer fence");

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(queue, 1, &submitInfo, fence);
    return fence;
}

} // namespace luna::core
