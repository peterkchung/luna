// About: Pipeline implementation — builder pattern assembles graphics pipeline state.

#include "core/Pipeline.h"
#include "core/VulkanContext.h"
#include "core/ShaderModule.h"

#include <array>
#include <stdexcept>

namespace luna::core {

Pipeline::~Pipeline() {
    if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
    if (layout_)   vkDestroyPipelineLayout(device_, layout_, nullptr);
}

Pipeline::Pipeline(Pipeline&& other) noexcept
    : device_(other.device_), pipeline_(other.pipeline_), layout_(other.layout_)
{
    other.pipeline_ = VK_NULL_HANDLE;
    other.layout_   = VK_NULL_HANDLE;
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept {
    if (this != &other) {
        if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
        if (layout_)   vkDestroyPipelineLayout(device_, layout_, nullptr);
        device_   = other.device_;
        pipeline_ = other.pipeline_;
        layout_   = other.layout_;
        other.pipeline_ = VK_NULL_HANDLE;
        other.layout_   = VK_NULL_HANDLE;
    }
    return *this;
}

// --- Builder ---

Pipeline::Builder::Builder(const VulkanContext& ctx, VkRenderPass renderPass)
    : ctx_(ctx), renderPass_(renderPass) {}

Pipeline::Builder& Pipeline::Builder::setShaders(const std::string& vertPath,
                                                  const std::string& fragPath) {
    vertPath_ = vertPath;
    fragPath_ = fragPath;
    return *this;
}

Pipeline::Builder& Pipeline::Builder::setVertexBinding(
    uint32_t stride, std::vector<VkVertexInputAttributeDescription> attrs) {
    vertexStride_ = stride;
    attributes_   = std::move(attrs);
    return *this;
}

Pipeline::Builder& Pipeline::Builder::setTopology(VkPrimitiveTopology topology) {
    topology_ = topology;
    return *this;
}

Pipeline::Builder& Pipeline::Builder::setCullMode(VkCullModeFlags mode) {
    cullMode_ = mode;
    return *this;
}

Pipeline::Builder& Pipeline::Builder::enableDepthTest(VkCompareOp compareOp) {
    depthTest_      = true;
    depthCompareOp_ = compareOp;
    return *this;
}

Pipeline::Builder& Pipeline::Builder::setDepthWrite(bool enabled) {
    depthWrite_ = enabled;
    return *this;
}

Pipeline::Builder& Pipeline::Builder::enableAlphaBlending() {
    alphaBlend_ = true;
    return *this;
}

Pipeline::Builder& Pipeline::Builder::setPushConstantSize(uint32_t size) {
    pushConstantSize_ = size;
    return *this;
}

Pipeline Pipeline::Builder::build() {
    ShaderModule vertShader(ctx_.device(), vertPath_);
    ShaderModule fragShader(ctx_.device(), fragPath_);

    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertShader.handle();
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragShader.handle();
    stages[1].pName  = "main";

    // Vertex input
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding   = 0;
    bindingDesc.stride    = vertexStride_;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (vertexStride_ > 0) {
        vertexInput.vertexBindingDescriptionCount   = 1;
        vertexInput.pVertexBindingDescriptions      = &bindingDesc;
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes_.size());
        vertexInput.pVertexAttributeDescriptions    = attributes_.data();
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = topology_;

    // Dynamic viewport and scissor
    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates    = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth   = 1.0f;
    rasterizer.cullMode    = cullMode_;
    rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable  = depthTest_ ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = (depthTest_ && depthWrite_) ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp   = depthCompareOp_;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (alphaBlend_) {
        colorBlendAttachment.blendEnable         = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments    = &colorBlendAttachment;

    // Pipeline layout with push constants
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset     = 0;
    pushRange.size       = pushConstantSize_;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (pushConstantSize_ > 0) {
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges    = &pushRange;
    }

    Pipeline pipeline;
    pipeline.device_ = ctx_.device();

    if (vkCreatePipelineLayout(ctx_.device(), &layoutInfo, nullptr, &pipeline.layout_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages             = stages.data();
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlend;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = pipeline.layout_;
    pipelineInfo.renderPass          = renderPass_;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(ctx_.device(), VK_NULL_HANDLE, 1, &pipelineInfo,
                                   nullptr, &pipeline.pipeline_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create graphics pipeline");

    return pipeline;
}

} // namespace luna::core
