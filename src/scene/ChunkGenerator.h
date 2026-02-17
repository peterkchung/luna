// About: Generates vertex/index mesh data for a single cubesphere patch.

#pragma once

#include "util/Math.h"
#include <vector>
#include <cstdint>

namespace luna::scene {

struct ChunkVertex {
    glm::vec3 position;   // relative to chunk center
    glm::vec3 normal;
    float     height;     // elevation above LUNAR_RADIUS in meters
};

struct ChunkMeshData {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t>    indices;
    glm::dvec3               worldCenter;
};

class ChunkGenerator {
public:
    // Generate mesh data for a cubesphere patch defined by face and UV bounds.
    // gridSize = vertices per edge (e.g. 33 → 32x32 quads → 2048 triangles)
    static ChunkMeshData generate(int faceIndex,
                                  double u0, double u1,
                                  double v0, double v1,
                                  double radius,
                                  uint32_t gridSize = 33);

private:
    // Map (face, u, v) to a unit sphere direction vector
    static glm::dvec3 facePointToSphere(int face, double u, double v);
};

} // namespace luna::scene
