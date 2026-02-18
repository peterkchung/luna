// About: Quadtree LOD cubesphere — dynamically splits/merges patches based on screen-space error.

#include "scene/CubesphereBody.h"
#include "scene/ChunkGenerator.h"
#include "core/Buffer.h"
#include "core/CommandPool.h"
#include "core/VulkanContext.h"
#include "util/Log.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace luna::scene {

// Push constant layout must match shaders/terrain.vert
struct TerrainPC {
    glm::mat4 viewProj;
    glm::vec3 cameraOffset;
    float     _pad0;
    glm::vec4 sunDirection;
    glm::vec3 cameraWorldPos;
    float     _pad1;
};

CubesphereBody::CubesphereBody(const luna::core::VulkanContext& ctx,
                               const luna::core::CommandPool& cmdPool,
                               double radius)
    : radius_(radius), ctx_(&ctx), cmdPool_(&cmdPool) {

    // 6 root meshes fit comfortably in one batch (12 device-local BOs + 1 staging = 13)
    luna::core::StagingBatch staging;
    staging.begin(ctx, 6 * BYTES_PER_MESH);
    VkCommandBuffer cmd = cmdPool.beginOneShot();

    for (int face = 0; face < 6; face++) {
        roots_[face] = std::make_unique<QuadtreeNode>();
        initNode(*roots_[face], face, -1.0, 1.0, -1.0, 1.0, 0);
        generateMeshBatched(*roots_[face], cmd, staging);
    }

    // Flush remaining
    if (batchCount_ > 0) {
        staging.end();
        cmdPool.endOneShot(cmd, ctx.graphicsQueue());
    } else {
        vkEndCommandBuffer(cmd);
        vkFreeCommandBuffers(ctx.device(), cmdPool.pool(), 1, &cmd);
    }

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
    // Additive margin for terrain displacement (LOLA range ~-9km to +11km)
    constexpr double MAX_TERRAIN_DISPLACEMENT = 12000.0;
    node.boundingRadius += MAX_TERRAIN_DISPLACEMENT;
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

void CubesphereBody::generateMeshBatched(QuadtreeNode& node, VkCommandBuffer& cmd,
                                          luna::core::StagingBatch& staging) {
    auto meshData = ChunkGenerator::generate(
        node.faceIndex, node.u0, node.u1, node.v0, node.v1, radius_, PATCH_GRID);
    node.worldCenter = meshData.worldCenter;
    node.mesh = std::make_unique<Mesh>(
        *ctx_, cmd,
        meshData.vertices.data(),
        static_cast<uint32_t>(meshData.vertices.size() * sizeof(ChunkVertex)),
        meshData.indices.data(),
        static_cast<uint32_t>(meshData.indices.size()),
        staging);
    batchCount_++;

    // Flush when sub-batch is full to keep BO count per submission low
    if (batchCount_ >= MESHES_PER_BATCH) {
        staging.end();
        cmdPool_->endOneShot(cmd, ctx_->graphicsQueue());

        // Start fresh sub-batch
        staging.begin(*ctx_, MESHES_PER_BATCH * BYTES_PER_MESH);
        cmd = cmdPool_->beginOneShot();
        batchCount_ = 0;
    }
}

void CubesphereBody::update(const glm::dvec3& cameraPos,
                             double fovY, double screenHeight,
                             const glm::mat4& viewProj) {
    activeNodes_ = 0;
    batchCount_ = 0;

    glm::vec4 frustumPlanes[6];
    extractFrustumPlanes(viewProj, frustumPlanes);

    luna::core::StagingBatch staging;
    staging.begin(*ctx_, MESHES_PER_BATCH * BYTES_PER_MESH);
    VkCommandBuffer cmd = cmdPool_->beginOneShot();

    // Sort faces by distance to camera so the closest face gets budget priority
    std::array<int, 6> faceOrder;
    std::iota(faceOrder.begin(), faceOrder.end(), 0);
    std::sort(faceOrder.begin(), faceOrder.end(), [&](int a, int b) {
        double da = glm::length(roots_[a]->worldCenter - cameraPos);
        double db = glm::length(roots_[b]->worldCenter - cameraPos);
        return da < db;
    });

    // Shared budget across all faces, but process closest faces first
    uint32_t splitBudget = MAX_SPLITS_PER_FRAME;
    for (int fi : faceOrder) {
        updateNode(*roots_[fi], cameraPos, fovY, screenHeight, splitBudget, cmd, staging, frustumPlanes);
    }

    // Flush remaining meshes
    if (batchCount_ > 0) {
        staging.end();
        cmdPool_->endOneShot(cmd, ctx_->graphicsQueue());
    } else {
        staging.end();
        vkEndCommandBuffer(cmd);
        vkFreeCommandBuffers(ctx_->device(), cmdPool_->pool(), 1, &cmd);
    }

    // Safe to destroy now — transfer command buffer has finished
    deferredDestroy_.clear();
}

void CubesphereBody::updateNode(QuadtreeNode& node, const glm::dvec3& cameraPos,
                                 double fovY, double screenHeight,
                                 uint32_t& splitBudget,
                                 VkCommandBuffer& cmd,
                                 luna::core::StagingBatch& staging,
                                 const glm::vec4 frustumPlanes[6]) {
    double distance = glm::length(node.worldCenter - cameraPos);
    // Avoid division by zero for camera inside the node's bounding sphere
    distance = glm::max(distance, node.boundingRadius * 0.1);

    // Geometric error: approximate as the arc length of one grid cell
    double patchArc = node.boundingRadius * 2.0;
    double geometricError = patchArc / static_cast<double>(PATCH_GRID - 1);

    // Project geometric error to screen pixels
    double screenError = (geometricError / distance) *
                         (screenHeight / (2.0 * std::tan(fovY * 0.5)));

    // Test visibility — only spend split budget on nodes the camera can see
    glm::vec3 offset = glm::vec3(node.worldCenter - cameraPos);
    bool visible = sphereInFrustum(frustumPlanes, offset, static_cast<float>(node.boundingRadius));

    if (node.isLeaf()) {
        // Only split visible nodes — off-screen nodes keep their current detail
        if (visible && screenError > SPLIT_THRESHOLD && node.depth < MAX_DEPTH && splitBudget >= 4) {
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
                generateMeshBatched(*child, cmd, staging);
            }
            splitBudget -= 4;

            // Defer parent mesh destruction until after batch submit
            if (node.mesh)
                deferredDestroy_.push_back(std::move(node.mesh));

            // Sort children by distance to camera so closest gets budget first
            std::array<int, 4> childOrder = {0, 1, 2, 3};
            std::sort(childOrder.begin(), childOrder.end(), [&](int a, int b) {
                return glm::length(node.children[a]->worldCenter - cameraPos) <
                       glm::length(node.children[b]->worldCenter - cameraPos);
            });
            for (int ci : childOrder) {
                updateNode(*node.children[ci], cameraPos, fovY, screenHeight, splitBudget, cmd, staging, frustumPlanes);
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
            // Merge: regenerate parent mesh, defer child destruction
            if (!node.hasMesh()) {
                generateMeshBatched(node, cmd, staging);
            }
            for (auto& child : node.children) {
                if (child && child->mesh)
                    deferredDestroy_.push_back(std::move(child->mesh));
                child.reset();
            }
            activeNodes_++;
        } else {
            // Sort children by distance to camera for balanced recursion
            std::array<int, 4> childOrder = {0, 1, 2, 3};
            std::sort(childOrder.begin(), childOrder.end(), [&](int a, int b) {
                if (!node.children[a]) return false;
                if (!node.children[b]) return true;
                return glm::length(node.children[a]->worldCenter - cameraPos) <
                       glm::length(node.children[b]->worldCenter - cameraPos);
            });
            for (int ci : childOrder) {
                if (node.children[ci])
                    updateNode(*node.children[ci], cameraPos, fovY, screenHeight, splitBudget, cmd, staging, frustumPlanes);
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
            pc.cameraWorldPos = glm::vec3(cameraPos);

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
