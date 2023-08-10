#pragma once

#include <runtime/function/rhi/vulkan/VulkanEnums.h>
#include <string>

namespace Horizon {

class Shader {
  public:
    Shader(VkDevice device, const std::string &path);
    ~Shader();
    VkShaderModule Get() const noexcept;

  private:
    std::vector<char> readFile(const std::string &path);

  private:
    VkShaderModule m_shader_module;
    VkDevice m_device;
};
} // namespace Horizon
