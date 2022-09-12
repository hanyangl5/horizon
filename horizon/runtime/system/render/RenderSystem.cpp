#include "RenderSystem.h"

#include <iostream>
#include <runtime/core/math/Math.h>
#include <runtime/core/path/Path.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/ResourceBarrier.h>
#include <runtime/function/rhi/dx12/RHIDX12.h>
#include <runtime/function/rhi/vulkan/RHIVulkan.h>

namespace Horizon {

class Window;

RenderSystem::RenderSystem(u32 width, u32 height, Window *window, RenderBackend backend) noexcept : m_window(window) {

    InitializeRenderAPI(backend);
}

RenderSystem::~RenderSystem() noexcept {}

void RenderSystem::InitializeRenderAPI(RenderBackend backend) {

    switch (backend) {
    case Horizon::RenderBackend::RENDER_BACKEND_NONE:
        break;
    case Horizon::RenderBackend::RENDER_BACKEND_VULKAN:
        m_rhi = std::make_unique<RHI::RHIVulkan>();
        break;
    case Horizon::RenderBackend::RENDER_BACKEND_DX12:
        // m_rhi = std::make_unique<RHI::RHIDX12>();
        break;
    }
    m_rhi->InitializeRenderer();
    LOG_DEBUG("size of render api {}", sizeof(*m_rhi.get()));
    m_rhi->CreateSwapChain(m_window);
}
Camera *RenderSystem::GetMainCamera() const { return {}; }

// Shader *RenderSystem::CreateShader(
//     ShaderType type, const std::string &entry_point, u32 compile_flags,
//     std::string file_name) noexcept {
//     return m_rhi->CreateShader(type, entry_point, compile_flags,
//                                              file_name);
// }

// void RenderSystem::DestroyShader(
//     Shader *shader_program) noexcept {
//     m_rhi->DestroyShader(shader_program);
// }

// Pipeline *RenderSystem::CreateGraphicsPipeline(
//     const GraphicsPipelineCreateInfo &create_info) noexcept {
//     return m_rhi->CreateGraphicsPipeline(create_info);
// }

// Pipeline *RenderSystem::CreateComputePipeline(
//     const ComputePipelineCreateInfo &create_info) noexcept {
//     return m_rhi->CreateComputePipeline(create_info);
// }

// Resource<Buffer> RenderSystem::CreateBuffer(
//     const BufferCreateInfo &buffer_create_info) noexcept {
//     return m_rhi->CreateBuffer(buffer_create_info);
// }

// Resource<Texture> RenderSystem::CreateTexture(
//     const TextureCreateInfo &texture_create_info) noexcept {
//     return m_rhi->CreateTexture(texture_create_info);
// }
// CommandList *RenderSystem::GetCommandList(CommandQueueType type) noexcept {
//     return m_rhi->GetCommandList(type);
// }
// void RenderSystem::WaitGpuExecution(CommandQueueType queue_type) noexcept {
//     m_rhi->WaitGpuExecution(queue_type);
// }

// void RenderSystem::ResetCommandResources() noexcept {
//     return m_rhi->ResetCommandResources();
// }

// // submit command list to command queue
// void RenderSystem::SubmitCommandLists(
//     CommandQueueType queue,
//     std::vector<CommandList *> &command_lists) noexcept {
//     m_rhi->SubmitCommandLists(queue, command_lists);
// }

// void RenderSystem::SetResource(Buffer *buffer, Pipeline *pipeline, u32 set,
//                                u32 binding) noexcept {
//     m_rhi->SetResource(buffer, pipeline, set, binding);
// }
// void RenderSystem::SetResource(Texture *texture) noexcept {
//     m_rhi->SetResource(texture);
// }
// void RenderSystem::UpdateDescriptors() noexcept {
//     m_rhi->UpdateDescriptors();
// }
} // namespace Horizon