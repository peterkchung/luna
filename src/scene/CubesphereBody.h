// About: Spherical Moon as a cubesphere with dynamic quadtree LOD and frustum culling.

#pragma once

#include "scene/Mesh.h"
#include "util/Math.h"
#include <array>
#include <memory>
#include <vulkan/vulkan.h>

namespace luna::core {
class VulkanContext;
class CommandPool;
struct StagingBatch;
}

namespace luna::scene {

struct QuadtreeNode {
    int      faceIndex;
    double   u0, u1, v0, v1;
    uint32_t depth;

    glm::dvec3 worldCenter;
    double     boundingRadius;  // conservative radius around worldCenter

    std::unique_ptr<Mesh> mesh;
    std::array<std::unique_ptr<QuadtreeNode>, 4> children;

    bool isLeaf() const { return !children[0]; }
    bool hasMesh() const { return mesh != nullptr; }
};

class CubesphereBody {
public:
    CubesphereBody(const luna::core::VulkanContext& ctx,
                   const luna::core::CommandPool& cmdPool,
                   double radius);

    // Update LOD based on camera position. Call once per frame before draw().
    void update(const glm::dvec3& cameraPos, double fovY, double screenHeight);

    // Record draw commands for visible leaf nodes.
    void draw(VkCommandBuffer cmd, VkPipelineLayout layout,
              const glm::mat4& viewProj,
              const glm::dvec3& cameraPos,
              const glm::vec4& sunDirection) const;

    uint32_t activeNodeCount() const { return activeNodes_; }

private:
    static constexpr uint32_t MAX_DEPTH            = 15;
    static constexpr uint32_t PATCH_GRID           = 17;
    static constexpr double   SPLIT_THRESHOLD      = 4.0;
    static constexpr double   MERGE_THRESHOLD      = 2.0;
    static constexpr uint32_t MAX_SPLITS_PER_FRAME = 64;

    // Max meshes per batch before flushing the command buffer
    static constexpr uint32_t MESHES_PER_BATCH     = 512;

    // Approximate bytes per mesh for staging capacity estimation
    static constexpr VkDeviceSize BYTES_PER_MESH =
        PATCH_GRID * PATCH_GRID * sizeof(float) * 7  // vertices (position + normal + height)
        + (PATCH_GRID - 1) * (PATCH_GRID - 1) * 6 * sizeof(uint32_t)  // indices
        + PATCH_GRID * 4 * (sizeof(float) * 7 + 6 * sizeof(uint32_t)); // skirt (conservative)

    void initNode(QuadtreeNode& node, int face,
                  double u0, double u1, double v0, double v1, uint32_t depth);

    // Blocking single-mesh upload (used for merges outside a batch)
    void generateMesh(QuadtreeNode& node);

    // Batched mesh upload; auto-flushes every MESHES_PER_BATCH meshes
    void generateMeshBatched(QuadtreeNode& node, VkCommandBuffer& cmd,
                             luna::core::StagingBatch& staging);

    void updateNode(QuadtreeNode& node, const glm::dvec3& cameraPos,
                    double fovY, double screenHeight, uint32_t& splitBudget,
                    VkCommandBuffer& cmd, luna::core::StagingBatch& staging);

    void drawNode(const QuadtreeNode& node, VkCommandBuffer cmd, VkPipelineLayout layout,
                  const glm::mat4& viewProj, const glm::dvec3& cameraPos,
                  const glm::vec4& sunDirection, const glm::vec4 frustumPlanes[6]) const;

    static void extractFrustumPlanes(const glm::mat4& vp, glm::vec4 planes[6]);
    static bool sphereInFrustum(const glm::vec4 planes[6],
                                const glm::vec3& center, float radius);

    double radius_;
    std::array<std::unique_ptr<QuadtreeNode>, 6> roots_;

    // Stored for on-the-fly mesh creation
    const luna::core::VulkanContext* ctx_;
    const luna::core::CommandPool*   cmdPool_;

    uint32_t activeNodes_ = 0;
    uint32_t batchCount_ = 0;

    // Meshes replaced during a batch whose VRAM buffers are still referenced
    // by the in-flight transfer command buffer. Destroyed after submit.
    std::vector<std::unique_ptr<Mesh>> deferredDestroy_;
};

} // namespace luna::scene
