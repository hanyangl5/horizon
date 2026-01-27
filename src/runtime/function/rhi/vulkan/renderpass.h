#pragma once

#include <vulkan/vulkan.hpp>

#include "Attachment.h"
#include "Device.h"
#include <runtime/function/rhi/RenderContext.h>

namespace Horizon {

class RenderPass {
  public:
    RenderPass(std::shared_ptr<Device> device, const std::vector<AttachmentCreateInfo> &attachment_create_info);
    ~RenderPass();
    VkRenderPass Get() const noexcept;

  private:
    void CreateRenderPass(const std::vector<AttachmentCreateInfo> &attachment_create_info);

  public:
    bool m_has_depth_attachment = false;
    u32 colorAttachmentCount = 0;

  private:
    std::shared_ptr<Device> m_device = nullptr;
    VkRenderPass m_render_pass;
};
} // namespace Horizon
