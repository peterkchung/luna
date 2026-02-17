// About: Generates vertex/index mesh data for a single cubesphere patch.

#pragma once

#include "util/Math.h"
#include <vector>
#include <cstdint>

namespace luna::scene {

struct ChunkVertex {
    glm::vec3 position;   // 12 bytes — relative to chunk center
    int16_t   normalX;    //  2 bytes — octahedron-encoded normal
    int16_t   normalY;    //  2 bytes
    float     height;     //  4 bytes — elevation above LUNAR_RADIUS in meters
};                        // 20 bytes total

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

    // Map (face, u, v) to a unit sphere direction vector
    static glm::dvec3 facePointToSphere(int face, double u, double v);
};

} // namespace luna::scene
