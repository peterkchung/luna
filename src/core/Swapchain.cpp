// About: Swapchain implementation — format selection, present mode, resize handling.

#include "core/Swapchain.h"
#include "core/VulkanContext.h"
#include "util/Log.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>

namespace luna::core {

static constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

Swapchain::Swapchain(const VulkanContext& ctx) : ctx_(ctx) {
    create();
}

Swapchain::~Swapchain() {
    cleanup();
}

bool Swapchain::recreate() {
    // Wait until window is non-zero size (minimized), but bail if closing
    int w = 0, h = 0;
    glfwGetFramebufferSize(ctx_.window(), &w, &h);
    while (w == 0 || h == 0) {
        if (glfwWindowShouldClose(ctx_.window()))
            return false;
        glfwGetFramebufferSize(ctx_.window(), &w, &h);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(ctx_.device());
    cleanup();
    create();
    return true;
}

void Swapchain::create() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx_.physicalDevice(), ctx_.surface(), &caps);

    auto surfaceFormat = chooseFormat();
    auto presentMode   = chooseMode();
    extent_            = chooseExtent(caps);
    format_            = surfaceFormat.format;

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = ctx_.surface();
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = format_;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent_;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto families = ctx_.queueFamilies();
    uint32_t familyIndices[] = { families.graphics, families.present };
    if (families.graphics != families.present) {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = familyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform   = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;

    if (vkCreateSwapchainKHR(ctx_.device(), &createInfo, nullptr, &swapchain_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swapchain");

    // Get swapchain images
    uint32_t count;
    vkGetSwapchainImagesKHR(ctx_.device(), swapchain_, &count, nullptr);
    images_.resize(count);
    vkGetSwapchainImagesKHR(ctx_.device(), swapchain_, &count, images_.data());

    // Create image views
    imageViews_.resize(count);
    for (uint32_t i = 0; i < count; i++)
        imageViews_[i] = Image::createImageView(ctx_.device(), images_[i], format_,
                                                 VK_IMAGE_ASPECT_COLOR_BIT);

    // Create depth image
    depthImage_ = Image(ctx_, extent_.width, extent_.height, DEPTH_FORMAT,
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                        VK_IMAGE_ASPECT_DEPTH_BIT);

    LOG_INFO("Swapchain created: %ux%u, %u images", extent_.width, extent_.height, count);
}

void Swapchain::cleanup() {
    depthImage_ = Image(); // destroy depth image
    for (auto view : imageViews_)
        vkDestroyImageView(ctx_.device(), view, nullptr);
    imageViews_.clear();
    images_.clear();
    if (swapchain_) {
        vkDestroySwapchainKHR(ctx_.device(), swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

VkSurfaceFormatKHR Swapchain::chooseFormat() const {
    uint32_t count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx_.physicalDevice(), ctx_.surface(), &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx_.physicalDevice(), ctx_.surface(), &count, formats.data());

    for (auto& fmt : formats) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return fmt;
    }
    return formats[0];
}

VkPresentModeKHR Swapchain::chooseMode() const {
    uint32_t count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx_.physicalDevice(), ctx_.surface(), &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx_.physicalDevice(), ctx_.surface(), &count, modes.data());

    for (auto mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::chooseExtent(const VkSurfaceCapabilitiesKHR& caps) const {
    if (caps.currentExtent.width != UINT32_MAX)
        return caps.currentExtent;

    int w, h;
    glfwGetFramebufferSize(ctx_.window(), &w, &h);
    VkExtent2D extent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    extent.width  = std::clamp(extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return extent;
}

} // namespace luna::core
