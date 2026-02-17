// About: Quadtree LOD cubesphere — dynamically splits/merges patches based on screen-space error.

#include "scene/CubesphereBody.h"
#include "scene/ChunkGenerator.h"
#include "core/VulkanContext.h"
#include "core/CommandPool.h"
#include "util/Log.h"

#include <cmath>

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
                               double radius)
    : radius_(radius), ctx_(&ctx), cmdPool_(&cmdPool) {

    VkCommandBuffer transferCmd = cmdPool_->beginOneShot();
    std::vector<luna::core::Buffer> staging;

    for (int face = 0; face < 6; face++) {
        roots_[face] = std::make_unique<QuadtreeNode>();
        initNode(*roots_[face], face, -1.0, 1.0, -1.0, 1.0, 0);
        generateMeshBatched(*roots_[face], transferCmd, staging);
    }

    // Submit all 6 root uploads in one batch, block for startup only
    cmdPool_->endOneShot(transferCmd, ctx_->graphicsQueue());

    LOG_INFO("Cubesphere initialized with 6 root nodes");
}

void CubesphereBody::initNode(QuadtreeNode& node, int face,
                               double u0, double u1, double v0, double v1,
                               uint32_t depth) {
    node.faceIndex = face;
    node.u0 = u0;
    node.u1 = u1;
    node.v0 = v0;
    node.v1 = v1;
    node.depth = depth;

    double uMid = (u0 + u1) * 0.5;
    double vMid = (v0 + v1) * 0.5;
    node.worldCenter = ChunkGenerator::facePointToSphere(face, uMid, vMid) * radius_;

    // Conservative bounding radius: max distance from center to any corner/edge midpoint
    glm::dvec3 testPoints[] = {
        ChunkGenerator::facePointToSphere(face, u0, v0) * radius_,
        ChunkGenerator::facePointToSphere(face, u1, v0) * radius_,
        ChunkGenerator::facePointToSphere(face, u0, v1) * radius_,
        ChunkGenerator::facePointToSphere(face, u1, v1) * radius_,
        ChunkGenerator::facePointToSphere(face, uMid, v0) * radius_,
        ChunkGenerator::facePointToSphere(face, uMid, v1) * radius_,
        ChunkGenerator::facePointToSphere(face, u0, vMid) * radius_,
        ChunkGenerator::facePointToSphere(face, u1, vMid) * radius_,
    };
    node.boundingRadius = 0.0;
    for (const auto& p : testPoints) {
        node.boundingRadius = glm::max(node.boundingRadius, glm::length(p - node.worldCenter));
    }
    node.boundingRadius *= 1.05; // 5% margin for heightmap displacement
}

void CubesphereBody::generateMesh(QuadtreeNode& node) {
    auto meshData = ChunkGenerator::generate(
        node.faceIndex, node.u0, node.u1, node.v0, node.v1, radius_, PATCH_GRID);
    node.worldCenter = meshData.worldCenter;
    node.mesh = std::make_unique<Mesh>(
        *ctx_, *cmdPool_,
        meshData.vertices.data(),
        static_cast<uint32_t>(meshData.vertices.size() * sizeof(ChunkVertex)),
        meshData.indices.data(),
        static_cast<uint32_t>(meshData.indices.size()));
}

void CubesphereBody::generateMeshBatched(QuadtreeNode& node,
                                          VkCommandBuffer transferCmd,
                                          std::vector<luna::core::Buffer>& staging) {
    auto meshData = ChunkGenerator::generate(
        node.faceIndex, node.u0, node.u1, node.v0, node.v1, radius_, PATCH_GRID);
    node.worldCenter = meshData.worldCenter;
    node.mesh = std::make_unique<Mesh>(
        *ctx_, transferCmd,
        meshData.vertices.data(),
        static_cast<uint32_t>(meshData.vertices.size() * sizeof(ChunkVertex)),
        meshData.indices.data(),
        static_cast<uint32_t>(meshData.indices.size()),
        staging);
}

