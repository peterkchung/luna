// About: Camera controller — radial up for spherical bodies, speed scales with altitude.

#include "camera/CameraController.h"
#include "camera/Camera.h"
#include "input/InputManager.h"
#include "util/Math.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace luna::camera {

CameraController::CameraController() {}

void CameraController::update(Camera& camera, const luna::input::InputManager& input, double dt) {
    // Mouse look (only when cursor is captured)
    if (input.isCursorCaptured()) {
        double yawDelta   = input.mouseDX() * sensitivity_;
        double pitchDelta = input.mouseDY() * sensitivity_;

        // Use radial up (from Moon center through camera position)
        glm::dvec3 radialUp = camera.localUp();
        camera.rotate(pitchDelta, yawDelta, radialUp);
    }

    // Scroll wheel adjusts base speed
    if (input.scrollDY() != 0.0) {
        baseSpeed_ *= std::pow(1.15, input.scrollDY());
        baseSpeed_ = std::clamp(baseSpeed_, 1.0, 1'000'000.0);
    }

    // Speed scales with altitude above surface
    double altitude = glm::length(camera.position()) - luna::util::LUNAR_RADIUS;
    double speedScale = std::max(1.0, altitude / 1000.0);
    double speed = baseSpeed_ * speedScale;
    if (input.isKeyDown(GLFW_KEY_LEFT_SHIFT)) speed *= 5.0;

    // Movement relative to camera orientation
    glm::dvec3 move(0.0);
    if (input.isKeyDown(GLFW_KEY_W)) move += camera.forward();
    if (input.isKeyDown(GLFW_KEY_S)) move -= camera.forward();
    if (input.isKeyDown(GLFW_KEY_D)) move += camera.right();
    if (input.isKeyDown(GLFW_KEY_A)) move -= camera.right();
    if (input.isKeyDown(GLFW_KEY_E)) move += camera.localUp();
    if (input.isKeyDown(GLFW_KEY_Q)) move -= camera.localUp();

    if (glm::length(move) > 0.0001)
        camera.setPosition(camera.position() + glm::normalize(move) * speed * dt);
}

} // namespace luna::camera
