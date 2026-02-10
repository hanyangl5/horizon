#include "dx12_command_list.h"
#include "dx12_buffer.h"
#include "dx12_pipeline.h"
#include "dx12_render_target.h"
#include "dx12_texture.h"
#include <DirectXHelpers.h>
#include <core/log.h>
#include <core/memory.h>

namespace Horizon::Backend
{

DX12CommandList::DX12CommandList(const DX12RendererContext &context, CommandQueueType type,
                                 Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list,
                                 Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator) noexcept
    : CommandList(type), m_context(context), m_command_list(command_list), m_allocator(allocator)
{
}

DX12CommandList::~DX12CommandList() noexcept
{
    // Microsoft::WRL::ComPtr will automatically release
}

void DX12CommandList::BeginRecording()
{
    if (m_is_recording)
    {
        LOG_WARN("Command list is already recording");
        return;
    }

    // Reset allocator and command list for reuse
    HRESULT hr = m_allocator->Reset();
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to reset command allocator: {}", hr);
        return;
    }

    hr = m_command_list->Reset(m_allocator.Get(), nullptr);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to reset command list: {}", hr);
        return;
    }

    // Reset internal state
    m_current_pipeline = nullptr;
    m_is_recording = true;
}

void DX12CommandList::EndRecording()
{
    if (!m_is_recording)
    {
        LOG_WARN("Command list is not recording");
        return;
    }

    // if (m_in_render_pass)
    // {
    //     EndRenderPass();
    // }

    // if (m_in_compute_pass)
    // {
    //     EndComputePass();
    // }

    HRESULT hr = m_command_list->Close();
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to close command list: {}", hr);
    }

    m_is_recording = false;
}

void DX12CommandList::BindVertexBuffers(u32 buffer_count, Buffer **buffers, u32 *offsets)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    if (m_current_pipeline == nullptr)
    {
        LOG_ERROR("No pipeline bound. BindPipeline must be called before BindVertexBuffers");
        return;
    }

    if (m_current_pipeline->GetType() != PipelineType::GRAPHICS)
    {
        LOG_ERROR("BindVertexBuffers can only be called with a graphics pipeline");
        return;
    }

    auto dx12_pipeline = reinterpret_cast<DX12Pipeline *>(m_current_pipeline);

    std::vector<D3D12_VERTEX_BUFFER_VIEW> views(buffer_count);
    for (u32 i = 0; i < buffer_count; ++i)
    {
        auto dx12_buffer = reinterpret_cast<DX12Buffer *>(buffers[i]);
        views[i].BufferLocation = dx12_buffer->GetGPUVirtualAddress() + offsets[i];
        views[i].SizeInBytes = static_cast<UINT>(buffers[i]->m_size - offsets[i]);

        // Get stride from pipeline for this input slot
        u32 stride = dx12_pipeline->GetVertexStride(i);
        if (stride == 0)
        {
            LOG_ERROR("Failed to get vertex stride for input slot {}", i);
            return;
        }
        views[i].StrideInBytes = stride;
    }

    m_command_list->IASetVertexBuffers(0, buffer_count, views.data());
}

void DX12CommandList::BindIndexBuffer(Buffer *buffer, u32 offset)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    auto dx12_buffer = reinterpret_cast<DX12Buffer *>(buffer);
    D3D12_INDEX_BUFFER_VIEW view{};
    view.BufferLocation = dx12_buffer->GetGPUVirtualAddress() + offset;
    view.SizeInBytes = static_cast<UINT>(buffer->m_size - offset);
    view.Format = DXGI_FORMAT_R32_UINT; // TODO: Support different index formats

    m_command_list->IASetIndexBuffer(&view);
}

