#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

#include "ValidationLayer.h"
#include <runtime/function/rhi/RenderContext.h>

namespace Horizon {

class Instance {
  public:
    Instance();
    ~Instance();
    VkInstance Get() const noexcept;
    const ValidationLayer &getValidationLayer() const noexcept;

  private:
    void createInstance();

  private:
    VkInstance m_instance;
    u32 m_extension_count = 0;
    std::vector<VkExtensionProperties> m_extensions;
    ValidationLayer m_validation_layer;
};
} // namespace Horizon