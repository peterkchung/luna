// About: Cubesphere patch mesh generation — projects cube face onto sphere with heightmap.

#include "scene/ChunkGenerator.h"
#include "sim/TerrainQuery.h"

#include <cmath>

namespace luna::scene {

glm::dvec3 ChunkGenerator::facePointToSphere(int face, double u, double v) {
    glm::dvec3 p;
    switch (face) {
        case 0: p = glm::dvec3( 1.0,   v,   u); break; // +X
        case 1: p = glm::dvec3(-1.0,   v,  -u); break; // -X
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

    // Generate vertex positions and heights
    for (uint32_t j = 0; j < gridSize; j++) {
        for (uint32_t i = 0; i < gridSize; i++) {
            double u = u0 + i * uStep;
            double v = v0 + j * vStep;

            glm::dvec3 dir = facePointToSphere(faceIndex, u, v);

            // Convert sphere direction to lat/lon for heightmap sampling
            double lat = std::asin(dir.y);
            double lon = std::atan2(dir.z, dir.x);

            double h = luna::sim::sampleTerrainHeight(lat, lon);
            glm::dvec3 worldPos = dir * (radius + h);

            uint32_t idx = j * gridSize + i;
            data.vertices[idx].position = glm::vec3(worldPos - data.worldCenter);
            data.vertices[idx].height = static_cast<float>(h);
        }
    }

    // Compute normals via central differencing
    double dU = uStep * 0.5;
    double dV = vStep * 0.5;

    for (uint32_t j = 0; j < gridSize; j++) {
        for (uint32_t i = 0; i < gridSize; i++) {
            double u = u0 + i * uStep;
            double v = v0 + j * vStep;

            // Sample 4 neighbors
            auto sampleWorld = [&](double su, double sv) -> glm::dvec3 {
                glm::dvec3 d = facePointToSphere(faceIndex, su, sv);
                double lat = std::asin(d.y);
                double lon = std::atan2(d.z, d.x);
                double h = luna::sim::sampleTerrainHeight(lat, lon);
                return d * (radius + h);
            };

            glm::dvec3 pL = sampleWorld(u - dU, v);
            glm::dvec3 pR = sampleWorld(u + dU, v);
            glm::dvec3 pD = sampleWorld(u, v - dV);
            glm::dvec3 pU = sampleWorld(u, v + dV);

            glm::dvec3 tangentU = pR - pL;
            glm::dvec3 tangentV = pU - pD;
            glm::dvec3 normal = glm::normalize(glm::cross(tangentU, tangentV));

            data.vertices[j * gridSize + i].normal = glm::vec3(normal);
        }
    }

    // Generate triangle-list indices from grid
    uint32_t quads = gridSize - 1;
    data.indices.reserve(quads * quads * 6);

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

    return data;
}

} // namespace luna::scene
