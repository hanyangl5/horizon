#include "rhi.h"

namespace Horizon::Backend
{

thread_local CommandContext *thread_command_context;

RHI::RHI() noexcept
{
}

RHI::~RHI() noexcept
{
}

std::unique_ptr<RHI> CreateRenderBackend(RenderBackend render_backend, bool offscreen) noexcept
{
    switch (render_backend)
    {
    case Horizon::RenderBackend::RENDER_BACKEND_VULKAN:
#if defined(USE_VULKAN)
        extern std::unique_ptr<RHI> CreateVulkanRenderBackend(bool offscreen) noexcept;
        return CreateVulkanRenderBackend(offscreen);
#else
        LOG_ERROR("Vulkan is not supported in this build");
        return nullptr;
#endif
    default:
        LOG_ERROR("invalid render backend");
        return nullptr;
    }
}
} // namespace Horizon::Backend