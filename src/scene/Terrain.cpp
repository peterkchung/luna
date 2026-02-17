// About: Terrain implementation — procedural heightmap with layered noise, Moon-centered coords.

#include "scene/Terrain.h"
#include "util/Log.h"

#include <cmath>
#include <vector>

namespace luna::scene {

// Simple hash-based noise for procedural terrain
static double hash(double x, double y) {
    double n = std::sin(x * 127.1 + y * 311.7) * 43758.5453;
    return n - std::floor(n);
}

static double smoothNoise(double x, double y) {
    double ix = std::floor(x), iy = std::floor(y);
    double fx = x - ix, fy = y - iy;

    // Smoothstep interpolation
    fx = fx * fx * (3.0 - 2.0 * fx);
    fy = fy * fy * (3.0 - 2.0 * fy);

    double a = hash(ix, iy);
    double b = hash(ix + 1.0, iy);
    double c = hash(ix, iy + 1.0);
    double d = hash(ix + 1.0, iy + 1.0);

    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

static double layeredNoise(double x, double y) {
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

double Terrain::sampleHeight(double lat, double lon) {
    // Scale lat/lon to noise space (higher freq = more terrain detail)
    double nx = lon * 500.0;
    double ny = lat * 500.0;

    // Gentle hills: ~200m variation
    double hills = layeredNoise(nx * 0.3, ny * 0.3) * 200.0 - 100.0;

    // Crater-like depressions using distance from periodic grid points
    double craterX = std::sin(lon * 80.0) * std::cos(lat * 60.0);
    double craterY = std::cos(lon * 50.0) * std::sin(lat * 90.0);
    double craterDist = craterX * craterX + craterY * craterY;
    double craters = -std::exp(-craterDist * 3.0) * 80.0;

    // Fine detail
    double detail = layeredNoise(nx * 2.0, ny * 2.0) * 20.0 - 10.0;

    return hills + craters + detail;
}

glm::dvec3 Terrain::latLonToCartesian(double lat, double lon, double radius) {
    // IAU Moon frame: +X toward 0°N 0°E, +Y toward north pole, +Z toward 0°N 90°E
    return glm::dvec3(
        radius * std::cos(lat) * std::cos(lon),
        radius * std::sin(lat),
        radius * std::cos(lat) * std::sin(lon)
    );
}

Terrain::Terrain(const luna::core::VulkanContext& ctx, const luna::core::CommandPool& cmdPool,
                 double centerLat, double centerLon, double gridSize, uint32_t resolution)
{
    // Compute chunk center in world space (double precision)
    center_ = latLonToCartesian(centerLat, centerLon, luna::util::LUNAR_RADIUS);

    // Convert grid size from meters to radians (approximate at lunar surface)
    double metersPerRadian = luna::util::LUNAR_RADIUS;
    double halfAngle = (gridSize / 2.0) / metersPerRadian;

    double latStep = (2.0 * halfAngle) / static_cast<double>(resolution - 1);
    double lonStep = (2.0 * halfAngle) / static_cast<double>(resolution - 1);

    double startLat = centerLat - halfAngle;
    double startLon = centerLon - halfAngle;

    // Generate vertices with chunk-local positions, normals, and height
    std::vector<TerrainVertex> vertices(resolution * resolution);

    for (uint32_t y = 0; y < resolution; y++) {
        for (uint32_t x = 0; x < resolution; x++) {
            double lat = startLat + y * latStep;
            double lon = startLon + x * lonStep;
            double h = sampleHeight(lat, lon);
            double r = luna::util::LUNAR_RADIUS + h;

            glm::dvec3 worldPos = latLonToCartesian(lat, lon, r);
            vertices[y * resolution + x].position = glm::vec3(worldPos - center_);
            vertices[y * resolution + x].height = static_cast<float>(h);
        }
    }

    // Compute normals via central differencing
    double dAngle = latStep;
    for (uint32_t y = 0; y < resolution; y++) {
        for (uint32_t x = 0; x < resolution; x++) {
            double lat = startLat + y * latStep;
            double lon = startLon + x * lonStep;

            double hL = sampleHeight(lat, lon - dAngle);
            double hR = sampleHeight(lat, lon + dAngle);
            double hD = sampleHeight(lat - dAngle, lon);
            double hU = sampleHeight(lat + dAngle, lon);

            glm::dvec3 pL = latLonToCartesian(lat, lon - dAngle, luna::util::LUNAR_RADIUS + hL);
            glm::dvec3 pR = latLonToCartesian(lat, lon + dAngle, luna::util::LUNAR_RADIUS + hR);
            glm::dvec3 pD = latLonToCartesian(lat - dAngle, lon, luna::util::LUNAR_RADIUS + hD);
            glm::dvec3 pU = latLonToCartesian(lat + dAngle, lon, luna::util::LUNAR_RADIUS + hU);

            glm::dvec3 tangentX = pR - pL;
            glm::dvec3 tangentY = pU - pD;
            glm::dvec3 normal = glm::normalize(glm::cross(tangentX, tangentY));

            vertices[y * resolution + x].normal = glm::vec3(normal);
        }
    }

    // Generate indices (triangle list from grid)
    std::vector<uint32_t> indices;
    indices.reserve((resolution - 1) * (resolution - 1) * 6);
    for (uint32_t y = 0; y < resolution - 1; y++) {
        for (uint32_t x = 0; x < resolution - 1; x++) {
            uint32_t tl = y * resolution + x;
            uint32_t tr = tl + 1;
            uint32_t bl = (y + 1) * resolution + x;
            uint32_t br = bl + 1;

            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);

            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    mesh_ = Mesh(ctx, cmdPool,
                 vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(TerrainVertex)),
                 indices.data(), static_cast<uint32_t>(indices.size()));

    LOG_INFO("Terrain generated: %ux%u vertices, %zu triangles",
             resolution, resolution, indices.size() / 3);
}

void Terrain::draw(VkCommandBuffer cmd) const {
    mesh_.draw(cmd);
}

} // namespace luna::scene
