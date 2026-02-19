// About: Procedural starfield — random point cloud on the unit sphere rendered as GL points.

#pragma once

#include "scene/Mesh.h"
#include "util/Math.h"
#include <vulkan/vulkan.h>

namespace luna::core {
class VulkanContext;
class CommandPool;
}

namespace luna::scene {

struct StarVertex {
    glm::vec3 direction;
    float     brightness;
};

class Starfield {
public:
    Starfield(const luna::core::VulkanContext& ctx,
              const luna::core::CommandPool& cmdPool,
              uint32_t starCount = 5000);

    void draw(VkCommandBuffer cmd, VkPipelineLayout layout,
              const glm::mat4& viewProj) const;

    void releaseGPU() { mesh_.release(); }

private:
    Mesh mesh_;
};

} // namespace luna::scene
