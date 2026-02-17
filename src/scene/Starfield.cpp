// About: Generates random star directions on the unit sphere and uploads as point mesh.

#include "scene/Starfield.h"

#include <cmath>
#include <random>
#include <vector>

namespace luna::scene {

// Push constant layout for starfield (just viewProj)
struct StarfieldPC {
    glm::mat4 viewProj;
};

Starfield::Starfield(const luna::core::VulkanContext& ctx,
                     const luna::core::CommandPool& cmdPool,
                     uint32_t starCount) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> uniform01(0.0f, 1.0f);

    std::vector<StarVertex> vertices(starCount);
    std::vector<uint32_t> indices(starCount);

    for (uint32_t i = 0; i < starCount; i++) {
        // Uniform random point on unit sphere (spherical coordinates)
        float theta = 2.0f * glm::pi<float>() * uniform01(rng);
        float phi = std::acos(1.0f - 2.0f * uniform01(rng));

        vertices[i].direction = glm::vec3(
            std::sin(phi) * std::cos(theta),
            std::sin(phi) * std::sin(theta),
            std::cos(phi)
        );

        // Random brightness with bias toward dim stars (more realistic distribution)
        float r = uniform01(rng);
        vertices[i].brightness = r * r * r;  // cubic falloff: mostly dim, few bright

        indices[i] = i;
    }

    mesh_ = Mesh(ctx, cmdPool,
                 vertices.data(),
                 static_cast<uint32_t>(vertices.size() * sizeof(StarVertex)),
                 indices.data(),
                 static_cast<uint32_t>(indices.size()));
}

void Starfield::draw(VkCommandBuffer cmd, VkPipelineLayout layout,
                     const glm::mat4& viewProj) const {
    StarfieldPC pc{};
    pc.viewProj = viewProj;

    vkCmdPushConstants(cmd, layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(StarfieldPC), &pc);
    mesh_.draw(cmd);
}

} // namespace luna::scene
