#include "rhi.h"

#include <core/log.h>

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
    case Horizon::RenderBackend::RENDER_BACKEND_DX12:
#if defined(USE_DX12)
        extern std::unique_ptr<RHI> CreateDX12RenderBackend(bool offscreen) noexcept;
        return CreateDX12RenderBackend(offscreen);
#else
        LOG_ERROR("DirectX 12 is not supported in this build");
        return nullptr;
#endif
    case Horizon::RenderBackend::RENDER_BACKEND_METAL:
#if defined(USE_METAL)
        extern std::unique_ptr<RHI> CreateMetalRenderBackend(bool offscreen) noexcept;
        return CreateMetalRenderBackend(offscreen);
#else
        LOG_ERROR("Metal is not supported in this build");
        return nullptr;
#endif
    default:
        LOG_ERROR("invalid render backend");
        return nullptr;
    }
}
} // namespace Horizon::Backend
