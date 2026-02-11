// About: Image implementation — creates Vulkan images with memory and views.

#include "core/Image.h"
#include "core/VulkanContext.h"
#include "util/Log.h"

#include <stdexcept>

namespace luna::core {

static uint32_t findMemoryType(VkPhysicalDevice physDevice, uint32_t typeFilter,
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

Image::Image(const VulkanContext& ctx, uint32_t width, uint32_t height,
             VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect)
    : device_(ctx.device())
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device_, &imageInfo, nullptr, &image_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, image_, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(ctx.physicalDevice(), memReqs.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory_) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate image memory");

    vkBindImageMemory(device_, image_, memory_, 0);
    view_ = createImageView(device_, image_, format, aspect);
}

Image::~Image() { cleanup(); }

Image::Image(Image&& other) noexcept
    : device_(other.device_), image_(other.image_),
      memory_(other.memory_), view_(other.view_)
{
    other.device_ = VK_NULL_HANDLE;
    other.image_  = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.view_   = VK_NULL_HANDLE;
}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        cleanup();
        device_ = other.device_;  image_ = other.image_;
        memory_ = other.memory_;  view_  = other.view_;
        other.device_ = VK_NULL_HANDLE;
        other.image_  = VK_NULL_HANDLE;
        other.memory_ = VK_NULL_HANDLE;
        other.view_   = VK_NULL_HANDLE;
    }
    return *this;
}

void Image::cleanup() {
    if (device_ == VK_NULL_HANDLE) return;
    if (view_)   vkDestroyImageView(device_, view_, nullptr);
    if (image_)  vkDestroyImage(device_, image_, nullptr);
    if (memory_) vkFreeMemory(device_, memory_, nullptr);
    view_ = VK_NULL_HANDLE;
    image_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
}

VkImageView Image::createImageView(VkDevice device, VkImage image,
                                   VkFormat format, VkImageAspectFlags aspect) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image    = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format   = format;
    viewInfo.subresourceRange.aspectMask     = aspect;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    VkImageView view;
    if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image view");
    return view;
}

} // namespace luna::core
