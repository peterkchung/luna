// About: Cubesphere implementation — builds all chunks at construction, draws with camera-relative offsets.

#include "scene/CubesphereBody.h"
#include "scene/ChunkGenerator.h"
#include "util/Log.h"

#include <cstring>

namespace luna::scene {

// Push constant layout must match shaders/terrain.vert
struct TerrainPC {
    glm::mat4 viewProj;
    glm::vec3 cameraOffset;
    float     _pad0;
    glm::vec4 sunDirection;
};

CubesphereBody::CubesphereBody(const luna::core::VulkanContext& ctx,
                               const luna::core::CommandPool& cmdPool,
                               double radius, uint32_t subdivisionDepth) {
    // Pre-allocate: 6 faces × 4^depth patches
    uint32_t patchesPerFace = 1;
    for (uint32_t i = 0; i < subdivisionDepth; i++)
        patchesPerFace *= 4;
    chunks_.reserve(6 * patchesPerFace);

    for (int face = 0; face < 6; face++) {
        buildFace(ctx, cmdPool, face, -1.0, 1.0, -1.0, 1.0,
                  radius, 0, subdivisionDepth);
    }

    LOG_INFO("Cubesphere built: %zu chunks", chunks_.size());
}

void CubesphereBody::buildFace(const luna::core::VulkanContext& ctx,
                                const luna::core::CommandPool& cmdPool,
                                int faceIndex,
                                double u0, double u1, double v0, double v1,
                                double radius, uint32_t currentDepth, uint32_t targetDepth) {
    if (currentDepth == targetDepth) {
        // Leaf node — generate mesh
        auto meshData = ChunkGenerator::generate(faceIndex, u0, u1, v0, v1, radius);

        Chunk chunk;
        chunk.worldCenter = meshData.worldCenter;
        chunk.mesh = Mesh(ctx, cmdPool,
                          meshData.vertices.data(),
                          static_cast<uint32_t>(meshData.vertices.size() * sizeof(ChunkVertex)),
                          meshData.indices.data(),
                          static_cast<uint32_t>(meshData.indices.size()));
        chunks_.push_back(std::move(chunk));
        return;
    }

    // Subdivide into 4 children
    double uMid = (u0 + u1) * 0.5;
    double vMid = (v0 + v1) * 0.5;

    buildFace(ctx, cmdPool, faceIndex, u0,   uMid, v0,   vMid, radius, currentDepth + 1, targetDepth);
    buildFace(ctx, cmdPool, faceIndex, uMid, u1,   v0,   vMid, radius, currentDepth + 1, targetDepth);
    buildFace(ctx, cmdPool, faceIndex, u0,   uMid, vMid, v1,   radius, currentDepth + 1, targetDepth);
    buildFace(ctx, cmdPool, faceIndex, uMid, u1,   vMid, v1,   radius, currentDepth + 1, targetDepth);
}

void CubesphereBody::draw(VkCommandBuffer cmd, VkPipelineLayout layout,
                           const glm::mat4& viewProj,
                           const glm::dvec3& cameraPos,
                           const glm::vec4& sunDirection) const {
    for (const auto& chunk : chunks_) {
        glm::dvec3 offsetD = chunk.worldCenter - cameraPos;
        glm::vec3 offset = glm::vec3(offsetD);

        TerrainPC pc{};
        pc.viewProj = viewProj;
        pc.cameraOffset = offset;
        pc.sunDirection = sunDirection;

        vkCmdPushConstants(cmd, layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(TerrainPC), &pc);

        chunk.mesh.draw(cmd);
    }
}

} // namespace luna::scene
