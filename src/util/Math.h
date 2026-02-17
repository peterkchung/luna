// About: GLM configuration and double-precision typedefs for Moon-scale coordinates.

#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

namespace luna::util {

// Double-precision types for simulation-scale coordinates
using dvec3 = glm::dvec3;
using dvec4 = glm::dvec4;
using dmat4 = glm::dmat4;

constexpr double LUNAR_RADIUS = 1'737'400.0; // meters
constexpr double LUNAR_GM = 4.9028695e12;   // m^3/s^2

using dquat = glm::dquat;

} // namespace luna::util
