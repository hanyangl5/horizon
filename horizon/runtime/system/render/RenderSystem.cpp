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

RenderSystem::RenderSystem(u32 width, u32 height, Window *window, RenderBackend backend, bool offscreen) noexcept
    : m_window(window) {
    switch (backend) {
    case Horizon::RenderBackend::RENDER_BACKEND_NONE:
        break;
    case Horizon::RenderBackend::RENDER_BACKEND_VULKAN:
        m_rhi = Memory::Alloc<Backend::RHIVulkan>(offscreen);
        break;
    case Horizon::RenderBackend::RENDER_BACKEND_DX12:
        // m_rhi = Memory::MakeUnique2<Backend::RHIDX12>();
        break;
    }
    m_rhi->InitializeRenderer();

    LOG_DEBUG("size of render api {}", sizeof(*m_rhi));

    m_rhi->SetWindow(m_window);

    m_resource_manager = std::make_unique<ResourceManager>(m_rhi.get());

    m_scene_manager = std::make_unique<SceneManager>(m_resource_manager.get());
}

RenderSystem::~RenderSystem() noexcept {}

void RenderSystem::InitializeRenderAPI(RenderBackend backend) {}

void RenderSystem::InitializeRenderAPI(RenderBackend backend) {}

Camera *RenderSystem::GetDebugCamera() const {
    assert(m_debug_camera != nullptr);
    return m_debug_camera;
}

// Shader *RenderSystem::CreateShader(
//     ShaderType type, const Container::String &entry_point, u32 compile_flags,
//     Container::String file_name) noexcept {
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

// Buffer* RenderSystem::CreateBuffer(
//     const BufferCreateInfo &buffer_create_info) noexcept {
//     return m_rhi->CreateBuffer(buffer_create_info);
// }

// Texture* RenderSystem::CreateTexture(
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
//     Container::Array<CommandList *> &command_lists) noexcept {
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