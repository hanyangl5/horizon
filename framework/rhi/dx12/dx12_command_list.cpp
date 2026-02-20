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
    CreateQueryHeap();
}

DX12CommandList::~DX12CommandList() noexcept
{
    // Microsoft::WRL::ComPtr will automatically release
}

void DX12CommandList::CreateQueryHeap()
{
    D3D12_QUERY_HEAP_DESC query_heap_desc{};
    query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    query_heap_desc.Count = 2;
    query_heap_desc.NodeMask = 0;

    HRESULT hr = m_context.device->CreateQueryHeap(&query_heap_desc, IID_PPV_ARGS(&m_timestamp_query_heap));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create timestamp query heap: {}", hr);
        return;
    }

    CD3DX12_HEAP_PROPERTIES readback_heap_props(D3D12_HEAP_TYPE_READBACK);
    CD3DX12_RESOURCE_DESC readback_desc = CD3DX12_RESOURCE_DESC::Buffer(2 * sizeof(u64));

    hr = m_context.device->CreateCommittedResource(&readback_heap_props, D3D12_HEAP_FLAG_NONE, &readback_desc,
                                                   D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                   IID_PPV_ARGS(&m_query_readback_buffer));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create query readback buffer: {}", hr);
    }
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
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    if (texture == nullptr || texture_data.texture_data_desc == nullptr)
    {
        LOG_ERROR("Invalid parameters for UpdateTexture");
        return;
    }

    auto dx12_texture = reinterpret_cast<DX12Texture *>(texture);
    auto *tex_data = texture_data.texture_data_desc;

    u64 data_size = texture_data.size != 0 ? texture_data.size : tex_data->raw_data.size();
    if (data_size == 0)
    {
        LOG_ERROR("UpdateTexture: texture data is empty");
        return;
    }

    // Calculate total required upload buffer size using GetCopyableFootprints
    u32 num_subresources = texture_data.mip_level_count * texture_data.layer_count;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(num_subresources);
    std::vector<UINT> num_rows(num_subresources);
    std::vector<UINT64> row_sizes(num_subresources);
    UINT64 total_upload_size = 0;

    D3D12_RESOURCE_DESC res_desc = dx12_texture->GetResource()->GetDesc();

    // Get footprints for all subresources we'll upload
    u32 footprint_idx = 0;
    for (u32 layer = texture_data.first_layer; layer < texture_data.first_layer + texture_data.layer_count; layer++)
    {
        for (u32 mip = texture_data.first_mip_level; mip < texture_data.first_mip_level + texture_data.mip_level_count;
             mip++)
        {
            u32 subresource_index = mip + layer * texture->mip_map_level;

            UINT64 subresource_size = 0;
            m_context.device->GetCopyableFootprints(&res_desc, subresource_index, 1, 0, &footprints[footprint_idx],
                                                    &num_rows[footprint_idx], &row_sizes[footprint_idx],
                                                    &subresource_size);
            total_upload_size += subresource_size;
            footprint_idx++;
        }
    }

    ID3D12Resource *upload_buffer = dx12_texture->GetOrCreateUploadBuffer(total_upload_size);
    if (upload_buffer == nullptr)
    {
        LOG_ERROR("Failed to get upload buffer for texture");
        return;
    }

    // Map and copy data
    void *mapped_data = nullptr;
    D3D12_RANGE read_range = {0, 0};
    HRESULT hr = upload_buffer->Map(0, &read_range, &mapped_data);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to map texture upload buffer: {}", hr);
        return;
    }

    // Recalculate footprints with the actual upload buffer offset
    UINT64 upload_offset = 0;
    u64 sequential_src_offset = 0;
    footprint_idx = 0;
    for (u32 layer = texture_data.first_layer; layer < texture_data.first_layer + texture_data.layer_count; layer++)
    {
        for (u32 mip = texture_data.first_mip_level; mip < texture_data.first_mip_level + texture_data.mip_level_count;
             mip++)
        {
            u32 subresource_index = mip + layer * texture->mip_map_level;

            UINT64 subresource_size = 0;
            m_context.device->GetCopyableFootprints(&res_desc, subresource_index, 1, upload_offset,
                                                    &footprints[footprint_idx], &num_rows[footprint_idx],
                                                    &row_sizes[footprint_idx], &subresource_size);

            // Copy source data row by row respecting pitch alignment
            u64 src_offset = sequential_src_offset;
            if (!tex_data->data_offset_map.empty() && layer < tex_data->data_offset_map.size() &&
                mip < tex_data->data_offset_map[layer].size())
            {
                src_offset = tex_data->data_offset_map[layer][mip];
            }

            u8 *dst_base = reinterpret_cast<u8 *>(mapped_data) + footprints[footprint_idx].Offset;
            const u8 *src_base = reinterpret_cast<const u8 *>(tex_data->raw_data.data()) + src_offset;
            const u32 depth_slices = footprints[footprint_idx].Footprint.Depth;
            const u64 tight_row_bytes = row_sizes[footprint_idx];
            const u64 tight_slice_bytes = tight_row_bytes * static_cast<u64>(num_rows[footprint_idx]);
            const u64 tight_subresource_bytes = tight_slice_bytes * depth_slices;

            if (src_offset + tight_subresource_bytes > tex_data->raw_data.size())
            {
                upload_buffer->Unmap(0, nullptr);
                LOG_ERROR("UpdateTexture source data out of bounds for mip {} layer {}", mip, layer);
                return;
            }

            for (u32 z = 0; z < depth_slices; ++z)
            {
                const u64 dst_slice_pitch =
                    static_cast<u64>(footprints[footprint_idx].Footprint.RowPitch) * num_rows[footprint_idx];
                u8 *dst_slice = dst_base + z * dst_slice_pitch;
                const u8 *src_slice = src_base + z * tight_slice_bytes;

                for (UINT row = 0; row < num_rows[footprint_idx]; ++row)
                {
                    memcpy(dst_slice + row * footprints[footprint_idx].Footprint.RowPitch,
                           src_slice + row * tight_row_bytes, static_cast<size_t>(tight_row_bytes));
                }
            }

            if (tex_data->data_offset_map.empty())
            {
                sequential_src_offset += tight_subresource_bytes;
            }
            upload_offset += subresource_size;
            footprint_idx++;
        }
    }

    upload_buffer->Unmap(0, nullptr);

    // Transition texture to copy dest
    CD3DX12_RESOURCE_BARRIER barrier_before = CD3DX12_RESOURCE_BARRIER::Transition(
        dx12_texture->GetResource(), dx12_texture->m_current_state, D3D12_RESOURCE_STATE_COPY_DEST);
    m_command_list->ResourceBarrier(1, &barrier_before);

    // Issue copy commands for each subresource
    footprint_idx = 0;
    for (u32 layer = texture_data.first_layer; layer < texture_data.first_layer + texture_data.layer_count; layer++)
    {
        for (u32 mip = texture_data.first_mip_level; mip < texture_data.first_mip_level + texture_data.mip_level_count;
             mip++)
        {
            u32 subresource_index = mip + layer * texture->mip_map_level;

            D3D12_TEXTURE_COPY_LOCATION dst{};
            dst.pResource = dx12_texture->GetResource();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = subresource_index;

            D3D12_TEXTURE_COPY_LOCATION src{};
            src.pResource = upload_buffer;
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint = footprints[footprint_idx];

            m_command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            footprint_idx++;
        }
    }

    // Transition back
    CD3DX12_RESOURCE_BARRIER barrier_after = CD3DX12_RESOURCE_BARRIER::Transition(
        dx12_texture->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, dx12_texture->m_current_state);
    m_command_list->ResourceBarrier(1, &barrier_after);
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
        const u32 first_mip = texture_barrier.first_mip_level;
        const u32 mip_count = texture_barrier.mip_level_count;
        const u32 first_layer = texture_barrier.first_layer;
        const u32 layer_count = texture_barrier.layer_count;
        const u32 total_mips = dx12_texture->mip_map_level;
        const u32 total_layers =
            dx12_texture->m_type == TextureType::TEXTURE_TYPE_3D ? 1u : dx12_texture->m_array_layer;

        if (mip_count == 0 || layer_count == 0)
        {
            continue;
        }

        if (mip_count == total_mips && layer_count == total_layers && first_mip == 0 && first_layer == 0)
        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                dx12_texture->GetResource(), Horizon::ToDX12ResourceState(texture_barrier.src_state),
                Horizon::ToDX12ResourceState(texture_barrier.dst_state));
            barriers.push_back(barrier);
            continue;
        }

        for (u32 layer = first_layer; layer < first_layer + layer_count; ++layer)
        {
            for (u32 mip = first_mip; mip < first_mip + mip_count; ++mip)
            {
                const UINT subresource = D3D12CalcSubresource(mip, layer, 0, total_mips, total_layers);
                CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                    dx12_texture->GetResource(), Horizon::ToDX12ResourceState(texture_barrier.src_state),
                    Horizon::ToDX12ResourceState(texture_barrier.dst_state), subresource);
                barriers.push_back(barrier);
            }
        }
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

        // Bind regular descriptor tables (set 0)
        for (const auto &[resource_name, gpu_handle] : dx12_pipeline->m_descriptor_tables)
        {
            auto root_param_it = dx12_pipeline->m_root_parameter_indices.find(resource_name);
            if (root_param_it != dx12_pipeline->m_root_parameter_indices.end())
            {
                m_command_list->SetGraphicsRootDescriptorTable(root_param_it->second, gpu_handle);
            }
        }

        // Bind bindless descriptor tables (set 1)
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

        // Bind regular descriptor tables (set 0)
        for (const auto &[resource_name, gpu_handle] : dx12_pipeline->m_descriptor_tables)
        {
            auto root_param_it = dx12_pipeline->m_root_parameter_indices.find(resource_name);
            if (root_param_it != dx12_pipeline->m_root_parameter_indices.end())
            {
                m_command_list->SetComputeRootDescriptorTable(root_param_it->second, gpu_handle);
            }
        }

        // Bind bindless descriptor tables (set 1)
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
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    auto dx12_pipeline = reinterpret_cast<DX12Pipeline *>(pipeline);
    const auto &push_constants = dx12_pipeline->GetRootSignatureDesc().push_constants;
    auto pc_it = push_constants.find(name);
    if (pc_it == push_constants.end())
    {
        LOG_ERROR("Pipeline doesn't have push constant '{}'", name);
        return;
    }

    auto root_idx_it = dx12_pipeline->m_push_constant_root_parameter_indices.find(name);
    if (root_idx_it == dx12_pipeline->m_push_constant_root_parameter_indices.end())
    {
        LOG_ERROR("No root parameter index found for push constant '{}'", name);
        return;
    }

    u32 root_param_index = root_idx_it->second;
    u32 num_32bit_values = (pc_it->second.size + 3) / 4;
    u32 dest_offset_32bit = pc_it->second.offset / 4;

    if (pipeline->GetType() == PipelineType::GRAPHICS)
    {
        m_command_list->SetGraphicsRoot32BitConstants(root_param_index, num_32bit_values, data, dest_offset_32bit);
    }
    else if (pipeline->GetType() == PipelineType::COMPUTE)
    {
        m_command_list->SetComputeRoot32BitConstants(root_param_index, num_32bit_values, data, dest_offset_32bit);
    }
}

