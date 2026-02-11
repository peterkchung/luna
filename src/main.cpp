// About: Entry point for the Luna lunar landing simulator.

#include "core/VulkanContext.h"
#include "util/Log.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

int main() {
    luna::util::Log::init();
    LOG_INFO("Luna starting");

    luna::core::VulkanContext ctx;

    while (!glfwWindowShouldClose(ctx.window())) {
        glfwPollEvents();
    }

    vkDeviceWaitIdle(ctx.device());
    LOG_INFO("Luna shutting down");
    return 0;
}
