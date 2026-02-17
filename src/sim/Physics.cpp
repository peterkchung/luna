// About: Semi-implicit Euler integration for 6DOF lunar lander physics.

#include "sim/Physics.h"

#include <cmath>

namespace luna::sim {

void Physics::setTerrainQuery(std::function<double(double, double)> fn) {
    terrainQuery_ = std::move(fn);
}

glm::dvec3 Physics::computeGravity(const glm::dvec3& position) const {
    double r = glm::length(position);
    if (r < 1.0) return glm::dvec3(0.0);
    glm::dvec3 dir = position / r;
    return -dir * (luna::util::LUNAR_GM / (r * r));
}

glm::dvec3 Physics::computeThrust(const SimState& state) const {
    if (state.throttle <= 0.0 || state.fuelMass <= 0.0)
        return glm::dvec3(0.0);

    // Thrust along lander's local +Y axis (body frame → world)
    glm::dvec3 thrustDir = state.orientation * glm::dvec3(0.0, 1.0, 0.0);
    double force = state.throttle * state.maxThrust;
    return thrustDir * (force / state.totalMass());
}

void Physics::consumeFuel(SimState& state, double dt) const {
    if (state.throttle <= 0.0 || state.fuelMass <= 0.0) return;

    double massFlow = state.throttle * state.maxThrust / (state.specificImpulse * G0);
    state.fuelMass -= massFlow * dt;
    if (state.fuelMass < 0.0) state.fuelMass = 0.0;
}

void Physics::deriveFlightData(SimState& state) const {
    double r = glm::length(state.position);
    glm::dvec3 radialDir = (r > 1.0) ? state.position / r : glm::dvec3(1.0, 0.0, 0.0);

    // Vertical speed (positive = moving away from Moon center)
    state.verticalSpeed = glm::dot(state.velocity, radialDir);

    // Surface speed (tangential component)
    glm::dvec3 tangential = state.velocity - radialDir * state.verticalSpeed;
    state.surfaceSpeed = glm::length(tangential);

    // Altitude above terrain
    double lat = std::asin(glm::clamp(radialDir.y, -1.0, 1.0));
    double lon = std::atan2(radialDir.z, radialDir.x);
    double terrainElev = terrainQuery_ ? terrainQuery_(lat, lon) : 0.0;
    state.altitude = r - luna::util::LUNAR_RADIUS - terrainElev;
}

void Physics::checkCollision(SimState& state) const {
    if (state.altitude > 0.0) return;
    if (state.phase == FlightPhase::Landed || state.phase == FlightPhase::Crashed) return;

    // Clamp position to surface
    double r = glm::length(state.position);
    glm::dvec3 radialDir = state.position / r;
    double lat = std::asin(glm::clamp(radialDir.y, -1.0, 1.0));
    double lon = std::atan2(radialDir.z, radialDir.x);
    double terrainElev = terrainQuery_ ? terrainQuery_(lat, lon) : 0.0;
    double surfaceR = luna::util::LUNAR_RADIUS + terrainElev;

    state.position = radialDir * surfaceR;
    state.altitude = 0.0;

    // Check landing criteria
    bool safeLanding = (std::abs(state.verticalSpeed) < LANDING_SPEED) &&
                       (state.surfaceSpeed < LANDING_HORIZ_SPEED);

    if (safeLanding) {
        state.phase = FlightPhase::Landed;
    } else {
        state.phase = FlightPhase::Crashed;
    }

    state.velocity = glm::dvec3(0.0);
    state.angularVelocity = glm::dvec3(0.0);
}

void Physics::step(SimState& state, double dt) {
    if (state.phase == FlightPhase::Landed || state.phase == FlightPhase::Crashed)
        return;

    // Clamp dt to prevent instability from frame spikes
    dt = glm::min(dt, 0.05);

    state.missionTime += dt;

    // Semi-implicit Euler: update velocity first, then position
    glm::dvec3 gravity = computeGravity(state.position);
    glm::dvec3 thrustAccel = computeThrust(state);

    state.velocity += (gravity + thrustAccel) * dt;
    state.position += state.velocity * dt;

    // Orientation integration: q' = q + 0.5 * dt * omega * q
    if (glm::length(state.angularVelocity + state.torqueInput) > 1e-10) {
        glm::dvec3 angVel = state.angularVelocity + state.torqueInput * dt;
        state.angularVelocity = angVel;
        glm::dquat spin(0.0, angVel.x, angVel.y, angVel.z);
        state.orientation += 0.5 * dt * spin * state.orientation;
        state.orientation = glm::normalize(state.orientation);
    }

    consumeFuel(state, dt);
    deriveFlightData(state);
    checkCollision(state);

    // Update flight phase based on altitude and throttle
    if (state.phase != FlightPhase::Landed && state.phase != FlightPhase::Crashed) {
        if (state.throttle > 0.0) {
            state.phase = (state.altitude < 1000.0) ? FlightPhase::Terminal
                                                     : FlightPhase::PoweredDescent;
        } else if (state.verticalSpeed < -1.0) {
            state.phase = FlightPhase::Descent;
        } else {
            state.phase = FlightPhase::Orbit;
        }
    }
}

} // namespace luna::sim
