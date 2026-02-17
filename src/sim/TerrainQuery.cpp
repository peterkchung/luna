// About: Procedural lunar heightmap — layered noise with crater depressions.

#include "sim/TerrainQuery.h"
#include <cmath>

namespace {

double hash(double x, double y) {
    double n = std::sin(x * 127.1 + y * 311.7) * 43758.5453;
    return n - std::floor(n);
}

double smoothNoise(double x, double y) {
    double ix = std::floor(x), iy = std::floor(y);
    double fx = x - ix, fy = y - iy;

    fx = fx * fx * (3.0 - 2.0 * fx);
    fy = fy * fy * (3.0 - 2.0 * fy);

    double a = hash(ix, iy);
    double b = hash(ix + 1.0, iy);
    double c = hash(ix, iy + 1.0);
    double d = hash(ix + 1.0, iy + 1.0);

    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

double layeredNoise(double x, double y) {
    double value = 0.0;
    double amplitude = 1.0;
    double frequency = 1.0;
    double total = 0.0;

    for (int i = 0; i < 6; i++) {
        value += smoothNoise(x * frequency, y * frequency) * amplitude;
        total += amplitude;
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value / total;
}

} // anonymous namespace

namespace luna::sim {

double sampleTerrainHeight(double lat, double lon) {
    double nx = lon * 500.0;
    double ny = lat * 500.0;

    // Gentle hills: ~200m variation
    double hills = layeredNoise(nx * 0.3, ny * 0.3) * 200.0 - 100.0;

    // Crater-like depressions
    double craterX = std::sin(lon * 80.0) * std::cos(lat * 60.0);
    double craterY = std::cos(lon * 50.0) * std::sin(lat * 90.0);
    double craterDist = craterX * craterX + craterY * craterY;
    double craters = -std::exp(-craterDist * 3.0) * 80.0;

    // Fine detail
    double detail = layeredNoise(nx * 2.0, ny * 2.0) * 20.0 - 10.0;

    return hills + craters + detail;
}

glm::dvec3 latLonToCartesian(double lat, double lon, double radius) {
    return glm::dvec3(
        radius * std::cos(lat) * std::cos(lon),
        radius * std::sin(lat),
        radius * std::cos(lat) * std::sin(lon)
    );
}

} // namespace luna::sim