void DX12CommandList::BeginRenderPass(const RenderPassBeginInfo &begin_info)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    if (m_in_render_pass)
    {
        LOG_WARN("Already in a render pass");
        return;
    }

    // Set render targets
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtv_handles;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle{};

    for (u32 i = 0; i < begin_info.render_target_count; ++i)
    {
        auto dx12_rt = reinterpret_cast<DX12RenderTarget *>(begin_info.render_targets[i].data);
        rtv_handles.push_back(dx12_rt->GetRTVHandle());
    }

    if (begin_info.depth_stencil.data != nullptr)
    {
        auto dx12_rt = reinterpret_cast<DX12RenderTarget *>(begin_info.depth_stencil.data);
        dsv_handle = dx12_rt->GetDSVHandle();
    }

    if (!rtv_handles.empty())
    {
        m_command_list->OMSetRenderTargets(static_cast<UINT>(rtv_handles.size()), rtv_handles.data(), FALSE,
                                           dsv_handle.ptr != 0 ? &dsv_handle : nullptr);
    }

    // Clear render targets
    for (u32 i = 0; i < begin_info.render_target_count; ++i)
    {
        if (begin_info.render_targets[i].load_op == RenderTargetLoadOp::CLEAR)
        {
            auto clear_value = std::get<ClearColorValue>(begin_info.render_targets[i].clear_color);
            FLOAT clear_color[4] = {clear_value.float32[0], clear_value.float32[1], clear_value.float32[2],
                                    clear_value.float32[3]};
            m_command_list->ClearRenderTargetView(rtv_handles[i], clear_color, 0, nullptr);
        }
    }

    if (begin_info.depth_stencil.data != nullptr && begin_info.depth_stencil.load_op == RenderTargetLoadOp::CLEAR)
    {
        auto clear_value = std::get<ClearValueDepthStencil>(begin_info.depth_stencil.clear_color);
        m_command_list->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                                              clear_value.depth, static_cast<UINT8>(clear_value.stencil), 0, nullptr);
    }

    // Set viewport and scissor
    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = static_cast<FLOAT>(begin_info.render_area.x);
    viewport.TopLeftY = static_cast<FLOAT>(begin_info.render_area.y);
    viewport.Width = static_cast<FLOAT>(begin_info.render_area.w);
    viewport.Height = static_cast<FLOAT>(begin_info.render_area.h);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_command_list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor_rect{};
    scissor_rect.left = begin_info.render_area.x;
    scissor_rect.top = begin_info.render_area.y;
    scissor_rect.right = begin_info.render_area.x + begin_info.render_area.w;
    scissor_rect.bottom = begin_info.render_area.y + begin_info.render_area.h;
    m_command_list->RSSetScissorRects(1, &scissor_rect);

    m_in_render_pass = true;
}

void DX12CommandList::EndRenderPass()
{
    if (!m_in_render_pass)
    {
        LOG_WARN("Not in a render pass");
        return;
    }

    // DX12 doesn't have explicit render pass end, but we can add barriers here if needed
    m_in_render_pass = false;
}

void DX12CommandList::BeginComputePass(const char *debug_name)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    if (m_in_compute_pass)
    {
        LOG_WARN("Already in a compute pass");
        return;
    }

    m_in_compute_pass = true;
}

void DX12CommandList::EndComputePass()
{
    if (!m_in_compute_pass)
    {
        LOG_WARN("Not in a compute pass");
        return;
    }

    m_in_compute_pass = false;
}

void DX12CommandList::DrawInstanced(u32 vertex_count, u32 first_vertex, u32 instance_count, u32 first_instance)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }
    m_command_list->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
}

void DX12CommandList::DrawIndexedInstanced(u32 index_count, u32 first_index, u32 first_vertex, u32 instance_count,
                                           u32 first_instance)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    m_command_list->DrawIndexedInstanced(index_count, instance_count, first_index, first_vertex, first_instance);
}

void DX12CommandList::DrawIndirect()
{
    LOG_ERROR("DrawIndirect not yet implemented for DX12");
}

void DX12CommandList::DrawIndirectIndexedInstanced(Buffer *buffer, u32 offset, u32 draw_count, u32 stride)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    auto dx12_buffer = reinterpret_cast<DX12Buffer *>(buffer);
    m_command_list->ExecuteIndirect(nullptr, draw_count, dx12_buffer->GetResource(), offset, nullptr, 0);
}

