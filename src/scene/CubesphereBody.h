// About: Spherical Moon represented as a cubesphere with per-chunk camera-relative rendering.

#pragma once

#include "scene/Mesh.h"
#include "util/Math.h"
#include <vector>
#include <vulkan/vulkan.h>

namespace luna::core {
class VulkanContext;
class CommandPool;
}

namespace luna::scene {

class CubesphereBody {
public:
    // Build a cubesphere at the given radius with fixed LOD subdivision depth.
    // depth 4 = 256 patches per face = 1,536 total.
    CubesphereBody(const luna::core::VulkanContext& ctx,
                   const luna::core::CommandPool& cmdPool,
                   double radius, uint32_t subdivisionDepth = 4);

    // Record draw commands for all chunks. Computes per-chunk camera offset internally.
    void draw(VkCommandBuffer cmd, VkPipelineLayout layout,
              const glm::mat4& viewProj,
              const glm::dvec3& cameraPos,
              const glm::vec4& sunDirection) const;

private:
    struct Chunk {
        Mesh mesh;
        glm::dvec3 worldCenter;
    };

    // Recursively subdivide a face region and generate chunks at target depth
    void buildFace(const luna::core::VulkanContext& ctx,
                   const luna::core::CommandPool& cmdPool,
                   int faceIndex,
                   double u0, double u1, double v0, double v1,
                   double radius, uint32_t currentDepth, uint32_t targetDepth);

    std::vector<Chunk> chunks_;
};

} // namespace luna::scene
