#include "RenderSystem.h"

#include <iostream>
#include <runtime/core/jobsystem/ThreadPool.h>
#include <runtime/core/math/Math.h>
#include <runtime/core/path/Path.h>
#include <runtime/function/rhi/RHIInterface.h>
#include <runtime/function/rhi/ResourceBarrier.h>
#include <runtime/function/rhi/dx12/RHIDX12.h>
#include <runtime/function/rhi/vulkan/RHIVulkan.h>

namespace Horizon {

class Window;

RenderSystem::RenderSystem(u32 width, u32 height, Window *window,
                           RenderBackend backend) noexcept
    : m_window(window) {

    InitializeRenderAPI(backend);
}

RenderSystem::~RenderSystem() noexcept {}

void RenderSystem::InitializeRenderAPI(RenderBackend backend) noexcept {
    switch (backend) {
    case Horizon::RenderBackend::RENDER_BACKEND_NONE:
        break;
    case Horizon::RenderBackend::RENDER_BACKEND_VULKAN:
        m_render_api = std::make_unique<RHI::RHIVulkan>();
        break;
    case Horizon::RenderBackend::RENDER_BACKEND_DX12:
        m_render_api = std::make_unique<RHI::RHIDX12>();
        break;
    }
    m_render_api->InitializeRenderer();
    LOG_DEBUG("size of render api {}", sizeof(*m_render_api.get()));
    m_render_api->CreateSwapChain(m_window);
}
Camera *RenderSystem::GetMainCamera() const noexcept { return {}; }

ShaderProgram *RenderSystem::CreateShaderProgram(
    ShaderType type, const std::string &entry_point, u32 compile_flags,
    std::string file_name) noexcept {
    return m_render_api->CreateShaderProgram(type, entry_point, compile_flags,
                                             file_name);
}

void RenderSystem::DestroyShaderProgram(
    ShaderProgram *shader_program) noexcept {
    m_render_api->DestroyShaderProgram(shader_program);
}

Pipeline *RenderSystem::CreatePipeline(
    const PipelineCreateInfo &pipeline_create_info) noexcept {
    return m_render_api->CreatePipeline(pipeline_create_info);
}

Resource<Buffer> RenderSystem::CreateBuffer(
    const BufferCreateInfo &buffer_create_info) noexcept {
    return m_render_api->CreateBuffer(buffer_create_info);
}

Resource<Texture> RenderSystem::CreateTexture(
    const TextureCreateInfo &texture_create_info) noexcept {
    return m_render_api->CreateTexture(texture_create_info);
}
CommandList *RenderSystem::GetCommandList(CommandQueueType type) noexcept {
    return m_render_api->GetCommandList(type);
}
void RenderSystem::ResetCommandResources() noexcept {
    return m_render_api->ResetCommandResources();
}

// submit command list to command queue
void RenderSystem::SubmitCommandLists(
    CommandQueueType queue,
    std::vector<CommandList *> &command_lists) noexcept {
    m_render_api->SubmitCommandLists(queue, command_lists);
}

void RenderSystem::SetResource(Buffer *buffer) noexcept {
    m_render_api->SetResource(buffer);
}
void RenderSystem::SetResource(Texture *texture) noexcept {
    m_render_api->SetResource(texture);
}
void RenderSystem::UpdateDescriptors() noexcept {
    m_render_api->UpdateDescriptors();
}
} // namespace Horizon