void DX12CommandList::ClearBuffer(Buffer *buffer, f32 clear_value)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    if (buffer == nullptr)
    {
        LOG_ERROR("ClearBuffer: buffer is null");
        return;
    }

    if (!(buffer->m_descriptor_types & DESCRIPTOR_TYPE_RW_BUFFER))
    {
        LOG_ERROR("ClearBuffer requires buffer with DESCRIPTOR_TYPE_RW_BUFFER");
        return;
    }
    if (m_current_pipeline == nullptr)
    {
        LOG_ERROR("ClearBuffer requires a bound pipeline");
        return;
    }

    auto dx12_buffer = reinterpret_cast<DX12Buffer *>(buffer);
    auto dx12_pipeline = reinterpret_cast<DX12Pipeline *>(m_current_pipeline);

    // Create UAV in shader-visible heap
    D3D12_CPU_DESCRIPTOR_HANDLE gpu_cpu_handle = dx12_pipeline->m_descriptor_heap_allocator.AllocateUAV();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.FirstElement = 0;
    uav_desc.Buffer.NumElements = static_cast<UINT>(buffer->m_size / 4);
    uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    m_context.device->CreateUnorderedAccessView(dx12_buffer->GetResource(), nullptr, &uav_desc, gpu_cpu_handle);

    // Calculate GPU handle from shader-visible heap
    auto heap = dx12_pipeline->m_descriptor_heap_allocator.GetSRVUAVCBVHeap();
    auto heap_start_cpu = heap->GetCPUDescriptorHandleForHeapStart();
    auto heap_start_gpu = heap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{};
    gpu_handle.ptr = heap_start_gpu.ptr + (gpu_cpu_handle.ptr - heap_start_cpu.ptr);

    // Create UAV in non-shader-visible (staging) heap
    D3D12_CPU_DESCRIPTOR_HANDLE staging_cpu_handle = dx12_pipeline->m_descriptor_heap_allocator.AllocateStagingUAV();
    m_context.device->CreateUnorderedAccessView(dx12_buffer->GetResource(), nullptr, &uav_desc, staging_cpu_handle);

    // Reinterpret float as uint (matching Vulkan's vkCmdFillBuffer behavior)
    u32 clear_uint;
    memcpy(&clear_uint, &clear_value, sizeof(u32));
    UINT values[4] = {clear_uint, clear_uint, clear_uint, clear_uint};

    m_command_list->ClearUnorderedAccessViewUint(gpu_handle, staging_cpu_handle, dx12_buffer->GetResource(), values, 0,
                                                 nullptr);
}

