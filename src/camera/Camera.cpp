// About: Quaternion camera — derives view directions from orientation, supports incremental rotation.

#include "camera/Camera.h"

#include <cmath>

namespace luna::camera {

Camera::Camera() {}

void Camera::setRotation(double pitch, double yaw) {
    // Build orientation quaternion from Euler angles (yaw around Y, then pitch around right)
    glm::dquat qYaw   = glm::angleAxis(yaw,   glm::dvec3(0.0, 1.0, 0.0));
    glm::dquat qPitch = glm::angleAxis(pitch,  glm::dvec3(1.0, 0.0, 0.0));
    orientation_ = glm::normalize(qYaw * qPitch);
}

void Camera::rotate(double pitchDelta, double yawDelta, const glm::dvec3& worldUp) {
    // Yaw around the provided world up axis (typically radial direction)
    if (std::abs(yawDelta) > 1e-10) {
        glm::dquat qYaw = glm::angleAxis(-yawDelta, worldUp);
        orientation_ = glm::normalize(qYaw * orientation_);
    }

    // Pitch around the camera's local right axis
    if (std::abs(pitchDelta) > 1e-10) {
        glm::dvec3 r = right();
        glm::dquat qPitch = glm::angleAxis(-pitchDelta, r);
        orientation_ = glm::normalize(qPitch * orientation_);
    }
}

glm::dvec3 Camera::forward() const {
    return glm::normalize(orientation_ * glm::dvec3(0.0, 0.0, -1.0));
}

glm::dvec3 Camera::right() const {
    return glm::normalize(orientation_ * glm::dvec3(1.0, 0.0, 0.0));
}

glm::dvec3 Camera::up() const {
    return glm::normalize(orientation_ * glm::dvec3(0.0, 1.0, 0.0));
}

glm::dvec3 Camera::localUp() const {
    double r = glm::length(position_);
    return (r > 1.0) ? position_ / r : glm::dvec3(0.0, 1.0, 0.0);
}

glm::dmat4 Camera::getViewMatrix() const {
    return glm::lookAt(position_, position_ + forward(), up());
}

glm::dmat4 Camera::getRotationOnlyViewMatrix() const {
    return glm::lookAt(glm::dvec3(0.0), forward(), up());
}

glm::dmat4 Camera::getProjectionMatrix() const {
    // Reversed-Z: near maps to 1.0, far maps to 0.0
    double tanHalfFov = std::tan(fovY_ / 2.0);

    glm::dmat4 proj(0.0);
    proj[0][0] = 1.0 / (aspect_ * tanHalfFov);
    proj[1][1] = 1.0 / tanHalfFov;
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
