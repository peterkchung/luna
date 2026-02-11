// About: SPIR-V shader module loading and RAII lifetime management.

#pragma once

#include <vulkan/vulkan.h>
#include <string>

namespace luna::core {

class ShaderModule {
public:
    ShaderModule(VkDevice device, const std::string& spirvPath);
    ~ShaderModule();

    ShaderModule(ShaderModule&& other) noexcept;
    ShaderModule& operator=(ShaderModule&& other) noexcept;
    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;

    VkShaderModule handle() const { return module_; }

private:
    VkDevice       device_ = VK_NULL_HANDLE;
    VkShaderModule module_ = VK_NULL_HANDLE;
};

} // namespace luna::core
