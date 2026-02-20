// About: Entry point for the Luna Artemis-era lunar landing simulator.

#include "camera/Camera.h"
#include "camera/CameraController.h"
#include "core/Buffer.h"
#include "core/CommandPool.h"
#include "core/Pipeline.h"
#include "core/RenderPass.h"
#include "core/Swapchain.h"
#include "core/Sync.h"
#include "core/VulkanContext.h"
#include "hud/Hud.h"
#include "input/InputManager.h"
#include "scene/ChunkGenerator.h"
#include "scene/CubesphereBody.h"
#include "scene/Starfield.h"
#include "sim/Physics.h"
#include "sim/SimState.h"
#include "sim/TerrainQuery.h"
#include "util/Log.h"
#include "util/Math.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <cstddef>

using namespace luna::core;

static bool framebufferResized = false;

static void framebufferResizeCallback(GLFWwindow * /*window*/, int /*w*/,
                                      int /*h*/) {
  framebufferResized = true;
}

struct TerrainPushConstants {
  glm::mat4 viewProj; // rotation + projection, no translation
  glm::vec3
      cameraOffset; // (chunkCenter - cameraPos), computed in doubles on CPU
  float _pad0;
  glm::vec4 sunDirection;
  glm::vec3 cameraWorldPos; // camera position relative to Moon center (for
                            // sphere dir)
  float _pad1;
};

struct StarfieldPushConstants {
  glm::mat4 viewProj;
};

