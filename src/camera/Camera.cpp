// About: Camera implementation — double-precision view/projection with reversed-Z.

#include "camera/Camera.h"
#include <cmath>
#include <algorithm>

namespace luna::camera {

Camera::Camera() {}

glm::dvec3 Camera::forward() const {
    return glm::dvec3(
        std::cos(pitch_) * std::sin(yaw_),
        std::sin(pitch_),
        std::cos(pitch_) * std::cos(yaw_)
    );
}

glm::dvec3 Camera::right() const {
    return glm::normalize(glm::cross(forward(), glm::dvec3(0.0, 1.0, 0.0)));
}

glm::dvec3 Camera::up() const {
    return glm::normalize(glm::cross(right(), forward()));
}

glm::dmat4 Camera::getViewMatrix() const {
    return glm::lookAt(position_, position_ + forward(), glm::dvec3(0.0, 1.0, 0.0));
}

glm::dmat4 Camera::getRotationOnlyViewMatrix() const {
    return glm::lookAt(glm::dvec3(0.0), forward(), glm::dvec3(0.0, 1.0, 0.0));
}

glm::dmat4 Camera::getProjectionMatrix() const {
    // Reversed-Z: near maps to 1.0, far maps to 0.0
    // Build standard perspective then swap near/far planes
    double tanHalfFov = std::tan(fovY_ / 2.0);

    glm::dmat4 proj(0.0);
    proj[0][0] = 1.0 / (aspect_ * tanHalfFov);
    proj[1][1] = 1.0 / tanHalfFov;
    // Reversed-Z: map near → 1.0, far → 0.0
    proj[2][2] = near_ / (far_ - near_);
    proj[2][3] = -1.0;
    proj[3][2] = (far_ * near_) / (far_ - near_);

    return proj;
}

glm::mat4 Camera::getViewProjectionMatrix() const {
    glm::dmat4 vp = getProjectionMatrix() * getViewMatrix();
    return glm::mat4(vp);
}

} // namespace luna::camera
