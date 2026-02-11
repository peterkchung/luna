// About: Vulkan buffer creation (static GPU-only, dynamic host-visible) and staging uploads.

#pragma once

#include <vulkan/vulkan.h>
#include <cstddef>

namespace luna::core {

class VulkanContext;
class CommandPool;

class Buffer {
public:
    Buffer() = default;
    ~Buffer();

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    VkBuffer handle() const { return buffer_; }
    VkDeviceSize size() const { return size_; }

    // Upload data to GPU-local memory via staging buffer
    static Buffer createStatic(const VulkanContext& ctx, const CommandPool& cmdPool,
                               VkBufferUsageFlags usage, const void* data, VkDeviceSize size);

    // Host-visible buffer for per-frame updates
    static Buffer createDynamic(const VulkanContext& ctx, VkBufferUsageFlags usage,
                                VkDeviceSize size);

    void* map();
    void  unmap();

private:
    static Buffer createRaw(VkDevice device, VkPhysicalDevice physDevice,
                            VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags memProps);
    static uint32_t findMemoryType(VkPhysicalDevice physDevice, uint32_t typeFilter,
                                   VkMemoryPropertyFlags properties);
    void cleanup();

    VkDevice       device_ = VK_NULL_HANDLE;
    VkBuffer       buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkDeviceSize   size_   = 0;
};

} // namespace luna::core
