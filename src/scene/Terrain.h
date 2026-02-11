// About: Procedural lunar terrain mesh generation with heightmap and normal computation.

#pragma once

#include "scene/Mesh.h"
#include "util/Math.h"
#include <vector>

namespace luna::core {
class VulkanContext;
class CommandPool;
}

namespace luna::scene {

struct TerrainVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

class Terrain {
public:
    // Generate a terrain patch centered at given lat/lon (radians)
    // gridSize: width/height in meters, resolution: vertices per side
    Terrain(const luna::core::VulkanContext& ctx, const luna::core::CommandPool& cmdPool,
            double centerLat = 0.0, double centerLon = 0.0,
            double gridSize = 10'000.0, uint32_t resolution = 256);

    void draw(VkCommandBuffer cmd) const;

private:
    // Procedural heightmap: elevation offset from lunar radius in meters
    static double sampleHeight(double lat, double lon);

    // Convert lat/lon/elevation to Moon-centered XYZ
    static glm::dvec3 latLonToCartesian(double lat, double lon, double radius);

    Mesh mesh_;
};

} // namespace luna::scene
