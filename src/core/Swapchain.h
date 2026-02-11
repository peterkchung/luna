// About: Vulkan swapchain creation and recreation on resize.

#pragma once

#include "core/Image.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace luna::core {

class VulkanContext;

class Swapchain {
public:
    Swapchain(const VulkanContext& ctx);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    void recreate();

    VkSwapchainKHR handle()     const { return swapchain_; }
    VkFormat       imageFormat() const { return format_; }
    VkExtent2D     extent()     const { return extent_; }
    uint32_t       imageCount() const { return static_cast<uint32_t>(imageViews_.size()); }
    VkImageView    imageView(uint32_t i) const { return imageViews_[i]; }

    VkImageView depthView() const { return depthImage_.view(); }

private:
    void create();
    void cleanup();

    VkSurfaceFormatKHR chooseFormat() const;
    VkPresentModeKHR   chooseMode() const;
    VkExtent2D         chooseExtent(const VkSurfaceCapabilitiesKHR& caps) const;

    const VulkanContext& ctx_;
    VkSwapchainKHR       swapchain_ = VK_NULL_HANDLE;
    VkFormat             format_    = VK_FORMAT_UNDEFINED;
    VkExtent2D           extent_    = {0, 0};
    std::vector<VkImage>     images_;
    std::vector<VkImageView> imageViews_;
    Image                    depthImage_;
};

} // namespace luna::core
