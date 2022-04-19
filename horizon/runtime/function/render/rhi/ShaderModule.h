#pragma once

#include <runtime/function/render/rhi/VulkanEnums.h>
#include <string>

namespace Horizon {

class Shader
{
public:
	Shader(VkDevice device, const std::string& path);
    ~Shader();
    VkShaderModule Get()const;
private:
    std::vector<char> readFile(const std::string& path);

private:
    VkShaderModule m_shader_module;
    VkDevice m_device;
};
}
