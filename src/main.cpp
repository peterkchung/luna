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
#include "scene/CubesphereBody.h"
#include "scene/ChunkGenerator.h"
#include "util/Log.h"
#include "util/Math.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <cstddef>

using namespace luna::core;

static bool framebufferResized = false;

static void framebufferResizeCallback(GLFWwindow* /*window*/, int /*w*/, int /*h*/) {
    framebufferResized = true;
}

struct TerrainPushConstants {
    glm::mat4 viewProj;       // rotation + projection, no translation
    glm::vec3 cameraOffset;   // (chunkCenter - cameraPos), computed in doubles on CPU
    float     _pad0;
    glm::vec4 sunDirection;
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
    luna::camera::CameraController cameraController;

    // Initial camera position: 100km above surface at 0°N 0°E, looking toward Moon center
    double startAlt = luna::util::LUNAR_RADIUS + 100'000.0;
    camera.setPosition(glm::dvec3(startAlt, 0.0, 0.0));
    camera.setRotation(glm::radians(-15.0), glm::radians(180.0));

    // Terrain pipeline
    auto terrainPipeline = Pipeline::Builder(ctx, renderPass.handle())
        .setShaders("shaders/terrain.vert.spv", "shaders/terrain.frag.spv")
        .setVertexBinding(sizeof(luna::scene::ChunkVertex), {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(luna::scene::ChunkVertex, position))},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(luna::scene::ChunkVertex, normal))},
            {2, 0, VK_FORMAT_R32_SFLOAT,       static_cast<uint32_t>(offsetof(luna::scene::ChunkVertex, height))},
        })
        .enableDepthTest()
        .setPushConstantSize(sizeof(TerrainPushConstants))
        .build();

    // Build spherical Moon (cubesphere with fixed LOD depth 4)
    luna::scene::CubesphereBody moon(ctx, commandPool, luna::util::LUNAR_RADIUS);

    // Sun direction (hardcoded: from upper-right in world space)
    glm::vec3 sunDir3 = glm::normalize(glm::vec3(0.5f, 0.8f, 0.3f));
    glm::vec4 sunDir(sunDir3, 0.0f);

    uint32_t currentFrame = 0;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(ctx.window())) {
        glfwPollEvents();
        input.update();

        double now = glfwGetTime();
        double dt = now - lastTime;
        lastTime = now;

        // Cursor capture: right-click to capture, ESC to release
        if (input.isKeyPressed(GLFW_KEY_ESCAPE))
            input.setCursorCaptured(false);
        if (glfwGetMouseButton(ctx.window(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS &&
            !input.isCursorCaptured())
            input.setCursorCaptured(true);

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

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
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

        // Camera-relative rendering: rotation-only VP + per-chunk offset
        glm::dmat4 viewRot = camera.getRotationOnlyViewMatrix();
        glm::dmat4 proj = camera.getProjectionMatrix();
        glm::mat4 vp = glm::mat4(proj * viewRot);

        // Draw Moon (cubesphere handles per-chunk push constants internally)
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainPipeline.handle());
        moon.draw(cmd, terrainPipeline.layout(), vp, camera.position(), sunDir);

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
