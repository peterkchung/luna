// About: Builds static HUD mesh and draws with per-frame telemetry push constants.

#include "hud/Hud.h"
#include "sim/SimState.h"

#include <vector>

namespace luna::hud {

namespace {

void addQuad(std::vector<HudVertex>& verts, std::vector<uint32_t>& indices,
             float x, float y, float w, float h, float instrumentId) {
    uint32_t base = static_cast<uint32_t>(verts.size());
    verts.push_back({{x,     y},     {0.0f, 0.0f}, instrumentId});
    verts.push_back({{x + w, y},     {1.0f, 0.0f}, instrumentId});
    verts.push_back({{x + w, y + h}, {1.0f, 1.0f}, instrumentId});
    verts.push_back({{x,     y + h}, {0.0f, 1.0f}, instrumentId});
    indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

} // anonymous namespace

Hud::Hud(const luna::core::VulkanContext& ctx,
         const luna::core::CommandPool& cmdPool) {
    std::vector<HudVertex> vertices;
    std::vector<uint32_t> indices;

    // Altitude — bottom-left, 7 digits
    addQuad(vertices, indices, 0.02f, 0.06f, 0.22f, 0.05f, 0.0f);

    // Vertical speed — below altitude, sign + 5 digits
    addQuad(vertices, indices, 0.02f, 0.01f, 0.18f, 0.04f, 1.0f);

    // Surface speed — bottom-right, 5 digits
    addQuad(vertices, indices, 0.80f, 0.01f, 0.18f, 0.04f, 2.0f);

    // Throttle bar — bottom-center-left, vertical
    addQuad(vertices, indices, 0.44f, 0.01f, 0.04f, 0.12f, 3.0f);

    // Fuel bar — bottom-center-right, vertical
    addQuad(vertices, indices, 0.52f, 0.01f, 0.04f, 0.12f, 4.0f);

    mesh_ = luna::scene::Mesh(ctx, cmdPool,
                               vertices.data(),
                               static_cast<uint32_t>(vertices.size() * sizeof(HudVertex)),
                               indices.data(),
                               static_cast<uint32_t>(indices.size()));
}

void Hud::draw(VkCommandBuffer cmd, VkPipelineLayout layout,
               const luna::sim::SimState& simState, float aspectRatio) const {
    HudPushConstants pc{};
    pc.altitude      = static_cast<float>(simState.altitude);
    pc.verticalSpeed = static_cast<float>(simState.verticalSpeed);
    pc.surfaceSpeed  = static_cast<float>(simState.surfaceSpeed);
    pc.throttle      = static_cast<float>(simState.throttle);
    pc.fuelFraction  = static_cast<float>(simState.fuelMass / (simState.fuelMass + simState.dryMass));
    pc.aspectRatio   = aspectRatio;

    vkCmdPushConstants(cmd, layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(HudPushConstants), &pc);
    mesh_.draw(cmd);
}

} // namespace luna::hud
