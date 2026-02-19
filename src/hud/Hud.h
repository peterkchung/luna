// About: Screen-space HUD overlay with flight instruments, attitude display, and cockpit frame.

#pragma once

#include "scene/Mesh.h"
#include "util/Math.h"
#include <vulkan/vulkan.h>

namespace luna::core {
class VulkanContext;
class CommandPool;
}

namespace luna::sim {
struct SimState;
}

namespace luna::hud {

struct HudVertex {
    glm::vec2 position;     // screen UV (0-1), origin bottom-left
    glm::vec2 uv;           // panel-local UV (0-1)
    float     instrumentId; // fragment shader rendering mode selector
};

struct HudPushConstants {
    // Telemetry (existing)
    float altitude;
    float verticalSpeed;
    float surfaceSpeed;
    float throttle;
    float fuelFraction;
    float aspectRatio;

    // Attitude
    float pitch;            // radians, 0 = thrust straight up
    float roll;             // radians, positive = clockwise from pilot view

    // Navigation
    float heading;          // degrees, 0=N, 90=E, 180=S, 270=W
    float timeToSurface;    // seconds, negative = N/A

    // Prograde marker screen position (NDC: -1 to 1)
    float progradeX;
    float progradeY;

    // Status
    float flightPhase;      // 0=Orbit..5=Crashed
    float missionTime;      // seconds since start
    float warningFlags;     // bit 0=low fuel, bit 1=high rate, bit 2=tilt
    float progradeVisible;  // 1.0 if on screen, 0.0 if behind camera

    float tiltAngle;        // degrees from vertical
    float _pad0;
    float _pad1;
    float _pad2;
};

class Hud {
public:
    Hud(const luna::core::VulkanContext& ctx,
        const luna::core::CommandPool& cmdPool);

    void draw(VkCommandBuffer cmd, VkPipelineLayout layout,
              const luna::sim::SimState& simState, float aspectRatio,
              const glm::mat4& viewProj) const;

    void releaseGPU() { mesh_.release(); }

private:
    luna::scene::Mesh mesh_;
};

} // namespace luna::hud