void DX12CommandList::Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    m_command_list->Dispatch(group_count_x, group_count_y, group_count_z);
}

void DX12CommandList::DispatchIndirect()
{
    LOG_ERROR("DispatchIndirect not yet implemented for DX12");
}

void DX12CommandList::UpdateBuffer(Buffer *buffer, void *data, u64 size)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    if (buffer == nullptr || data == nullptr || size == 0)
    {
        LOG_ERROR("Invalid parameters for UpdateBuffer");
        return;
    }

    auto dx12_buffer = reinterpret_cast<DX12Buffer *>(buffer);
    if (size > buffer->m_size)
    {
        LOG_ERROR("Update size {} exceeds buffer size {}", size, buffer->m_size);
        size = buffer->m_size;
    }

    // Get or create upload buffer from the buffer object
    ID3D12Resource *upload_buffer = dx12_buffer->GetUploadBuffer();
    if (upload_buffer == nullptr)
    {
        LOG_ERROR("Failed to get upload buffer");
        return;
    }

    // Map and copy data to upload buffer
    void *mapped_data = nullptr;
    D3D12_RANGE read_range = {0, 0}; // We don't read from this resource
    HRESULT hr = upload_buffer->Map(0, &read_range, &mapped_data);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to map upload buffer: {}", hr);
        return;
    }

    memcpy(mapped_data, data, size);
    upload_buffer->Unmap(0, nullptr);

    // Transition destination buffer to copy destination state if needed
    CD3DX12_RESOURCE_BARRIER barrier_before = CD3DX12_RESOURCE_BARRIER::Transition(
        dx12_buffer->GetResource(), dx12_buffer->m_current_state, D3D12_RESOURCE_STATE_COPY_DEST);
    m_command_list->ResourceBarrier(1, &barrier_before);

    // Copy from upload buffer to destination buffer
    m_command_list->CopyBufferRegion(dx12_buffer->GetResource(), 0, upload_buffer, 0, size);

    // Transition back to original state
    CD3DX12_RESOURCE_BARRIER barrier_after = CD3DX12_RESOURCE_BARRIER::Transition(
        dx12_buffer->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, dx12_buffer->m_current_state);
    m_command_list->ResourceBarrier(1, &barrier_after);

    // Note: In a production implementation, you would want to manage upload buffers
    // in a pool to avoid creating/destroying them frequently
}

void DX12CommandList::CopyBuffer(Buffer *src_buffer, Buffer *dst_buffer)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    auto src_dx12 = reinterpret_cast<DX12Buffer *>(src_buffer);
    auto dst_dx12 = reinterpret_cast<DX12Buffer *>(dst_buffer);

    m_command_list->CopyResource(dst_dx12->GetResource(), src_dx12->GetResource());
}

void DX12CommandList::CopyTexture(Texture *src_texture, Texture *dst_texture)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    auto src_dx12 = reinterpret_cast<DX12Texture *>(src_texture);
    auto dst_dx12 = reinterpret_cast<DX12Texture *>(dst_texture);

    m_command_list->CopyResource(dst_dx12->GetResource(), src_dx12->GetResource());
}

void DX12CommandList::UpdateTexture(Texture *texture, const TextureUpdateDesc &texture_data)
{
    // TODO: Implement texture update using upload buffer
    LOG_ERROR("UpdateTexture not yet fully implemented for DX12");
}

void DX12CommandList::InsertBarrier(const BarrierDesc &desc)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    std::vector<D3D12_RESOURCE_BARRIER> barriers;

    for (const auto &texture_barrier : desc.texture_memory_barriers)
    {
        auto dx12_texture = reinterpret_cast<DX12Texture *>(texture_barrier.texture);
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            dx12_texture->GetResource(), Horizon::ToDX12ResourceState(texture_barrier.src_state),
            Horizon::ToDX12ResourceState(texture_barrier.dst_state));
        barriers.push_back(barrier);
    }

    for (const auto &buffer_barrier : desc.buffer_memory_barriers)
    {
        auto dx12_buffer = reinterpret_cast<DX12Buffer *>(buffer_barrier.buffer);
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            dx12_buffer->GetResource(), Horizon::ToDX12ResourceState(buffer_barrier.src_state),
            Horizon::ToDX12ResourceState(buffer_barrier.dst_state));
        barriers.push_back(barrier);
    }

    if (!barriers.empty())
    {
        m_command_list->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }
}

