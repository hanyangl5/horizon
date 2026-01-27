#pragma once

#include <optional>

#include <vulkan/vulkan.hpp>

#include <runtime/function/rhi/RenderContext.h>

namespace Horizon {

class QueueFamilyIndices {
  public:
    QueueFamilyIndices();
    QueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface);

    ~QueueFamilyIndices() = default;

    // try to find required queue families and save they indices

    // this device have all required queue families (for this surface)
    bool completed() const noexcept;

    u32 getGraphics() const noexcept;

    u32 getPresent() const noexcept;

  private:
    std::optional<u32> graphics;
    std::optional<u32> present;
};

} // namespace Horizon