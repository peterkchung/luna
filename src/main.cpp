// About: Entry point for the Luna lunar landing simulator.

#include "core/VulkanContext.h"
#include "core/Swapchain.h"
#include "core/RenderPass.h"
#include "core/CommandPool.h"
#include "core/Sync.h"
#include "util/Log.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>

using namespace luna::core;

static bool framebufferResized = false;

static void framebufferResizeCallback(GLFWwindow* /*window*/, int /*w*/, int /*h*/) {
    framebufferResized = true;
}

int main() {
    luna::util::Log::init();
    LOG_INFO("Luna starting");

    VulkanContext ctx;
    glfwSetFramebufferSizeCallback(ctx.window(), framebufferResizeCallback);

    Swapchain   swapchain(ctx);
    RenderPass  renderPass(ctx, swapchain);
    CommandPool commandPool(ctx, MAX_FRAMES_IN_FLIGHT);
    Sync        sync(ctx);

    uint32_t currentFrame = 0;

    while (!glfwWindowShouldClose(ctx.window())) {
        glfwPollEvents();

        // Wait for previous frame's fence
        VkFence fence = sync.inFlight(currentFrame);
        vkWaitForFences(ctx.device(), 1, &fence, VK_TRUE, UINT64_MAX);

        // Acquire next swapchain image
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(ctx.device(), swapchain.handle(), UINT64_MAX,
                                                 sync.imageAvailable(currentFrame),
                                                 VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            swapchain.recreate();
            renderPass.recreateFramebuffers(swapchain);
            continue;
        }

        vkResetFences(ctx.device(), 1, &fence);

        // Record command buffer
        VkCommandBuffer cmd = commandPool.buffer(currentFrame);
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &beginInfo);

        // Reversed-Z: depth clear = 0.0 (far plane)
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
        clearValues[1].depthStencil = {0.0f, 0};

        VkRenderPassBeginInfo rpBegin{};
        rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass        = renderPass.handle();
        rpBegin.framebuffer       = renderPass.framebuffer(imageIndex);
        rpBegin.renderArea.offset = {0, 0};
        rpBegin.renderArea.extent = swapchain.extent();
        rpBegin.clearValueCount   = static_cast<uint32_t>(clearValues.size());
        rpBegin.pClearValues      = clearValues.data();

        vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);

        // Submit
        VkSemaphore waitSems[]   = { sync.imageAvailable(currentFrame) };
        VkSemaphore signalSems[] = { sync.renderFinished(currentFrame) };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submitInfo{};
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = waitSems;
        submitInfo.pWaitDstStageMask    = waitStages;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = signalSems;

        vkQueueSubmit(ctx.graphicsQueue(), 1, &submitInfo, sync.inFlight(currentFrame));

        // Present
        VkSwapchainKHR swapchains[] = { swapchain.handle() };
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = signalSems;
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = swapchains;
        presentInfo.pImageIndices      = &imageIndex;

        result = vkQueuePresentKHR(ctx.presentQueue(), &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            swapchain.recreate();
            renderPass.recreateFramebuffers(swapchain);
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    vkDeviceWaitIdle(ctx.device());
    LOG_INFO("Luna shutting down");
    return 0;
}
