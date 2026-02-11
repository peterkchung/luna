// About: Vulkan image and image view creation helpers (depth buffer, textures).

#pragma once

#include <vulkan/vulkan.h>

namespace luna::core {

class VulkanContext;

class Image {
public:
    Image() = default;
    Image(const VulkanContext& ctx, uint32_t width, uint32_t height,
          VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect);
    ~Image();

    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    VkImageView view() const { return view_; }
    VkImage     image() const { return image_; }

    static VkImageView createImageView(VkDevice device, VkImage image,
                                       VkFormat format, VkImageAspectFlags aspect);

private:
    void cleanup();

    VkDevice       device_ = VK_NULL_HANDLE;
    VkImage        image_  = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkImageView    view_   = VK_NULL_HANDLE;
};

} // namespace luna::core
