// About: Camera controller with radial-up mouse look and altitude-scaled movement.

#pragma once

namespace luna::input { class InputManager; }

namespace luna::camera {

class Camera;

class CameraController {
public:
    CameraController();

    void update(Camera& camera, const luna::input::InputManager& input, double dt);

private:
    double baseSpeed_ = 100.0;   // m/s
    double sensitivity_ = 0.002; // radians/pixel
};

} // namespace luna::camera
