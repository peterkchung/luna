// About: Screen-space HUD overlay with seven-segment displays and bar gauges.

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
    float altitude;
    float verticalSpeed;
    float surfaceSpeed;
    float throttle;
    float fuelFraction;
    float aspectRatio;
    float _pad0;
    float _pad1;
};

class Hud {
public:
    Hud(const luna::core::VulkanContext& ctx,
        const luna::core::CommandPool& cmdPool);

    void draw(VkCommandBuffer cmd, VkPipelineLayout layout,
              const luna::sim::SimState& simState, float aspectRatio) const;

private:
    luna::scene::Mesh mesh_;
};

} // namespace luna::hud