void DX12CommandList::BindPipeline(Pipeline *pipeline)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    // Store current pipeline for vertex stride lookup
    m_current_pipeline = pipeline;

    auto dx12_pipeline = reinterpret_cast<DX12Pipeline *>(pipeline);

    // Set descriptor heaps (required for shader-visible descriptors)
    ID3D12DescriptorHeap *heaps[] = {dx12_pipeline->m_descriptor_heap_allocator.GetSRVUAVCBVHeap(),
                                     dx12_pipeline->m_descriptor_heap_allocator.GetSamplerHeap()};
    m_command_list->SetDescriptorHeaps(2, heaps);

    if (pipeline->GetType() == PipelineType::GRAPHICS)
    {
        m_command_list->IASetPrimitiveTopology(ToDX12PrimitiveTopology(pipeline->GetTopology()));
        m_command_list->SetPipelineState(dx12_pipeline->GetPipelineState());
        m_command_list->SetGraphicsRootSignature(dx12_pipeline->GetRootSignature());

        // Bind all bindless descriptor tables
        for (const auto &[resource_name, gpu_handle] : dx12_pipeline->m_bindless_descriptor_tables)
        {
            auto root_param_it = dx12_pipeline->m_bindless_root_parameter_indices.find(resource_name);
            if (root_param_it != dx12_pipeline->m_bindless_root_parameter_indices.end())
            {
                m_command_list->SetGraphicsRootDescriptorTable(root_param_it->second, gpu_handle);
            }
        }
    }
    else if (pipeline->GetType() == PipelineType::COMPUTE)
    {
        m_command_list->SetPipelineState(dx12_pipeline->GetPipelineState());
        m_command_list->SetComputeRootSignature(dx12_pipeline->GetRootSignature());

        // Bind all bindless descriptor tables
        for (const auto &[resource_name, gpu_handle] : dx12_pipeline->m_bindless_descriptor_tables)
        {
            auto root_param_it = dx12_pipeline->m_bindless_root_parameter_indices.find(resource_name);
            if (root_param_it != dx12_pipeline->m_bindless_root_parameter_indices.end())
            {
                m_command_list->SetComputeRootDescriptorTable(root_param_it->second, gpu_handle);
            }
        }
    }
}

void DX12CommandList::BindPushConstant(Pipeline *pipeline, const std::string &name, void *data)
{
    // TODO: Implement push constant binding
    LOG_ERROR("BindPushConstant not yet fully implemented for DX12");
}

void DX12CommandList::ClearBuffer(Buffer *buffer, f32 clear_value)
{
    // TODO: Implement buffer clear using compute shader or UAV clear
    LOG_ERROR("ClearBuffer not yet fully implemented for DX12");
}

void DX12CommandList::ClearTextrue(Texture *texture, const ClearColorValue &clear_value)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    // TODO: Get RTV handle for texture and clear
    LOG_ERROR("ClearTextrue not yet fully implemented for DX12");
}

void DX12CommandList::GenerateMipMap(Texture *texture)
{
    // TODO: Implement mipmap generation
    LOG_ERROR("GenerateMipMap not yet fully implemented for DX12");
}

void DX12CommandList::BeginQuery()
{
    // TODO: Implement query
    LOG_ERROR("BeginQuery not yet fully implemented for DX12");
}

void DX12CommandList::EndQuery()
{
    // TODO: Implement query
    LOG_ERROR("EndQuery not yet fully implemented for DX12");
}

} // namespace Horizon::Backend
