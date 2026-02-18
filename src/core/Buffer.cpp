// About: Buffer implementation — staging uploads, memory type selection, RAII cleanup.

#include "core/Buffer.h"
#include "core/VulkanContext.h"
#include "core/CommandPool.h"

#include <cstring>
#include <stdexcept>

namespace luna::core {

Buffer::~Buffer() { cleanup(); }

Buffer::Buffer(Buffer&& other) noexcept
    : device_(other.device_), buffer_(other.buffer_),
      memory_(other.memory_), size_(other.size_)
{
    other.buffer_ = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.size_   = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        cleanup();
        device_ = other.device_;
        buffer_ = other.buffer_;
        memory_ = other.memory_;
        size_   = other.size_;
        other.buffer_ = VK_NULL_HANDLE;
        other.memory_ = VK_NULL_HANDLE;
        other.size_   = 0;
    }
    return *this;
}

void Buffer::cleanup() {
    if (buffer_) vkDestroyBuffer(device_, buffer_, nullptr);
    if (memory_) vkFreeMemory(device_, memory_, nullptr);
    buffer_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
}

uint32_t Buffer::findMemoryType(VkPhysicalDevice physDevice, uint32_t typeFilter,
                                VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

Buffer Buffer::createRaw(VkDevice device, VkPhysicalDevice physDevice,
                         VkDeviceSize size, VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags memProps) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    Buffer buf;
    buf.device_ = device;
    buf.size_   = size;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buf.buffer_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buf.buffer_, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = Buffer::findMemoryType(physDevice, memReqs.memoryTypeBits, memProps);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &buf.memory_) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate buffer memory");

    vkBindBufferMemory(device, buf.buffer_, buf.memory_, 0);
    return buf;
}

Buffer Buffer::createStatic(const VulkanContext& ctx, const CommandPool& cmdPool,
                            VkBufferUsageFlags usage, const void* data, VkDeviceSize size) {
    // Create staging buffer
    auto staging = createRaw(
        ctx.device(), ctx.physicalDevice(), size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Copy data to staging
    void* mapped;
    vkMapMemory(ctx.device(), staging.memory_, 0, size, 0, &mapped);
    std::memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(ctx.device(), staging.memory_);

    // Create device-local buffer
    auto buffer = createRaw(
        ctx.device(), ctx.physicalDevice(), size,
        usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Copy via one-shot command buffer
    VkCommandBuffer cmd = cmdPool.beginOneShot();
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, staging.handle(), buffer.handle(), 1, &copyRegion);
    cmdPool.endOneShot(cmd, ctx.graphicsQueue());

    return buffer;
}

void StagingBatch::begin(const VulkanContext& ctx, VkDeviceSize cap) {
    capacity = cap;
    offset = 0;
    buffer = Buffer::createDynamic(ctx, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, cap);
    mapped = buffer.map();
}

void StagingBatch::end() {
    if (mapped) {
        buffer.unmap();
        mapped = nullptr;
    }
}

VkDeviceSize StagingBatch::write(const void* data, VkDeviceSize size) {
    VkDeviceSize srcOffset = offset;
    std::memcpy(static_cast<char*>(mapped) + offset, data, static_cast<size_t>(size));
    offset += size;
    return srcOffset;
}

Buffer Buffer::createStaticBatch(const VulkanContext& ctx,
                                 VkCommandBuffer transferCmd,
                                 VkBufferUsageFlags usage,
                                 const void* data, VkDeviceSize size,
                                 StagingBatch& staging) {
    VkDeviceSize srcOffset = staging.write(data, size);

    auto buffer = createRaw(
        ctx.device(), ctx.physicalDevice(), size,
        usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcOffset;
    copyRegion.size = size;
    vkCmdCopyBuffer(transferCmd, staging.buffer.handle(), buffer.handle(), 1, &copyRegion);

    return buffer;
}

Buffer Buffer::createDynamic(const VulkanContext& ctx, VkBufferUsageFlags usage,
                             VkDeviceSize size) {
    return createRaw(
        ctx.device(), ctx.physicalDevice(), size, usage,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void* Buffer::map() {
    void* data;
    vkMapMemory(device_, memory_, 0, size_, 0, &data);
    return data;
}

void Buffer::unmap() {
    vkUnmapMemory(device_, memory_);
}

void Buffer::release() {
    buffer_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
    size_   = 0;
}

} // namespace luna::core
