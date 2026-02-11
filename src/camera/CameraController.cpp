// About: CameraController implementation — WASD/QE movement, mouse look, speed control.

#include "camera/CameraController.h"
#include "camera/Camera.h"
#include "input/InputManager.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace luna::camera {

CameraController::CameraController() {}

void CameraController::update(Camera& camera, const luna::input::InputManager& input, double dt) {
    // Mouse look (only when cursor is captured)
    if (input.isCursorCaptured()) {
        double newYaw   = camera.yaw()   - input.mouseDX() * sensitivity_;
        double newPitch = camera.pitch()  - input.mouseDY() * sensitivity_;

        constexpr double maxPitch = glm::radians(89.0);
        newPitch = std::clamp(newPitch, -maxPitch, maxPitch);

        camera.setRotation(newPitch, newYaw);
    }

    // Scroll wheel adjusts base speed
    if (input.scrollDY() != 0.0) {
        baseSpeed_ *= std::pow(1.15, input.scrollDY());
        baseSpeed_ = std::clamp(baseSpeed_, 1.0, 100'000.0);
    }

    // Movement
    double speed = baseSpeed_;
    if (input.isKeyDown(GLFW_KEY_LEFT_SHIFT)) speed *= 5.0;

    glm::dvec3 move(0.0);
    if (input.isKeyDown(GLFW_KEY_W)) move += camera.forward();
    if (input.isKeyDown(GLFW_KEY_S)) move -= camera.forward();
    if (input.isKeyDown(GLFW_KEY_D)) move += camera.right();
    if (input.isKeyDown(GLFW_KEY_A)) move -= camera.right();
    if (input.isKeyDown(GLFW_KEY_E)) move += glm::dvec3(0.0, 1.0, 0.0);
    if (input.isKeyDown(GLFW_KEY_Q)) move -= glm::dvec3(0.0, 1.0, 0.0);

    if (glm::length(move) > 0.0001)
        camera.setPosition(camera.position() + glm::normalize(move) * speed * dt);
}

} // namespace luna::camera
