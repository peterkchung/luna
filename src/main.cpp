// About: Entry point for the Luna lunar landing simulator.

#include "core/VulkanContext.h"
#include "core/Swapchain.h"
#include "core/RenderPass.h"
#include "core/CommandPool.h"
#include "core/Sync.h"
#include "core/Pipeline.h"
#include "core/Buffer.h"
#include "input/InputManager.h"
#include "camera/Camera.h"
#include "camera/CameraController.h"
#include "util/Log.h"
#include "util/Math.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>

using namespace luna::core;

static bool framebufferResized = false;

static void framebufferResizeCallback(GLFWwindow* /*window*/, int /*w*/, int /*h*/) {
    framebufferResized = true;
}

struct TestVertex {
    glm::vec3 position;
    glm::vec3 color;
};

int main() {
    luna::util::Log::init();
    LOG_INFO("Luna starting");

    VulkanContext ctx;
    glfwSetFramebufferSizeCallback(ctx.window(), framebufferResizeCallback);

    Swapchain   swapchain(ctx);
    RenderPass  renderPass(ctx, swapchain);
    CommandPool commandPool(ctx, MAX_FRAMES_IN_FLIGHT);
    Sync        sync(ctx);

    luna::input::InputManager input(ctx.window());
    luna::camera::Camera camera;
    camera.setPosition(glm::dvec3(0.0, 0.0, 3.0));
    luna::camera::CameraController cameraController;

    // Test triangle pipeline
    auto pipeline = Pipeline::Builder(ctx, renderPass.handle())
        .setShaders("shaders/test.vert.spv", "shaders/test.frag.spv")
        .setVertexBinding(sizeof(TestVertex), {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TestVertex, position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TestVertex, color)},
        })
        .enableDepthTest()
        .setPushConstantSize(sizeof(glm::mat4))
        .build();

    // Triangle vertices
    std::array<TestVertex, 3> vertices = {{
        {{ 0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    }};
    auto vertexBuffer = Buffer::createStatic(ctx, commandPool,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        vertices.data(), sizeof(vertices));

    uint32_t currentFrame = 0;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(ctx.window())) {
        glfwPollEvents();
        input.update();

        // Delta time
        double now = glfwGetTime();
        double dt = now - lastTime;
        lastTime = now;

        // Toggle cursor capture with right mouse button
        if (input.isKeyPressed(GLFW_KEY_ESCAPE))
            input.setCursorCaptured(false);
        if (glfwGetMouseButton(ctx.window(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS &&
            !input.isCursorCaptured())
            input.setCursorCaptured(true);

        // Update camera aspect ratio
        camera.setAspect(static_cast<double>(swapchain.extent().width) /
                         static_cast<double>(swapchain.extent().height));
        cameraController.update(camera, input, dt);

        VkFence fence = sync.inFlight(currentFrame);
        vkWaitForFences(ctx.device(), 1, &fence, VK_TRUE, UINT64_MAX);

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

        VkCommandBuffer cmd = commandPool.buffer(currentFrame);
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &beginInfo);

        // Reversed-Z: depth clear = 0.0 (far plane)
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.05f, 0.05f, 0.05f, 1.0f}};
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

        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(swapchain.extent().width);
        viewport.height   = static_cast<float>(swapchain.extent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchain.extent();
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Draw triangle with camera VP
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle());

        glm::mat4 mvp = camera.getViewProjectionMatrix();
        vkCmdPushConstants(cmd, pipeline.layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(glm::mat4), &mvp);

        VkBuffer buffers[] = { vertexBuffer.handle() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
        vkCmdDraw(cmd, 3, 1, 0, 0);

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
