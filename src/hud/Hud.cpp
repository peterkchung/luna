// About: Builds static HUD mesh and draws with per-frame telemetry push constants.

#include "hud/Hud.h"
#include "sim/SimState.h"

#include <cmath>
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

    // === Bottom instruments (existing) ===

    // Altitude — bottom-left, 7 digits + label
    addQuad(vertices, indices, 0.02f, 0.06f, 0.22f, 0.07f, 0.0f);

    // Vertical speed — below altitude, sign + 5 digits + label
    addQuad(vertices, indices, 0.02f, 0.00f, 0.18f, 0.06f, 1.0f);

    // Surface speed — bottom-right, 5 digits + label
    addQuad(vertices, indices, 0.80f, 0.00f, 0.18f, 0.06f, 2.0f);

    // Throttle bar — bottom-center-left, vertical + label
    addQuad(vertices, indices, 0.44f, 0.00f, 0.04f, 0.15f, 3.0f);

    // Fuel bar — bottom-center-right, vertical + label
    addQuad(vertices, indices, 0.52f, 0.00f, 0.04f, 0.15f, 4.0f);

    // === Phase 2 instruments ===

    // Full-screen overlay — MUST be drawn first (behind other panels)
    addQuad(vertices, indices, 0.0f, 0.0f, 1.0f, 1.0f, 10.0f);

    // Attitude indicator — left side, square
    addQuad(vertices, indices, 0.02f, 0.22f, 0.14f, 0.14f, 5.0f);

    // Heading compass — top-center horizontal strip
    addQuad(vertices, indices, 0.30f, 0.95f, 0.40f, 0.03f, 6.0f);

    // Flight phase — top-left
    addQuad(vertices, indices, 0.02f, 0.93f, 0.12f, 0.04f, 7.0f);

    // Mission elapsed time — top-right
    addQuad(vertices, indices, 0.84f, 0.93f, 0.14f, 0.04f, 8.0f);

    // Time to surface — below altitude stack
    addQuad(vertices, indices, 0.02f, 0.13f, 0.14f, 0.06f, 9.0f);

    mesh_ = luna::scene::Mesh(ctx, cmdPool,
                               vertices.data(),
                               static_cast<uint32_t>(vertices.size() * sizeof(HudVertex)),
                               indices.data(),
                               static_cast<uint32_t>(indices.size()));
}

void Hud::draw(VkCommandBuffer cmd, VkPipelineLayout layout,
               const luna::sim::SimState& simState, float aspectRatio,
               const glm::mat4& viewProj) const {
    HudPushConstants pc{};

    // Existing telemetry
    pc.altitude      = static_cast<float>(simState.altitude);
    pc.verticalSpeed = static_cast<float>(simState.verticalSpeed);
    pc.surfaceSpeed  = static_cast<float>(simState.surfaceSpeed);
    pc.throttle      = static_cast<float>(simState.throttle);
    pc.fuelFraction  = static_cast<float>(simState.fuelMass / (simState.fuelMass + simState.dryMass));
    pc.aspectRatio   = aspectRatio;

    // Attitude: compute pitch, roll, heading from vehicle orientation
    double r = glm::length(simState.position);
    glm::dvec3 localUp = (r > 1.0) ? simState.position / r : glm::dvec3(0.0, 1.0, 0.0);

    glm::dvec3 bodyUp    = simState.orientation * glm::dvec3(0.0, 1.0, 0.0);
    glm::dvec3 bodyRight = simState.orientation * glm::dvec3(1.0, 0.0, 0.0);
    glm::dvec3 bodyFwd   = simState.orientation * glm::dvec3(0.0, 0.0, 1.0);

    // Pitch: angle between thrust axis (bodyUp) and local vertical
    double cosP = glm::clamp(glm::dot(bodyUp, localUp), -1.0, 1.0);
    pc.pitch = static_cast<float>(std::acos(cosP));

    // Tilt angle in degrees (same as pitch for warning display)
    pc.tiltAngle = glm::degrees(pc.pitch);

    // Roll: rotation of body around the thrust axis relative to local vertical
    // Project localUp onto the plane perpendicular to bodyUp
    glm::dvec3 localUpInBody = localUp - glm::dot(localUp, bodyUp) * bodyUp;
    double localUpLen = glm::length(localUpInBody);
    if (localUpLen > 1e-6) {
        localUpInBody /= localUpLen;
        double rollCos = glm::dot(localUpInBody, -bodyRight);
        double rollSin = glm::dot(localUpInBody, bodyFwd);
        pc.roll = static_cast<float>(std::atan2(rollSin, rollCos));
    } else {
        pc.roll = 0.0f;
    }

    // Heading: project body forward onto local horizontal plane, angle from north
    glm::dvec3 north = glm::dvec3(0.0, 1.0, 0.0) - glm::dot(glm::dvec3(0.0, 1.0, 0.0), localUp) * localUp;
    double northLen = glm::length(north);
    if (northLen > 1e-6) {
        north /= northLen;
        glm::dvec3 east = glm::cross(localUp, north);
        glm::dvec3 fwdHoriz = bodyFwd - glm::dot(bodyFwd, localUp) * localUp;
        double fwdHorizLen = glm::length(fwdHoriz);
        if (fwdHorizLen > 1e-6) {
            fwdHoriz /= fwdHorizLen;
            double hdg = glm::degrees(std::atan2(glm::dot(fwdHoriz, east), glm::dot(fwdHoriz, north)));
            if (hdg < 0.0) hdg += 360.0;
            pc.heading = static_cast<float>(hdg);
        }
    }

    // Time to surface
    if (simState.verticalSpeed < -0.5 && simState.altitude > 0.0) {
        pc.timeToSurface = static_cast<float>(simState.altitude / -simState.verticalSpeed);
    } else {
        pc.timeToSurface = -1.0f;
    }

    // Prograde marker: project velocity direction through rotation-only VP
    glm::dvec3 vel = simState.velocity;
    double velMag = glm::length(vel);
    if (velMag > 0.1) {
        glm::vec3 velDir = glm::vec3(glm::normalize(vel));
        glm::vec4 clip = viewProj * glm::vec4(velDir * 1000.0f, 1.0f);
        if (clip.w > 0.0f) {
            pc.progradeX = clip.x / clip.w;
            pc.progradeY = clip.y / clip.w;
            pc.progradeVisible = 1.0f;
        }
    }

    // Flight phase
    pc.flightPhase = static_cast<float>(static_cast<int>(simState.phase));
    pc.missionTime = static_cast<float>(simState.missionTime);

    // Warning flags: bit 0 = low fuel, bit 1 = high descent rate, bit 2 = tilt
    int warnings = 0;
    if (pc.fuelFraction < 0.10f) warnings |= 1;
    if (simState.verticalSpeed < -50.0 && simState.altitude < 5000.0 && simState.altitude > 0.0)
        warnings |= 2;
    if (pc.tiltAngle > 30.0f && simState.phase != luna::sim::FlightPhase::Landed
        && simState.phase != luna::sim::FlightPhase::Crashed)
        warnings |= 4;
    pc.warningFlags = static_cast<float>(warnings);

    vkCmdPushConstants(cmd, layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(HudPushConstants), &pc);
    mesh_.draw(cmd);
}

} // namespace luna::hud