void DX12CommandList::ClearTextrue(Texture *texture, const ClearColorValue &clear_value)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    if (texture == nullptr)
    {
        LOG_ERROR("ClearTextrue: texture is null");
        return;
    }

    auto dx12_texture = reinterpret_cast<DX12Texture *>(texture);

    if (m_current_pipeline == nullptr)
    {
        LOG_ERROR("ClearTextrue requires a bound pipeline");
        return;
    }

    if (texture->m_descriptor_types & DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        auto dx12_pipeline = reinterpret_cast<DX12Pipeline *>(m_current_pipeline);

        // Create UAV in shader-visible heap
        D3D12_CPU_DESCRIPTOR_HANDLE gpu_cpu_handle = dx12_pipeline->m_descriptor_heap_allocator.AllocateUAV();
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
        uav_desc.Format = Horizon::ToDX12Format(texture->m_format);
        bool is_array = (texture->m_array_layer > 1);
        uav_desc.ViewDimension = Horizon::ToDX12UAVDimension(texture->m_type, is_array);
        if (texture->m_type == TextureType::TEXTURE_TYPE_2D)
        {
            if (is_array)
            {
                uav_desc.Texture2DArray.MipSlice = 0;
                uav_desc.Texture2DArray.FirstArraySlice = 0;
                uav_desc.Texture2DArray.ArraySize = texture->m_array_layer;
            }
            else
            {
                uav_desc.Texture2D.MipSlice = 0;
            }
        }
        m_context.device->CreateUnorderedAccessView(dx12_texture->GetResource(), nullptr, &uav_desc, gpu_cpu_handle);

        auto heap = dx12_pipeline->m_descriptor_heap_allocator.GetSRVUAVCBVHeap();
        auto heap_start_cpu = heap->GetCPUDescriptorHandleForHeapStart();
        auto heap_start_gpu = heap->GetGPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{};
        gpu_handle.ptr = heap_start_gpu.ptr + (gpu_cpu_handle.ptr - heap_start_cpu.ptr);

        // Create UAV in staging (non-shader-visible) heap
        D3D12_CPU_DESCRIPTOR_HANDLE staging_cpu_handle =
            dx12_pipeline->m_descriptor_heap_allocator.AllocateStagingUAV();
        m_context.device->CreateUnorderedAccessView(dx12_texture->GetResource(), nullptr, &uav_desc,
                                                    staging_cpu_handle);

        FLOAT clear_color[4] = {clear_value.float32[0], clear_value.float32[1], clear_value.float32[2],
                                clear_value.float32[3]};
        m_command_list->ClearUnorderedAccessViewFloat(gpu_handle, staging_cpu_handle, dx12_texture->GetResource(),
                                                      clear_color, 0, nullptr);
    }
    else if (texture->m_descriptor_types & DESCRIPTOR_TYPE_COLOR_ATTACHMENT)
    {
        auto dx12_pipeline = reinterpret_cast<DX12Pipeline *>(m_current_pipeline);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = dx12_pipeline->m_descriptor_heap_allocator.AllocateRTV();
        D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
        rtv_desc.Format = Horizon::ToDX12Format(texture->m_format);
        bool is_array = (texture->m_array_layer > 1);
        rtv_desc.ViewDimension = Horizon::ToDX12RTVDimension(texture->m_type, is_array);
        if (texture->m_type == TextureType::TEXTURE_TYPE_2D)
        {
            if (is_array)
            {
                rtv_desc.Texture2DArray.MipSlice = 0;
                rtv_desc.Texture2DArray.FirstArraySlice = 0;
                rtv_desc.Texture2DArray.ArraySize = texture->m_array_layer;
            }
            else
            {
                rtv_desc.Texture2D.MipSlice = 0;
            }
        }
        m_context.device->CreateRenderTargetView(dx12_texture->GetResource(), &rtv_desc, rtv_handle);

        FLOAT clear_color[4] = {clear_value.float32[0], clear_value.float32[1], clear_value.float32[2],
                                clear_value.float32[3]};
        m_command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
    }
    else
    {
        LOG_ERROR("ClearTextrue: texture does not support UAV or RTV clear");
    }
}

