// About: Vulkan instance, device, queues, and GLFW window — the central graphics context.

#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace luna::core {

struct QueueFamilyIndices {
    uint32_t graphics = UINT32_MAX;
    uint32_t present  = UINT32_MAX;
    bool isComplete() const { return graphics != UINT32_MAX && present != UINT32_MAX; }
};

class VulkanContext {
public:
    VulkanContext(int width = 1280, int height = 720, const char* title = "Luna");
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    GLFWwindow*      window()         const { return window_; }
    VkInstance       instance()       const { return instance_; }
    VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
    VkDevice         device()         const { return device_; }
    VkSurfaceKHR     surface()        const { return surface_; }
    VkQueue          graphicsQueue()  const { return graphicsQueue_; }
    VkQueue          presentQueue()   const { return presentQueue_; }
    QueueFamilyIndices queueFamilies() const { return queueFamilies_; }

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    bool isDeviceSuitable(VkPhysicalDevice device) const;

    GLFWwindow*              window_         = nullptr;
    VkInstance               instance_       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR             surface_        = VK_NULL_HANDLE;
    VkPhysicalDevice         physicalDevice_ = VK_NULL_HANDLE;
    VkDevice                 device_         = VK_NULL_HANDLE;
    VkQueue                  graphicsQueue_  = VK_NULL_HANDLE;
    VkQueue                  presentQueue_   = VK_NULL_HANDLE;
    QueueFamilyIndices       queueFamilies_;
};

} // namespace luna::core
