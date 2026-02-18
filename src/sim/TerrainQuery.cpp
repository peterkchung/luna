// About: Lunar heightmap query — delegates to Heightmap TIFF loader, falls back to flat if absent.

#include "sim/TerrainQuery.h"
#include "sim/Heightmap.h"
#include "util/Log.h"

#include <cmath>

namespace luna::sim {

static Heightmap s_heightmap;

bool initTerrain(const std::string& path) {
    if (!s_heightmap.load(path)) {
        LOG_WARN("Terrain data not available — using flat sphere");
        return false;
    }
    return true;
}

void shutdownTerrain() {
    s_heightmap = Heightmap{};
}

double sampleTerrainHeight(double lat, double lon) {
    if (!s_heightmap.isLoaded()) return 0.0;
    return s_heightmap.sample(lat, lon);
}

glm::dvec3 latLonToCartesian(double lat, double lon, double radius) {
    return glm::dvec3(
        radius * std::cos(lat) * std::cos(lon),
        radius * std::sin(lat),
        radius * std::cos(lat) * std::sin(lon)
    );
}

} // namespace luna::sim
