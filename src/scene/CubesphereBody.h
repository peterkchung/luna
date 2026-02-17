// About: Spherical Moon as a cubesphere with dynamic quadtree LOD and frustum culling.

#pragma once

#include "scene/Mesh.h"
#include "core/Buffer.h"
#include "util/Math.h"
#include <array>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace luna::core {
class VulkanContext;
class CommandPool;
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
    static constexpr uint32_t MAX_DEPTH       = 15;
    static constexpr uint32_t PATCH_GRID      = 33;
    static constexpr double   SPLIT_THRESHOLD = 2.0;
    static constexpr double   MERGE_THRESHOLD = 1.0;
    static constexpr uint32_t MAX_SPLITS_PER_FRAME = 64;

    void initNode(QuadtreeNode& node, int face,
                  double u0, double u1, double v0, double v1, uint32_t depth);
    void generateMesh(QuadtreeNode& node);
    void generateMeshBatched(QuadtreeNode& node, VkCommandBuffer transferCmd,
                             std::vector<luna::core::Buffer>& staging);
    void updateNode(QuadtreeNode& node, const glm::dvec3& cameraPos,
                    double fovY, double screenHeight, uint32_t& splitBudget,
                    VkCommandBuffer transferCmd,
                    std::vector<luna::core::Buffer>& staging);
    void drawNode(const QuadtreeNode& node, VkCommandBuffer cmd, VkPipelineLayout layout,
                  const glm::mat4& viewProj, const glm::dvec3& cameraPos,
                  const glm::vec4& sunDirection, const glm::vec4 frustumPlanes[6]) const;

    // Release previous frame's staging buffers once their transfer is confirmed complete
    void retirePendingTransfer();

    static void extractFrustumPlanes(const glm::mat4& vp, glm::vec4 planes[6]);
    static bool sphereInFrustum(const glm::vec4 planes[6],
                                const glm::vec3& center, float radius);

    double radius_;
    std::array<std::unique_ptr<QuadtreeNode>, 6> roots_;

    const luna::core::VulkanContext* ctx_;
    const luna::core::CommandPool*   cmdPool_;

    // Pending transfer from previous frame
    VkFence                          transferFence_ = VK_NULL_HANDLE;
    VkCommandBuffer                  transferCmd_   = VK_NULL_HANDLE;
    std::vector<luna::core::Buffer>  pendingStaging_;

    uint32_t activeNodes_ = 0;
};

} // namespace luna::scene
