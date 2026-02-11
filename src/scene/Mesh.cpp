// About: Mesh implementation — creates vertex/index buffers and records draw commands.

#include "scene/Mesh.h"
#include "core/VulkanContext.h"
#include "core/CommandPool.h"

namespace luna::scene {

Mesh::Mesh(const luna::core::VulkanContext& ctx, const luna::core::CommandPool& cmdPool,
           const void* vertexData, uint32_t vertexSize,
           const void* indexData,  uint32_t indexCount)
    : indexCount_(indexCount)
{
    vertexBuffer_ = luna::core::Buffer::createStatic(ctx, cmdPool,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexData, vertexSize);

    indexBuffer_ = luna::core::Buffer::createStatic(ctx, cmdPool,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexData,
        static_cast<VkDeviceSize>(indexCount) * sizeof(uint32_t));
}

void Mesh::draw(VkCommandBuffer cmd) const {
    VkBuffer buffers[] = { vertexBuffer_.handle() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.handle(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
}

} // namespace luna::scene
