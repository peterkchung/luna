// About: 6DOF rigid body state for the Starship HLS lunar lander.

#pragma once

#include "util/Math.h"

namespace luna::sim {

enum class FlightPhase { Orbit, Descent, PoweredDescent, Terminal, Landed, Crashed };

struct SimState {
    glm::dvec3 position{0.0};
    glm::dvec3 velocity{0.0};
    glm::dquat orientation{1.0, 0.0, 0.0, 0.0};
    glm::dvec3 angularVelocity{0.0};

    double dryMass         = 85000.0;    // kg (Starship HLS — landing gear, solar, elevator, no heat shield)
    double fuelMass        = 200000.0;   // kg (CH4/LOX, loaded for descent from low lunar orbit)
    double specificImpulse = 380.0;      // seconds (Raptor Vacuum)
    double maxThrust       = 4400000.0;  // Newtons (2x Raptor Vacuum)
    double throttle        = 0.0;        // 0.0–1.0
    glm::dvec3 torqueInput{0.0};         // body-frame torque command (rad/s^2)

    FlightPhase phase = FlightPhase::Orbit;
    double altitude      = 0.0;       // above terrain surface (meters)
    double surfaceSpeed  = 0.0;       // tangential speed (m/s)
    double verticalSpeed = 0.0;       // radial speed, positive = away from Moon (m/s)
    double missionTime   = 0.0;       // seconds since start

    double totalMass() const { return dryMass + fuelMass; }
};

} // namespace luna::sim
