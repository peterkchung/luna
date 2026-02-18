// About: Pure-math heightmap sampling for lunar terrain — shared by mesh generation and physics.

#pragma once

#include "util/Math.h"
#include <string>

namespace luna::sim {

// Load LOLA heightmap from TIFF. Returns false if file missing (graceful fallback to flat).
bool initTerrain(const std::string& path);

// Free heightmap memory.
void shutdownTerrain();

// Returns elevation above LUNAR_RADIUS in meters at the given lat/lon (radians)
double sampleTerrainHeight(double lat, double lon);

// Convert lat/lon/elevation to Moon-centered XYZ (IAU_MOON frame)
glm::dvec3 latLonToCartesian(double lat, double lon, double radius);

} // namespace luna::sim
