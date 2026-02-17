// About: 6DOF rigid body state for the lunar lander — position, velocity, orientation, fuel.

#pragma once

#include "util/Math.h"

namespace luna::sim {

enum class FlightPhase { Orbit, Descent, PoweredDescent, Terminal, Landed, Crashed };

struct SimState {
    glm::dvec3 position{0.0};
    glm::dvec3 velocity{0.0};
    glm::dquat orientation{1.0, 0.0, 0.0, 0.0};
    glm::dvec3 angularVelocity{0.0};

    double dryMass       = 2150.0;    // kg (Apollo LM descent stage)
    double fuelMass      = 8200.0;    // kg
    double specificImpulse = 311.0;   // seconds (AJ10-137)
    double maxThrust     = 45040.0;   // Newtons
    double throttle      = 0.0;       // 0.0–1.0
    glm::dvec3 torqueInput{0.0};      // body-frame torque command (rad/s^2)

    FlightPhase phase = FlightPhase::Orbit;
    double altitude      = 0.0;       // above terrain surface (meters)
    double surfaceSpeed  = 0.0;       // tangential speed (m/s)
    double verticalSpeed = 0.0;       // radial speed, positive = away from Moon (m/s)
    double missionTime   = 0.0;       // seconds since start

    double totalMass() const { return dryMass + fuelMass; }
};

} // namespace luna::sim
