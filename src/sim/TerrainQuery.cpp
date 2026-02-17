// About: Lunar heightmap query — flat placeholder until NASA elevation data is integrated.

#include "sim/TerrainQuery.h"
#include <cmath>

namespace luna::sim {

double sampleTerrainHeight(double /*lat*/, double /*lon*/) {
    return 0.0;
}

glm::dvec3 latLonToCartesian(double lat, double lon, double radius) {
    return glm::dvec3(
        radius * std::cos(lat) * std::cos(lon),
        radius * std::sin(lat),
        radius * std::cos(lat) * std::sin(lon)
    );
}

} // namespace luna::sim
