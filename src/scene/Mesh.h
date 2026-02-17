// About: Vertex/index buffer pair with draw command — owns GPU buffer lifetime.

#pragma once

#include "core/Buffer.h"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace luna::core {
class VulkanContext;
class CommandPool;
}

namespace luna::scene {

class Mesh {
public:
    Mesh() = default;
    Mesh(const luna::core::VulkanContext& ctx, const luna::core::CommandPool& cmdPool,
         const void* vertexData, uint32_t vertexSize,
         const void* indexData,  uint32_t indexCount);

    // Batched variant: records copies into transferCmd, appends staging buffers to stagingOut
    Mesh(const luna::core::VulkanContext& ctx, VkCommandBuffer transferCmd,
         const void* vertexData, uint32_t vertexSize,
         const void* indexData,  uint32_t indexCount,
         std::vector<luna::core::Buffer>& stagingOut);

    void draw(VkCommandBuffer cmd) const;

    uint32_t indexCount() const { return indexCount_; }

private:
    luna::core::Buffer vertexBuffer_;
    luna::core::Buffer indexBuffer_;
    uint32_t indexCount_ = 0;
};

} // namespace luna::scene
