// About: Perspective camera with quaternion orientation and reversed-Z projection.

#pragma once

#include "util/Math.h"

namespace luna::camera {

class Camera {
public:
    Camera();

    void setPosition(const glm::dvec3& pos) { position_ = pos; }
    void setOrientation(const glm::dquat& q) { orientation_ = glm::normalize(q); }

    // Initialize orientation from Euler angles (for startup convenience)
    void setRotation(double pitch, double yaw);

    // Apply incremental rotation around given axes
    void rotate(double pitchDelta, double yawDelta, const glm::dvec3& worldUp);

    void setAspect(double aspect) { aspect_ = aspect; }

    const glm::dvec3& position() const { return position_; }
    const glm::dquat& orientation() const { return orientation_; }

    glm::dvec3 forward() const;
    glm::dvec3 right()   const;
    glm::dvec3 up()      const;

    // Local up direction (radial from Moon center for spherical bodies)
    glm::dvec3 localUp() const;

    glm::dmat4 getViewMatrix() const;
    glm::dmat4 getProjectionMatrix() const;

    // Rotation-only view matrix (no translation — for camera-relative rendering)
    glm::dmat4 getRotationOnlyViewMatrix() const;

    // Compute VP in doubles, downcast to float for GPU push constants
    glm::mat4 getViewProjectionMatrix() const;

    double fovY() const { return fovY_; }

private:
    glm::dvec3 position_ = glm::dvec3(0.0);
    glm::dquat orientation_{1.0, 0.0, 0.0, 0.0};

    double fovY_   = glm::radians(70.0);
    double aspect_ = 16.0 / 9.0;
    double near_   = 0.5;
    double far_    = 2'000'000.0;
};

} // namespace luna::camera
