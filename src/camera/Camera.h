// About: Perspective camera with double-precision view matrix and reversed-Z projection.

#pragma once

#include "util/Math.h"

namespace luna::camera {

class Camera {
public:
    Camera();

    void setPosition(const glm::dvec3& pos) { position_ = pos; }
    void setRotation(double pitch, double yaw) { pitch_ = pitch; yaw_ = yaw; }
    void setAspect(double aspect) { aspect_ = aspect; }

    const glm::dvec3& position() const { return position_; }
    double pitch() const { return pitch_; }
    double yaw()   const { return yaw_; }

    glm::dvec3 forward() const;
    glm::dvec3 right()   const;
    glm::dvec3 up()      const;

    glm::dmat4 getViewMatrix() const;
    glm::dmat4 getProjectionMatrix() const;

    // Rotation-only view matrix (no translation — for camera-relative rendering)
    glm::dmat4 getRotationOnlyViewMatrix() const;

    // Compute VP in doubles, downcast to float for GPU push constants
    glm::mat4 getViewProjectionMatrix() const;

    double fovY() const { return fovY_; }

private:
    glm::dvec3 position_ = glm::dvec3(0.0);
    double pitch_ = 0.0; // radians, clamped to ±89°
    double yaw_   = 0.0; // radians

    double fovY_   = glm::radians(70.0);
    double aspect_ = 16.0 / 9.0;
    double near_   = 0.1;
    double far_    = 200'000.0;
};

} // namespace luna::camera
