// About: Vulkan graphics pipeline with builder pattern for configurable setup.

#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace luna::core {

class VulkanContext;

class Pipeline {
public:
    ~Pipeline();

    Pipeline(Pipeline&& other) noexcept;
    Pipeline& operator=(Pipeline&& other) noexcept;
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    VkPipeline       handle() const { return pipeline_; }
    VkPipelineLayout layout() const { return layout_; }

    class Builder {
    public:
        Builder(const VulkanContext& ctx, VkRenderPass renderPass);

        Builder& setShaders(const std::string& vertPath, const std::string& fragPath);
        Builder& setVertexBinding(uint32_t stride,
                                  std::vector<VkVertexInputAttributeDescription> attrs);
        Builder& setTopology(VkPrimitiveTopology topology);
        Builder& enableDepthTest(VkCompareOp compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL);
        Builder& setPushConstantSize(uint32_t size);

        Pipeline build();

    private:
        const VulkanContext& ctx_;
        VkRenderPass         renderPass_;
        std::string          vertPath_;
        std::string          fragPath_;
        VkPrimitiveTopology  topology_ = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        bool                 depthTest_ = false;
        VkCompareOp          depthCompareOp_ = VK_COMPARE_OP_GREATER_OR_EQUAL;
        uint32_t             pushConstantSize_ = 0;
        uint32_t             vertexStride_ = 0;
        std::vector<VkVertexInputAttributeDescription> attributes_;
    };

private:
    Pipeline() = default;

    VkDevice         device_   = VK_NULL_HANDLE;
    VkPipeline       pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_   = VK_NULL_HANDLE;
};

} // namespace luna::core
