// About: Physics integrator — gravity, thrust, orientation, collision with terrain.

#pragma once

#include "sim/SimState.h"
#include <functional>

namespace luna::sim {

class Physics {
public:
    void step(SimState& state, double dt);

    // Terrain height query: (lat, lon) → elevation above LUNAR_RADIUS in meters
    void setTerrainQuery(std::function<double(double, double)> fn);

private:
    glm::dvec3 computeGravity(const glm::dvec3& position) const;
    glm::dvec3 computeThrust(const SimState& state) const;
    void consumeFuel(SimState& state, double dt) const;
    void deriveFlightData(SimState& state) const;
    void checkCollision(SimState& state) const;

    std::function<double(double, double)> terrainQuery_;

    static constexpr double G0 = 9.80665;             // standard gravity for Isp
    static constexpr double LANDING_SPEED = 4.0;       // max safe landing vertical speed (m/s)
    static constexpr double LANDING_HORIZ_SPEED = 2.0; // max safe landing horizontal speed (m/s)
};

} // namespace luna::sim