int main() {
  luna::util::Log::init();
  LOG_INFO("Luna starting");

  luna::sim::initTerrain("assets/terrain/ldem_16.tif");

  VulkanContext ctx;
  glfwSetFramebufferSizeCallback(ctx.window(), framebufferResizeCallback);

  Swapchain swapchain(ctx);
  RenderPass renderPass(ctx, swapchain);
  CommandPool commandPool(ctx, MAX_FRAMES_IN_FLIGHT);
  Sync sync(ctx, swapchain.imageCount());

  luna::input::InputManager input(ctx.window());
  luna::camera::Camera camera;
  luna::camera::CameraController cameraController;

  // Initial camera position: 100km above surface on -Y axis, looking toward
  // horizon
  double startAlt = luna::util::LUNAR_RADIUS + 100'000.0;
  camera.setPosition(glm::dvec3(0.0, -startAlt, 0.0));
  // Face orbital velocity direction (+X) with slight pitch toward the Moon
  camera.setRotation(glm::radians(10.0), glm::radians(-90.0));

  // Terrain pipeline
  auto terrainPipeline =
      Pipeline::Builder(ctx, renderPass.handle())
          .setShaders("shaders/terrain.vert.spv", "shaders/terrain.frag.spv")
          .setVertexBinding(sizeof(luna::scene::ChunkVertex),
                            {
                                {0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                 static_cast<uint32_t>(offsetof(
                                     luna::scene::ChunkVertex, position))},
                                {1, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                 static_cast<uint32_t>(offsetof(
                                     luna::scene::ChunkVertex, normal))},
                                {2, 0, VK_FORMAT_R32_SFLOAT,
                                 static_cast<uint32_t>(offsetof(
                                     luna::scene::ChunkVertex, height))},
                            })
          .setCullMode(VK_CULL_MODE_NONE)
          .enableDepthTest()
          .setPushConstantSize(sizeof(TerrainPushConstants))
          .build();

  // Starfield pipeline (points, depth test on but no depth write, alpha
  // blending)
  auto starfieldPipeline =
      Pipeline::Builder(ctx, renderPass.handle())
          .setShaders("shaders/starfield.vert.spv",
                      "shaders/starfield.frag.spv")
          .setVertexBinding(sizeof(luna::scene::StarVertex),
                            {
                                {0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                 static_cast<uint32_t>(offsetof(
                                     luna::scene::StarVertex, direction))},
                                {1, 0, VK_FORMAT_R32_SFLOAT,
                                 static_cast<uint32_t>(offsetof(
                                     luna::scene::StarVertex, brightness))},
                            })
          .setTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
          .enableDepthTest()
          .setDepthWrite(false)
          .enableAlphaBlending()
          .setPushConstantSize(sizeof(StarfieldPushConstants))
          .build();

  // HUD pipeline (screen-space overlay, no depth test, alpha blending)
  auto hudPipeline =
      Pipeline::Builder(ctx, renderPass.handle())
          .setShaders("shaders/hud.vert.spv", "shaders/hud.frag.spv")
          .setVertexBinding(
              sizeof(luna::hud::HudVertex),
              {
                  {0, 0, VK_FORMAT_R32G32_SFLOAT,
                   static_cast<uint32_t>(
                       offsetof(luna::hud::HudVertex, position))},
                  {1, 0, VK_FORMAT_R32G32_SFLOAT,
                   static_cast<uint32_t>(offsetof(luna::hud::HudVertex, uv))},
                  {2, 0, VK_FORMAT_R32_SFLOAT,
                   static_cast<uint32_t>(
                       offsetof(luna::hud::HudVertex, instrumentId))},
              })
          .setCullMode(VK_CULL_MODE_NONE)
          .enableAlphaBlending()
          .setPushConstantSize(sizeof(luna::hud::HudPushConstants))
          .build();

  luna::hud::Hud hud(ctx, commandPool);
  luna::scene::Starfield starfield(ctx, commandPool);
  luna::scene::CubesphereBody moon(ctx, commandPool, luna::util::LUNAR_RADIUS);

  // Starship HLS starts in 100km circular orbit (post-transfer from NRHO)
  luna::sim::SimState simState;
  double orbitR = luna::util::LUNAR_RADIUS + 100'000.0;
  double orbitV = std::sqrt(luna::util::LUNAR_GM / orbitR);
  simState.position = glm::dvec3(0.0, -orbitR, 0.0);
  simState.velocity = glm::dvec3(orbitV, 0.0, 0.0);

  luna::sim::Physics physics;
  physics.setTerrainQuery(luna::sim::sampleTerrainHeight);

  bool attachedToLander = true;

  // Sun direction (hardcoded: from upper-right in world space)
  glm::vec3 sunDir3 = glm::normalize(glm::vec3(0.5f, 0.8f, 0.3f));
  glm::vec4 sunDir(sunDir3, 0.0f);

  uint32_t currentFrame = 0;
  uint32_t currentSemaphore = 0;
  double lastTime = glfwGetTime();

  while (!glfwWindowShouldClose(ctx.window())) {
    glfwPollEvents();

    // Exit immediately after processing close event — before any blocking
    // Vulkan calls
    if (glfwWindowShouldClose(ctx.window()))
      break;

    input.update();

    double now = glfwGetTime();
    double dt = now - lastTime;
    lastTime = now;

    // Cursor capture: right-click to capture, ESC to release
    if (input.isKeyPressed(GLFW_KEY_ESCAPE))
      input.setCursorCaptured(false);
    if (glfwGetMouseButton(ctx.window(), GLFW_MOUSE_BUTTON_RIGHT) ==
            GLFW_PRESS &&
        !input.isCursorCaptured())
      input.setCursorCaptured(true);

    // Toggle camera mode: P key
    if (input.isKeyPressed(GLFW_KEY_P))
      attachedToLander = !attachedToLander;

    // Lander throttle: Z to increase, X to decrease
    if (input.isKeyDown(GLFW_KEY_Z))
      simState.throttle = glm::min(simState.throttle + 0.5 * dt, 1.0);
    if (input.isKeyDown(GLFW_KEY_X))
      simState.throttle = glm::max(simState.throttle - 0.5 * dt, 0.0);

    // Lander rotation torque (body frame): IJKL for pitch/yaw, UO for roll
    simState.torqueInput = glm::dvec3(0.0);
    double torqueRate = 0.5;
    if (input.isKeyDown(GLFW_KEY_I))
      simState.torqueInput.x += torqueRate;
    if (input.isKeyDown(GLFW_KEY_K))
      simState.torqueInput.x -= torqueRate;
    if (input.isKeyDown(GLFW_KEY_J))
      simState.torqueInput.y += torqueRate;
    if (input.isKeyDown(GLFW_KEY_L))
      simState.torqueInput.y -= torqueRate;
    if (input.isKeyDown(GLFW_KEY_U))
      simState.torqueInput.z += torqueRate;
    if (input.isKeyDown(GLFW_KEY_O))
      simState.torqueInput.z -= torqueRate;

    // Step physics
    physics.step(simState, dt);

    // Camera follows lander when attached
    if (attachedToLander) {
      camera.setPosition(simState.position);
    }

    camera.setAspect(static_cast<double>(swapchain.extent().width) /
                     static_cast<double>(swapchain.extent().height));
    cameraController.update(camera, input, dt);

    // Wait for previous frame's GPU work with timeout so we stay responsive
    VkFence fence = sync.inFlight(currentFrame);
    constexpr uint64_t FENCE_TIMEOUT =
        100'000'000; // 100ms — keeps loop responsive to close events
    VkResult fenceResult =
        vkWaitForFences(ctx.device(), 1, &fence, VK_TRUE, FENCE_TIMEOUT);
    if (fenceResult == VK_TIMEOUT)
      continue; // retry next iteration

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        ctx.device(), swapchain.handle(), FENCE_TIMEOUT,
        sync.imageAvailable(currentSemaphore), VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      if (!swapchain.recreate())
        break;
      renderPass.recreateFramebuffers(swapchain);
      sync.recreateSemaphores(swapchain.imageCount());
      currentSemaphore = 0;
      continue;
    }
    if (result == VK_TIMEOUT || result == VK_NOT_READY)
      continue;

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
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = renderPass.handle();
    rpBegin.framebuffer = renderPass.framebuffer(imageIndex);
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = swapchain.extent();
    rpBegin.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpBegin.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain.extent().width);
    viewport.height = static_cast<float>(swapchain.extent().height);
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

    // Draw starfield first (behind everything, no depth write)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      starfieldPipeline.handle());
    starfield.draw(cmd, starfieldPipeline.layout(), vp);

    // Update LOD before drawing — frustum-aware so budget goes to visible
    // patches
    moon.update(camera.position(), camera.fovY(),
                static_cast<double>(swapchain.extent().height), vp);

    // Draw Moon (cubesphere handles per-chunk push constants internally)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      terrainPipeline.handle());
    moon.draw(cmd, terrainPipeline.layout(), vp, camera.position(), sunDir);

    // Draw HUD overlay (screen-space, after all world geometry)
    float aspect = static_cast<float>(swapchain.extent().width) /
                   static_cast<float>(swapchain.extent().height);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      hudPipeline.handle());
    hud.draw(cmd, hudPipeline.layout(), simState, aspect, vp);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    // Submit
    VkSemaphore waitSems[] = {sync.imageAvailable(currentSemaphore)};
    VkSemaphore signalSems[] = {sync.renderFinished(currentSemaphore)};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSems;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSems;

    vkQueueSubmit(ctx.graphicsQueue(), 1, &submitInfo,
                  sync.inFlight(currentFrame));

    VkSwapchainKHR swapchains[] = {swapchain.handle()};
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSems;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(ctx.presentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        framebufferResized) {
      framebufferResized = false;
      if (!swapchain.recreate())
        break;
      renderPass.recreateFramebuffers(swapchain);
      sync.recreateSemaphores(swapchain.imageCount());
      currentSemaphore = 0;
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    currentSemaphore = (currentSemaphore + 1) % sync.semaphoreCount();
  }

  // Hide window immediately so the OS doesn't show "not responding"
  glfwHideWindow(ctx.window());

  // Drain in-flight fences before global device wait
  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkFence f = sync.inFlight(i);
    vkWaitForFences(ctx.device(), 1, &f, VK_TRUE, 200'000'000);
  }

  vkDeviceWaitIdle(ctx.device());

  // Free cubesphere GPU resources in a flat traversal before the destructor
  // chain tears down the quadtree — avoids deep recursive Vulkan calls.
  moon.releaseGPU();

  luna::sim::shutdownTerrain();
  LOG_INFO("Luna shutting down");
  return 0;
}
