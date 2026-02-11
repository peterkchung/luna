// About: ShaderModule implementation — loads SPIR-V binary and creates VkShaderModule.

#include "core/ShaderModule.h"
#include "util/FileIO.h"
#include "util/Log.h"

#include <stdexcept>

namespace luna::core {

ShaderModule::ShaderModule(VkDevice device, const std::string& spirvPath)
    : device_(device)
{
    auto code = luna::util::readBinaryFile(spirvPath);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    if (vkCreateShaderModule(device_, &createInfo, nullptr, &module_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module: " + spirvPath);

    LOG_INFO("Loaded shader: %s", spirvPath.c_str());
}

ShaderModule::~ShaderModule() {
    if (module_) vkDestroyShaderModule(device_, module_, nullptr);
}

ShaderModule::ShaderModule(ShaderModule&& other) noexcept
    : device_(other.device_), module_(other.module_)
{
    other.module_ = VK_NULL_HANDLE;
}

ShaderModule& ShaderModule::operator=(ShaderModule&& other) noexcept {
    if (this != &other) {
        if (module_) vkDestroyShaderModule(device_, module_, nullptr);
        device_ = other.device_;
        module_ = other.module_;
        other.module_ = VK_NULL_HANDLE;
    }
    return *this;
}

} // namespace luna::core
