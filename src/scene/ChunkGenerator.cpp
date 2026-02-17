// About: Cubesphere patch mesh generation — projects cube face onto sphere with heightmap.

#include "scene/ChunkGenerator.h"

#include <cmath>

namespace luna::scene {

glm::dvec3 ChunkGenerator::facePointToSphere(int face, double u, double v) {
    glm::dvec3 p;
    switch (face) {
        case 0: p = glm::dvec3( 1.0,   u,   v); break; // +X
        case 1: p = glm::dvec3(-1.0,  -u,   v); break; // -X
        case 2: p = glm::dvec3(   u, 1.0,  -v); break; // +Y
        case 3: p = glm::dvec3(   u,-1.0,   v); break; // -Y
        case 4: p = glm::dvec3(   u,   v, 1.0); break; // +Z
        case 5: p = glm::dvec3(  -u,   v,-1.0); break; // -Z
        default: p = glm::dvec3(1.0, 0.0, 0.0); break;
    }
    return glm::normalize(p);
}

ChunkMeshData ChunkGenerator::generate(int faceIndex,
                                        double u0, double u1,
                                        double v0, double v1,
                                        double radius,
                                        uint32_t gridSize) {
    ChunkMeshData data;

    double uMid = (u0 + u1) * 0.5;
    double vMid = (v0 + v1) * 0.5;
    data.worldCenter = facePointToSphere(faceIndex, uMid, vMid) * radius;

    uint32_t vertCount = gridSize * gridSize;
    data.vertices.resize(vertCount);

    double uStep = (u1 - u0) / static_cast<double>(gridSize - 1);
    double vStep = (v1 - v0) / static_cast<double>(gridSize - 1);

    // Generate vertex positions with sphere normals (flat terrain, no heightmap)
    for (uint32_t j = 0; j < gridSize; j++) {
        for (uint32_t i = 0; i < gridSize; i++) {
            double u = u0 + i * uStep;
            double v = v0 + j * vStep;

            glm::dvec3 dir = facePointToSphere(faceIndex, u, v);
            glm::dvec3 worldPos = dir * radius;

            uint32_t idx = j * gridSize + i;
            data.vertices[idx].position = glm::vec3(worldPos - data.worldCenter);
            data.vertices[idx].normal   = glm::vec3(dir);
            data.vertices[idx].height   = 0.0f;
        }
    }

    // Generate triangle-list indices from grid
    uint32_t quads = gridSize - 1;
    data.indices.reserve(quads * quads * 6 + 4 * quads * 6);

    for (uint32_t j = 0; j < quads; j++) {
        for (uint32_t i = 0; i < quads; i++) {
            uint32_t tl = j * gridSize + i;
            uint32_t tr = tl + 1;
            uint32_t bl = (j + 1) * gridSize + i;
            uint32_t br = bl + 1;

            data.indices.push_back(tl);
            data.indices.push_back(bl);
            data.indices.push_back(tr);

            data.indices.push_back(tr);
            data.indices.push_back(bl);
            data.indices.push_back(br);
        }
    }

    // Skirt geometry — fills T-junction gaps between patches at different LOD levels.
    // Each edge gets a strip of triangles hanging inward toward the Moon center.
    double skirtDepth = (u1 - u0) * radius / static_cast<double>(gridSize - 1);

    auto addSkirt = [&](uint32_t startIdx, uint32_t stride, uint32_t count, bool flip) {
        uint32_t skirtBase = static_cast<uint32_t>(data.vertices.size());
        for (uint32_t k = 0; k < count; k++) {
            uint32_t edgeIdx = startIdx + k * stride;
            ChunkVertex sv = data.vertices[edgeIdx];
            glm::dvec3 worldPos = glm::dvec3(sv.position) + data.worldCenter;
            glm::dvec3 dir = glm::normalize(worldPos);
            worldPos -= dir * skirtDepth;
            sv.position = glm::vec3(worldPos - data.worldCenter);
            data.vertices.push_back(sv);
        }
        for (uint32_t k = 0; k < count - 1; k++) {
            uint32_t e0 = startIdx + k * stride;
            uint32_t e1 = startIdx + (k + 1) * stride;
            uint32_t s0 = skirtBase + k;
            uint32_t s1 = skirtBase + k + 1;
            if (flip) {
                data.indices.push_back(e0);
                data.indices.push_back(e1);
                data.indices.push_back(s0);
                data.indices.push_back(s0);
                data.indices.push_back(e1);
                data.indices.push_back(s1);
            } else {
                data.indices.push_back(e0);
                data.indices.push_back(s0);
                data.indices.push_back(e1);
                data.indices.push_back(e1);
                data.indices.push_back(s0);
                data.indices.push_back(s1);
            }
        }
    };

    // Bottom edge (j=0): skirt faces away from patch interior (downward in v)
    addSkirt(0, 1, gridSize, false);
    // Top edge (j=gridSize-1): skirt faces away from interior (upward in v)
    addSkirt((gridSize - 1) * gridSize, 1, gridSize, true);
    // Left edge (i=0): skirt faces left
    addSkirt(0, gridSize, gridSize, true);
    // Right edge (i=gridSize-1): skirt faces right
    addSkirt(gridSize - 1, gridSize, gridSize, false);

    return data;
}

} // namespace luna::scene