void CubesphereBody::retirePendingTransfer() {
    if (transferFence_ == VK_NULL_HANDLE) return;

    vkWaitForFences(ctx_->device(), 1, &transferFence_, VK_TRUE, UINT64_MAX);
    vkDestroyFence(ctx_->device(), transferFence_, nullptr);
    vkFreeCommandBuffers(ctx_->device(), cmdPool_->pool(), 1, &transferCmd_);

    transferFence_ = VK_NULL_HANDLE;
    transferCmd_   = VK_NULL_HANDLE;
    pendingStaging_.clear();
}

void CubesphereBody::update(const glm::dvec3& cameraPos,
                             double fovY, double screenHeight) {
    // Wait for previous frame's transfers before releasing staging memory
    retirePendingTransfer();

    VkCommandBuffer transferCmd = cmdPool_->beginOneShot();
    std::vector<luna::core::Buffer> staging;

    uint32_t splitBudget = MAX_SPLITS_PER_FRAME;
    activeNodes_ = 0;

    for (auto& root : roots_) {
        updateNode(*root, cameraPos, fovY, screenHeight, splitBudget,
                   transferCmd, staging);
    }

    if (!staging.empty()) {
        // Make transfer writes visible to subsequent vertex/index reads
        VkMemoryBarrier barrier{};
        barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
        vkCmdPipelineBarrier(transferCmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);

        transferFence_ = cmdPool_->endOneShotWithFence(transferCmd, ctx_->graphicsQueue());
        transferCmd_   = transferCmd;
        pendingStaging_ = std::move(staging);
    } else {
        // No uploads this frame — discard the unused command buffer
        vkEndCommandBuffer(transferCmd);
        vkFreeCommandBuffers(ctx_->device(), cmdPool_->pool(), 1, &transferCmd);
    }
}

void CubesphereBody::updateNode(QuadtreeNode& node, const glm::dvec3& cameraPos,
                                 double fovY, double screenHeight,
                                 uint32_t& splitBudget,
                                 VkCommandBuffer transferCmd,
                                 std::vector<luna::core::Buffer>& staging) {
    double distance = glm::length(node.worldCenter - cameraPos);
    // Avoid division by zero for camera inside the node's bounding sphere
    distance = glm::max(distance, node.boundingRadius * 0.1);

    // Geometric error: approximate as the arc length of one grid cell
    // Patch arc ≈ 2 * radius * sin(patchAngle/2), cell = patchArc / (PATCH_GRID - 1)
    double patchArc = node.boundingRadius * 2.0;
    double geometricError = patchArc / static_cast<double>(PATCH_GRID - 1);

    // Project geometric error to screen pixels
    double screenError = (geometricError / distance) *
                         (screenHeight / (2.0 * std::tan(fovY * 0.5)));

    if (node.isLeaf()) {
        // Consider splitting — require budget for all 4 children to avoid partial splits
        if (screenError > SPLIT_THRESHOLD && node.depth < MAX_DEPTH && splitBudget >= 4) {
            double uMid = (node.u0 + node.u1) * 0.5;
            double vMid = (node.v0 + node.v1) * 0.5;

            node.children[0] = std::make_unique<QuadtreeNode>();
            node.children[1] = std::make_unique<QuadtreeNode>();
            node.children[2] = std::make_unique<QuadtreeNode>();
            node.children[3] = std::make_unique<QuadtreeNode>();

            initNode(*node.children[0], node.faceIndex, node.u0, uMid, node.v0, vMid, node.depth + 1);
            initNode(*node.children[1], node.faceIndex, uMid, node.u1, node.v0, vMid, node.depth + 1);
            initNode(*node.children[2], node.faceIndex, node.u0, uMid, vMid, node.v1, node.depth + 1);
            initNode(*node.children[3], node.faceIndex, uMid, node.u1, vMid, node.v1, node.depth + 1);

            for (auto& child : node.children) {
                generateMeshBatched(*child, transferCmd, staging);
            }
            splitBudget -= 4;

            // All children have meshes, release parent
            node.mesh.reset();

            // Recurse into children
            for (auto& child : node.children) {
                updateNode(*child, cameraPos, fovY, screenHeight, splitBudget,
                           transferCmd, staging);
            }
        } else {
            activeNodes_++;
        }
    } else {
        // Interior node — check if we should merge children back
        bool allChildrenLeaves = true;
        double maxChildError = 0.0;
        for (auto& child : node.children) {
            if (!child || !child->isLeaf()) {
                allChildrenLeaves = false;
                break;
            }
            double childDist = glm::length(child->worldCenter - cameraPos);
            childDist = glm::max(childDist, child->boundingRadius * 0.1);
            double childArc = child->boundingRadius * 2.0;
            double childGeoError = childArc / static_cast<double>(PATCH_GRID - 1);
            double childScreenError = (childGeoError / childDist) *
                                      (screenHeight / (2.0 * std::tan(fovY * 0.5)));
            maxChildError = glm::max(maxChildError, childScreenError);
        }

        if (allChildrenLeaves && maxChildError < MERGE_THRESHOLD) {
            // Merge: regenerate parent mesh, destroy children
            if (!node.hasMesh()) {
                generateMeshBatched(node, transferCmd, staging);
            }
            for (auto& child : node.children) {
                child.reset();
            }
            activeNodes_++;
        } else {
            // Recurse into children
            for (auto& child : node.children) {
                if (child) updateNode(*child, cameraPos, fovY, screenHeight, splitBudget,
                                      transferCmd, staging);
            }
        }
    }
}

