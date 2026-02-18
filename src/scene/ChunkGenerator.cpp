// About: Cubesphere patch mesh generation — projects cube face onto sphere with heightmap displacement.

#include "scene/ChunkGenerator.h"
#include "sim/TerrainQuery.h"

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

namespace {

// Sphere direction to lat/lon (Y-up: Y is polar axis)
inline void dirToLatLon(const glm::dvec3& dir, double& lat, double& lon) {
    lat = std::asin(glm::clamp(dir.y, -1.0, 1.0));
    lon = std::atan2(dir.z, dir.x);
}

// Sample displaced world position from face UV coordinates
inline glm::dvec3 sampleWorldPos(int face, double u, double v, double radius) {
    glm::dvec3 dir = ChunkGenerator::facePointToSphere(face, u, v);
    double lat, lon;
    dirToLatLon(dir, lat, lon);
    double h = luna::sim::sampleTerrainHeight(lat, lon);
    return dir * (radius + h);
}

} // anonymous namespace

ChunkMeshData ChunkGenerator::generate(int faceIndex,
                                        double u0, double u1,
                                        double v0, double v1,
                                        double radius,
                                        uint32_t gridSize) {
    ChunkMeshData data;

    double uStep = (u1 - u0) / static_cast<double>(gridSize - 1);
    double vStep = (v1 - v0) / static_cast<double>(gridSize - 1);

    // Compute world center with terrain displacement
    double uMid = (u0 + u1) * 0.5;
    double vMid = (v0 + v1) * 0.5;
    data.worldCenter = sampleWorldPos(faceIndex, uMid, vMid, radius);

    uint32_t vertCount = gridSize * gridSize;
    data.vertices.resize(vertCount);

    // Half-step offsets for central differencing normals
    double halfU = uStep * 0.5;
    double halfV = vStep * 0.5;

    for (uint32_t j = 0; j < gridSize; j++) {
        for (uint32_t i = 0; i < gridSize; i++) {
            double u = u0 + i * uStep;
            double v = v0 + j * vStep;

            glm::dvec3 worldPos = sampleWorldPos(faceIndex, u, v, radius);

            // Central differencing: sample 4 neighbors, compute tangent cross product
            glm::dvec3 pU0 = sampleWorldPos(faceIndex, u - halfU, v, radius);
            glm::dvec3 pU1 = sampleWorldPos(faceIndex, u + halfU, v, radius);
            glm::dvec3 pV0 = sampleWorldPos(faceIndex, u, v - halfV, radius);
            glm::dvec3 pV1 = sampleWorldPos(faceIndex, u, v + halfV, radius);

            glm::dvec3 tangentU = pU1 - pU0;
            glm::dvec3 tangentV = pV1 - pV0;
            glm::dvec3 normal = glm::normalize(glm::cross(tangentU, tangentV));

            double lat, lon;
            dirToLatLon(glm::normalize(worldPos), lat, lon);
            double h = luna::sim::sampleTerrainHeight(lat, lon);

            uint32_t idx = j * gridSize + i;
            data.vertices[idx].position = glm::vec3(worldPos - data.worldCenter);
            data.vertices[idx].normal   = glm::vec3(normal);
            data.vertices[idx].height   = static_cast<float>(h);
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
