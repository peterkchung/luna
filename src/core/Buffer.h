// About: Vulkan buffer creation (static GPU-only, dynamic host-visible) and staging uploads.

#pragma once

#include <vulkan/vulkan.h>
#include <cstddef>

namespace luna::core {

class VulkanContext;
class CommandPool;
struct StagingBatch;

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

    // Upload data to GPU-local memory via staging buffer (blocks until complete)
    static Buffer createStatic(const VulkanContext& ctx, const CommandPool& cmdPool,
                               VkBufferUsageFlags usage, const void* data, VkDeviceSize size);

    // Record a staging copy into an existing command buffer using shared staging memory.
    // Caller must keep staging alive until the command buffer finishes execution.
    static Buffer createStaticBatch(const VulkanContext& ctx,
                                    VkCommandBuffer transferCmd,
                                    VkBufferUsageFlags usage,
                                    const void* data, VkDeviceSize size,
                                    StagingBatch& staging);

    // Host-visible buffer for per-frame updates
    static Buffer createDynamic(const VulkanContext& ctx, VkBufferUsageFlags usage,
                                VkDeviceSize size);

    void* map();
    void  unmap();

    // Relinquish ownership without Vulkan destroy calls — for bulk shutdown
    // where vkDestroyDevice handles cleanup.
    void release();

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

// Bump allocator over a single host-visible staging buffer.
// Reduces per-batch staging allocations from N to 1, avoiding RADV BO-list limits.
struct StagingBatch {
    Buffer       buffer;
    void*        mapped = nullptr;
    VkDeviceSize offset = 0;
    VkDeviceSize capacity = 0;

    void begin(const VulkanContext& ctx, VkDeviceSize cap);
    void end();
    VkDeviceSize write(const void* data, VkDeviceSize size);
};

} // namespace luna::core