void CubesphereBody::extractFrustumPlanes(const glm::mat4& vp, glm::vec4 planes[6]) {
    // Extract 6 frustum planes from VP matrix (Gribb/Hartmann method)
    for (int i = 0; i < 4; i++) {
        planes[0][i] = vp[i][3] + vp[i][0]; // left
        planes[1][i] = vp[i][3] - vp[i][0]; // right
        planes[2][i] = vp[i][3] + vp[i][1]; // bottom
        planes[3][i] = vp[i][3] - vp[i][1]; // top
        planes[4][i] = vp[i][3] + vp[i][2]; // near
        planes[5][i] = vp[i][3] - vp[i][2]; // far
    }
    // Normalize planes
    for (int i = 0; i < 6; i++) {
        float len = glm::length(glm::vec3(planes[i]));
        if (len > 0.0f) planes[i] /= len;
    }
}

bool CubesphereBody::sphereInFrustum(const glm::vec4 planes[6],
                                      const glm::vec3& center, float radius) {
    for (int i = 0; i < 6; i++) {
        float dist = glm::dot(glm::vec3(planes[i]), center) + planes[i].w;
        if (dist < -radius) return false;
    }
    return true;
}

void CubesphereBody::draw(VkCommandBuffer cmd, VkPipelineLayout layout,
                           const glm::mat4& viewProj,
                           const glm::dvec3& cameraPos,
                           const glm::vec4& sunDirection) const {
    glm::vec4 frustumPlanes[6];
    extractFrustumPlanes(viewProj, frustumPlanes);

    for (const auto& root : roots_) {
        drawNode(*root, cmd, layout, viewProj, cameraPos, sunDirection, frustumPlanes);
    }
}

void CubesphereBody::drawNode(const QuadtreeNode& node, VkCommandBuffer cmd,
                               VkPipelineLayout layout,
                               const glm::mat4& viewProj,
                               const glm::dvec3& cameraPos,
                               const glm::vec4& sunDirection,
                               const glm::vec4 frustumPlanes[6]) const {
    // Frustum cull: test bounding sphere in camera-relative space
    glm::vec3 offset = glm::vec3(node.worldCenter - cameraPos);
    if (!sphereInFrustum(frustumPlanes, offset, static_cast<float>(node.boundingRadius))) {
        return;
    }

    if (node.isLeaf()) {
        if (node.hasMesh()) {
            TerrainPC pc{};
            pc.viewProj = viewProj;
            pc.cameraOffset = offset;
            pc.sunDirection = sunDirection;

            vkCmdPushConstants(cmd, layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(TerrainPC), &pc);
            node.mesh->draw(cmd);
        }
        return;
    }

    // Interior node: recurse into children
    for (const auto& child : node.children) {
        if (child) drawNode(*child, cmd, layout, viewProj, cameraPos, sunDirection, frustumPlanes);
    }
}

} // namespace luna::scene