void DX12CommandList::GenerateMipMap(Texture *texture)
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    if (texture == nullptr || texture->mip_map_level <= 1)
    {
        return;
    }

    auto dx12_texture = reinterpret_cast<DX12Texture *>(texture);

    i32 mip_w = static_cast<i32>(texture->m_width);
    i32 mip_h = static_cast<i32>(texture->m_height);

    for (u32 i = 1; i < texture->mip_map_level; i++)
    {
        // Ensure source mip is in COPY_SOURCE
        if (i == 1 && texture->m_state != ResourceState::RESOURCE_STATE_COPY_SOURCE)
        {
            BarrierDesc src_desc{};
            TextureBarrierDesc src_barrier{};
            src_barrier.texture = texture;
            src_barrier.first_mip_level = 0;
            src_barrier.mip_level_count = 1;
            src_barrier.first_layer = 0;
            src_barrier.layer_count = texture->m_type == TextureType::TEXTURE_TYPE_3D ? 1u : texture->m_array_layer;
            src_barrier.src_state = texture->m_state;
            src_barrier.dst_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
            src_desc.texture_memory_barriers.emplace_back(src_barrier);
            InsertBarrier(src_desc);
        }

        // Transition mip i to COPY_DEST
        {
            BarrierDesc desc{};
            TextureBarrierDesc mip_barrier{};
            mip_barrier.texture = texture;
            mip_barrier.first_mip_level = i;
            mip_barrier.mip_level_count = 1;
            mip_barrier.first_layer = 0;
            mip_barrier.layer_count = texture->m_type == TextureType::TEXTURE_TYPE_3D ? 1u : texture->m_array_layer;
            mip_barrier.src_state = texture->m_state;
            mip_barrier.dst_state = ResourceState::RESOURCE_STATE_COPY_DEST;
            desc.texture_memory_barriers.emplace_back(mip_barrier);
            InsertBarrier(desc);
        }

        // Copy from mip i-1 to mip i
        // DX12 CopyTextureRegion doesn't do filtering, so this is a 1:1 copy of the
        // upper-left region. For proper bilinear downsampling, a compute/render pass is needed.
        i32 dst_w = mip_w > 1 ? mip_w / 2 : 1;
        i32 dst_h = mip_h > 1 ? mip_h / 2 : 1;

        D3D12_TEXTURE_COPY_LOCATION src_loc{};
        src_loc.pResource = dx12_texture->GetResource();
        src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src_loc.SubresourceIndex = i - 1;

        D3D12_TEXTURE_COPY_LOCATION dst_loc{};
        dst_loc.pResource = dx12_texture->GetResource();
        dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst_loc.SubresourceIndex = i;

        D3D12_BOX src_box{};
        src_box.left = 0;
        src_box.top = 0;
        src_box.front = 0;
        src_box.right = static_cast<UINT>(dst_w);
        src_box.bottom = static_cast<UINT>(dst_h);
        src_box.back = 1;

        m_command_list->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, &src_box);

        // Transition mip i to COPY_SOURCE so it can serve as source for next level
        {
            BarrierDesc desc{};
            TextureBarrierDesc mip_barrier{};
            mip_barrier.texture = texture;
            mip_barrier.first_mip_level = i;
            mip_barrier.mip_level_count = 1;
            mip_barrier.first_layer = 0;
            mip_barrier.layer_count = texture->m_type == TextureType::TEXTURE_TYPE_3D ? 1u : texture->m_array_layer;
            mip_barrier.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
            mip_barrier.dst_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
            desc.texture_memory_barriers.emplace_back(mip_barrier);
            InsertBarrier(desc);
        }

        mip_w = dst_w;
        mip_h = dst_h;
    }

    if (texture->m_state != ResourceState::RESOURCE_STATE_COPY_SOURCE)
    {
        BarrierDesc restore_desc{};
        TextureBarrierDesc restore_barrier{};
        restore_barrier.texture = texture;
        restore_barrier.first_mip_level = 0;
        restore_barrier.mip_level_count = texture->mip_map_level;
        restore_barrier.first_layer = 0;
        restore_barrier.layer_count = texture->m_type == TextureType::TEXTURE_TYPE_3D ? 1u : texture->m_array_layer;
        restore_barrier.src_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
        restore_barrier.dst_state = texture->m_state;
        restore_desc.texture_memory_barriers.emplace_back(restore_barrier);
        InsertBarrier(restore_desc);
    }
}

void DX12CommandList::BeginQuery()
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    if (m_timestamp_query_heap == nullptr)
    {
        LOG_ERROR("Timestamp query heap not available");
        return;
    }

    m_command_list->EndQuery(m_timestamp_query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
}

void DX12CommandList::EndQuery()
{
    if (!m_is_recording)
    {
        LOG_ERROR("Command list is not recording");
        return;
    }

    if (m_timestamp_query_heap == nullptr)
    {
        LOG_ERROR("Timestamp query heap not available");
        return;
    }

    m_command_list->EndQuery(m_timestamp_query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

    if (m_query_readback_buffer != nullptr)
    {
        m_command_list->ResolveQueryData(m_timestamp_query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2,
                                         m_query_readback_buffer.Get(), 0);
    }
}

} // namespace Horizon::Backend
