// About: Vertex/index buffer pair with draw command — owns GPU buffer lifetime.

#pragma once

#include "core/Buffer.h"
#include <vulkan/vulkan.h>
#include <cstdint>

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

    void draw(VkCommandBuffer cmd) const;

    uint32_t indexCount() const { return indexCount_; }

private:
    luna::core::Buffer vertexBuffer_;
    luna::core::Buffer indexBuffer_;
    uint32_t indexCount_ = 0;
};

} // namespace luna::scene
