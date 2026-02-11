// About: Free-look camera controller with WASD movement and mouse look.

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
