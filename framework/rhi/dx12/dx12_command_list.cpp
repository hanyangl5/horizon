#include "dx12_command_list.h"
#include "dx12_buffer.h"
#include "dx12_descriptor_heap_allocator.h"
#include "dx12_pipeline.h"
#include "dx12_render_target.h"
#include "dx12_texture.h"
#include <DirectXHelpers.h>
#include <core/log.h>
#include <core/memory.h>
#include <cstring>
#ifdef _WIN32
#include <d3dcompiler.h>
#endif

namespace Horizon::Backend
{
namespace
{
struct DX12MipGenProgram
{
    bool initialized = false;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state;
};

DX12MipGenProgram g_dx12_mip_gen_program;

static D3D12_GPU_DESCRIPTOR_HANDLE CpuToGpuHandleForMipGen(ID3D12DescriptorHeap *heap,
                                                           D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
{
    auto heap_start_cpu = heap->GetCPUDescriptorHandleForHeapStart();
    auto heap_start_gpu = heap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{};
    gpu_handle.ptr = heap_start_gpu.ptr + (cpu_handle.ptr - heap_start_cpu.ptr);
    return gpu_handle;
}

bool EnsureMipGenProgram(const DX12RendererContext &context)
{
    if (g_dx12_mip_gen_program.initialized)
    {
        return true;
    }

    static const char *k_mip_gen_cs_hlsl = R"(
Texture2D<float4> gSrcTex : register(t0);
RWTexture2D<float4> gDstTex : register(u0);

cbuffer MipGenConstants : register(b0)
{
    uint srcWidth;
    uint srcHeight;
    uint dstWidth;
    uint dstHeight;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= dstWidth || dispatchThreadID.y >= dstHeight)
    {
        return;
    }

    uint2 srcBase = uint2(dispatchThreadID.xy) * 2;
    uint2 p0 = uint2(min(srcBase.x, srcWidth - 1), min(srcBase.y, srcHeight - 1));
    uint2 p1 = uint2(min(srcBase.x + 1, srcWidth - 1), min(srcBase.y, srcHeight - 1));
    uint2 p2 = uint2(min(srcBase.x, srcWidth - 1), min(srcBase.y + 1, srcHeight - 1));
    uint2 p3 = uint2(min(srcBase.x + 1, srcWidth - 1), min(srcBase.y + 1, srcHeight - 1));

    float4 c0 = gSrcTex.Load(int3(p0, 0));
    float4 c1 = gSrcTex.Load(int3(p1, 0));
    float4 c2 = gSrcTex.Load(int3(p2, 0));
    float4 c3 = gSrcTex.Load(int3(p3, 0));

    gDstTex[dispatchThreadID.xy] = (c0 + c1 + c2 + c3) * 0.25;
}
)";

    Microsoft::WRL::ComPtr<ID3DBlob> cs_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompile(k_mip_gen_cs_hlsl, strlen(k_mip_gen_cs_hlsl), "dx12_generate_mips", nullptr, nullptr,
                            "main", "cs_5_0", 0, 0, &cs_blob, &errors);
    if (FAILED(hr))
    {
        if (errors != nullptr)
        {
            LOG_ERROR("Failed to compile DX12 mip generation shader: {}",
                      static_cast<const char *>(errors->GetBufferPointer()));
        }
        else
        {
            LOG_ERROR("Failed to compile DX12 mip generation shader: {}", hr);
        }
        return false;
    }

    D3D12_DESCRIPTOR_RANGE ranges[2]{};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 1;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].RegisterSpace = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER root_params[3]{};
    root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_params[0].DescriptorTable.NumDescriptorRanges = 1;
    root_params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
    root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_params[1].DescriptorTable.NumDescriptorRanges = 1;
    root_params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
    root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    root_params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    root_params[2].Constants.ShaderRegister = 0;
    root_params[2].Constants.RegisterSpace = 0;
    root_params[2].Constants.Num32BitValues = 4;
    root_params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rs_desc{};
    rs_desc.NumParameters = 3;
    rs_desc.pParameters = root_params;
    rs_desc.NumStaticSamplers = 0;
    rs_desc.pStaticSamplers = nullptr;
    rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3DBlob> rs_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> rs_errors;
    hr = D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1, &rs_blob, &rs_errors);
    if (FAILED(hr))
    {
        if (rs_errors != nullptr)
        {
            LOG_ERROR("Failed to serialize mip generation root signature: {}",
                      static_cast<const char *>(rs_errors->GetBufferPointer()));
        }
        else
        {
            LOG_ERROR("Failed to serialize mip generation root signature: {}", hr);
        }
        return false;
    }

    hr = context.device->CreateRootSignature(0, rs_blob->GetBufferPointer(), rs_blob->GetBufferSize(),
                                             IID_PPV_ARGS(&g_dx12_mip_gen_program.root_signature));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create mip generation root signature: {}", hr);
        return false;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.pRootSignature = g_dx12_mip_gen_program.root_signature.Get();
    pso_desc.CS = {cs_blob->GetBufferPointer(), cs_blob->GetBufferSize()};
    pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    hr = context.device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&g_dx12_mip_gen_program.pipeline_state));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create mip generation compute pipeline state: {}", hr);
        return false;
    }

    g_dx12_mip_gen_program.initialized = true;
    return true;
}
} // namespace

void ShutdownDX12CommandListGlobals() noexcept
{
    g_dx12_mip_gen_program.pipeline_state.Reset();
    g_dx12_mip_gen_program.root_signature.Reset();
    g_dx12_mip_gen_program.initialized = false;
}

DX12CommandList::DX12CommandList(const DX12RendererContext &context, CommandQueueType type,
                                 Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list,
                                 Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator,
                                 DX12DescriptorHeapAllocator *descriptor_heap_allocator) noexcept
    : CommandList(type), m_context(context), m_command_list(command_list), m_allocator(allocator),
      m_descriptor_heap_allocator(descriptor_heap_allocator)
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

    if (!rtv_handles.empty() || dsv_handle.ptr != 0)
    {
        m_command_list->OMSetRenderTargets(static_cast<UINT>(rtv_handles.size()),
                                           rtv_handles.empty() ? nullptr : rtv_handles.data(), FALSE,
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
        auto dx12_depth_rt = reinterpret_cast<DX12RenderTarget *>(begin_info.depth_stencil.data);
        D3D12_CLEAR_FLAGS clear_flags = D3D12_CLEAR_FLAG_DEPTH;
        if (dx12_depth_rt->GetTexture() != nullptr &&
            dx12_depth_rt->GetTexture()->m_format != TextureFormat::TEXTURE_FORMAT_D32_SFLOAT)
        {
            clear_flags = static_cast<D3D12_CLEAR_FLAGS>(clear_flags | D3D12_CLEAR_FLAG_STENCIL);
        }
        auto clear_value = std::get<ClearValueDepthStencil>(begin_info.depth_stencil.clear_color);
        m_command_list->ClearDepthStencilView(dsv_handle, clear_flags, clear_value.depth,
                                              static_cast<UINT8>(clear_value.stencil), 0, nullptr);
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

    ID3D12CommandSignature *command_signature = m_context.draw_indexed_indirect_command_signature.Get();
    if (m_current_pipeline != nullptr && m_current_pipeline->GetType() == PipelineType::GRAPHICS)
    {
        auto dx12_pipeline = reinterpret_cast<DX12Pipeline *>(m_current_pipeline);
        if (auto *extended_signature = dx12_pipeline->GetDrawIndexedIndirectCommandSignature())
        {
            command_signature = extended_signature;
        }
    }

    if (command_signature == nullptr)
    {
        LOG_ERROR("Draw indexed indirect command signature is null");
        return;
    }
    m_command_list->ExecuteIndirect(command_signature, draw_count, dx12_buffer->GetResource(), offset, nullptr, 0);
}

void DX12CommandList::DrawMeshTasks(u32 group_count_x, u32 group_count_y, u32 group_count_z)
{
#ifdef _WIN32
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> mesh_command_list;
    if (SUCCEEDED(m_command_list.As(&mesh_command_list)) && mesh_command_list)
    {
        mesh_command_list->DispatchMesh(group_count_x, group_count_y, group_count_z);
        return;
    }
#endif
    LOG_ERROR("DrawMeshTasks is not supported by the current DX12 command list/device.");
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
    data_size = std::min<u64>(data_size, tex_data->raw_data.size());
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
            const u32 depth_slices = footprints[footprint_idx].Footprint.Depth;
            const u64 tight_row_bytes = row_sizes[footprint_idx];
            const u64 tight_slice_bytes = tight_row_bytes * static_cast<u64>(num_rows[footprint_idx]);
            const u64 tight_subresource_bytes = tight_slice_bytes * depth_slices;

            if (src_offset + tight_subresource_bytes > data_size)
            {
                upload_buffer->Unmap(0, nullptr);
                LOG_ERROR("UpdateTexture source data out of bounds for mip {} layer {}", mip, layer);
                return;
            }
            const u8 *src_base = reinterpret_cast<const u8 *>(tex_data->raw_data.data()) + src_offset;

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
    const D3D12_RESOURCE_STATES tracked_state = dx12_texture->GetSubresourceState(0);
    if (tracked_state != D3D12_RESOURCE_STATE_COPY_DEST)
    {
        CD3DX12_RESOURCE_BARRIER barrier_before = CD3DX12_RESOURCE_BARRIER::Transition(
            dx12_texture->GetResource(), tracked_state, D3D12_RESOURCE_STATE_COPY_DEST);
        m_command_list->ResourceBarrier(1, &barrier_before);
        dx12_texture->SetAllSubresourceStates(D3D12_RESOURCE_STATE_COPY_DEST);
    }

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
    // CD3DX12_RESOURCE_BARRIER barrier_after = CD3DX12_RESOURCE_BARRIER::Transition(
    //    dx12_texture->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, dx12_texture->m_current_state);
    // m_command_list->ResourceBarrier(1, &barrier_after);
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
        const D3D12_RESOURCE_STATES dst_state = Horizon::ToDX12ResourceState(texture_barrier.dst_state);
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

        for (u32 layer = first_layer; layer < first_layer + layer_count; ++layer)
        {
            for (u32 mip = first_mip; mip < first_mip + mip_count; ++mip)
            {
                const UINT subresource = D3D12CalcSubresource(mip, layer, 0, total_mips, total_layers);
                const D3D12_RESOURCE_STATES src_state = dx12_texture->GetSubresourceState(subresource);
                if (src_state == dst_state)
                {
                    continue;
                }
                CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                    dx12_texture->GetResource(), src_state, dst_state, subresource);
                barriers.push_back(barrier);
                dx12_texture->SetSubresourceState(subresource, dst_state);
            }
        }

        bool all_subresources_same = dx12_texture->GetSubresourceCount() > 0;
        for (u32 sub = 1; sub < dx12_texture->GetSubresourceCount(); ++sub)
        {
            if (dx12_texture->GetSubresourceState(sub) != dx12_texture->GetSubresourceState(0))
            {
                all_subresources_same = false;
                break;
            }
        }
        if (all_subresources_same && dx12_texture->GetSubresourceCount() > 0)
        {
            dx12_texture->m_current_state = dx12_texture->GetSubresourceState(0);
        }
    }

    for (const auto &buffer_barrier : desc.buffer_memory_barriers)
    {
        auto dx12_buffer = reinterpret_cast<DX12Buffer *>(buffer_barrier.buffer);
        const D3D12_RESOURCE_STATES src_state = dx12_buffer->m_current_state;
        const D3D12_RESOURCE_STATES dst_state = Horizon::ToDX12ResourceState(buffer_barrier.dst_state);
        if (src_state == dst_state)
        {
            continue;
        }
        CD3DX12_RESOURCE_BARRIER barrier =
            CD3DX12_RESOURCE_BARRIER::Transition(dx12_buffer->GetResource(), src_state, dst_state);
        barriers.push_back(barrier);
        dx12_buffer->m_current_state = dst_state;
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
        if (!dx12_pipeline->UsesMeshShading())
        {
            m_command_list->IASetPrimitiveTopology(ToDX12PrimitiveTopology(pipeline->GetTopology()));
        }
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
    // already set by indirect root constant
    return;
    // if (!m_is_recording)
    // {
    //     LOG_ERROR("Command list is not recording");
    //     return;
    // }

    // auto dx12_pipeline = reinterpret_cast<DX12Pipeline *>(pipeline);
    // const auto &push_constants = dx12_pipeline->GetRootSignatureDesc().push_constants;
    // auto pc_it = push_constants.find(name);
    // if (pc_it == push_constants.end())
    // {
    //     LOG_ERROR("Pipeline doesn't have push constant '{}'", name);
    //     return;
    // }

    // auto root_idx_it = dx12_pipeline->m_push_constant_root_parameter_indices.find(name);
    // if (root_idx_it == dx12_pipeline->m_push_constant_root_parameter_indices.end())
    // {
    //     LOG_ERROR("No root parameter index found for push constant '{}'", name);
    //     return;
    // }

    // u32 root_param_index = root_idx_it->second;
    // u32 num_32bit_values = (pc_it->second.size + 3) / 4;
    // u32 dest_offset_32bit = pc_it->second.offset / 4;

    // if (pipeline->GetType() == PipelineType::GRAPHICS)
    // {
    //     m_command_list->SetGraphicsRoot32BitConstants(root_param_index, num_32bit_values, data, dest_offset_32bit);
    // }
    // else if (pipeline->GetType() == PipelineType::COMPUTE)
    // {
    //     m_command_list->SetComputeRoot32BitConstants(root_param_index, num_32bit_values, data, dest_offset_32bit);
    // }
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

    // Get descriptor heap allocator from pipeline or command list
    DX12DescriptorHeapAllocator *allocator = nullptr;
    if (m_current_pipeline != nullptr)
    {
        allocator = &reinterpret_cast<DX12Pipeline *>(m_current_pipeline)->m_descriptor_heap_allocator;
    }
    else if (m_descriptor_heap_allocator != nullptr)
    {
        allocator = m_descriptor_heap_allocator;
    }
    else
    {
        LOG_ERROR("ClearBuffer requires either a bound pipeline or a descriptor heap allocator");
        return;
    }

    auto dx12_buffer = reinterpret_cast<DX12Buffer *>(buffer);

    // Set descriptor heaps if no pipeline is bound (pipeline's BindPipeline already sets them)
    if (m_current_pipeline == nullptr)
    {
        ID3D12DescriptorHeap *heaps[] = {allocator->GetSRVUAVCBVHeap(), allocator->GetSamplerHeap()};
        m_command_list->SetDescriptorHeaps(2, heaps);
    }

    // Create UAV in shader-visible heap
    D3D12_CPU_DESCRIPTOR_HANDLE gpu_cpu_handle = allocator->AllocateUAV();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.FirstElement = 0;
    uav_desc.Buffer.NumElements = static_cast<UINT>(buffer->m_size / 4);
    uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    m_context.device->CreateUnorderedAccessView(dx12_buffer->GetResource(), nullptr, &uav_desc, gpu_cpu_handle);

    // Calculate GPU handle from shader-visible heap
    auto heap = allocator->GetSRVUAVCBVHeap();
    auto heap_start_cpu = heap->GetCPUDescriptorHandleForHeapStart();
    auto heap_start_gpu = heap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{};
    gpu_handle.ptr = heap_start_gpu.ptr + (gpu_cpu_handle.ptr - heap_start_cpu.ptr);

    // Create UAV in non-shader-visible (staging) heap
    D3D12_CPU_DESCRIPTOR_HANDLE staging_cpu_handle = allocator->AllocateStagingUAV();
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

    // Get descriptor heap allocator from pipeline or command list
    DX12DescriptorHeapAllocator *allocator = nullptr;
    if (m_current_pipeline != nullptr)
    {
        allocator = &reinterpret_cast<DX12Pipeline *>(m_current_pipeline)->m_descriptor_heap_allocator;
    }
    else if (m_descriptor_heap_allocator != nullptr)
    {
        allocator = m_descriptor_heap_allocator;
    }
    else
    {
        LOG_ERROR("ClearTextrue requires either a bound pipeline or a descriptor heap allocator");
        return;
    }

    // Set descriptor heaps if no pipeline is bound
    if (m_current_pipeline == nullptr)
    {
        ID3D12DescriptorHeap *heaps[] = {allocator->GetSRVUAVCBVHeap(), allocator->GetSamplerHeap()};
        m_command_list->SetDescriptorHeaps(2, heaps);
    }

    if (texture->m_descriptor_types & DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        // Create UAV in shader-visible heap
        D3D12_CPU_DESCRIPTOR_HANDLE gpu_cpu_handle = allocator->AllocateUAV();
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

        auto heap = allocator->GetSRVUAVCBVHeap();
        auto heap_start_cpu = heap->GetCPUDescriptorHandleForHeapStart();
        auto heap_start_gpu = heap->GetGPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{};
        gpu_handle.ptr = heap_start_gpu.ptr + (gpu_cpu_handle.ptr - heap_start_cpu.ptr);

        // Create UAV in staging (non-shader-visible) heap
        D3D12_CPU_DESCRIPTOR_HANDLE staging_cpu_handle = allocator->AllocateStagingUAV();
        m_context.device->CreateUnorderedAccessView(dx12_texture->GetResource(), nullptr, &uav_desc,
                                                    staging_cpu_handle);

        FLOAT clear_color[4] = {clear_value.float32[0], clear_value.float32[1], clear_value.float32[2],
                                clear_value.float32[3]};
        m_command_list->ClearUnorderedAccessViewFloat(gpu_handle, staging_cpu_handle, dx12_texture->GetResource(),
                                                      clear_color, 0, nullptr);
    }
    else if (texture->m_descriptor_types & DESCRIPTOR_TYPE_COLOR_ATTACHMENT)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = allocator->AllocateRTV();
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

    if (texture->m_type != TextureType::TEXTURE_TYPE_2D || texture->m_array_layer != 1)
    {
        LOG_WARN("DX12 runtime mip generation currently supports only 2D non-array textures");
        return;
    }

    if ((texture->m_descriptor_types & DESCRIPTOR_TYPE_RW_TEXTURE) == 0)
    {
        LOG_WARN("DX12 runtime mip generation requires DESCRIPTOR_TYPE_RW_TEXTURE");
        return;
    }

    if (!EnsureMipGenProgram(m_context))
    {
        LOG_ERROR("DX12 runtime mip generation program initialization failed");
        return;
    }

    auto *allocator = m_descriptor_heap_allocator;
    if (allocator == nullptr)
    {
        LOG_ERROR("Descriptor heap allocator not available for runtime mip generation");
        return;
    }

    auto dx12_texture = reinterpret_cast<DX12Texture *>(texture);
    auto *heap = allocator->GetSRVUAVCBVHeap();
    if (heap == nullptr)
    {
        LOG_ERROR("SRV/UAV heap is null for runtime mip generation");
        return;
    }

    ID3D12DescriptorHeap *heaps[] = {heap};
    m_command_list->SetDescriptorHeaps(1, heaps);
    m_command_list->SetComputeRootSignature(g_dx12_mip_gen_program.root_signature.Get());
    m_command_list->SetPipelineState(g_dx12_mip_gen_program.pipeline_state.Get());

    const u32 total_mips = texture->mip_map_level;
    for (u32 mip = 1; mip < total_mips; ++mip)
    {
        const u32 src_mip = mip - 1;
        const UINT src_subresource = D3D12CalcSubresource(src_mip, 0, 0, total_mips, 1);
        const UINT dst_subresource = D3D12CalcSubresource(mip, 0, 0, total_mips, 1);

        std::vector<D3D12_RESOURCE_BARRIER> barriers;
        const D3D12_RESOURCE_STATES src_state = dx12_texture->GetSubresourceState(src_subresource);
        if (src_state != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
        {
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(dx12_texture->GetResource(), src_state,
                                                                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                                                    src_subresource));
            dx12_texture->SetSubresourceState(src_subresource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }

        const D3D12_RESOURCE_STATES dst_state = dx12_texture->GetSubresourceState(dst_subresource);
        if (dst_state != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                dx12_texture->GetResource(), dst_state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, dst_subresource));
            dx12_texture->SetSubresourceState(dst_subresource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }

        if (!barriers.empty())
        {
            m_command_list->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
        }

        D3D12_CPU_DESCRIPTOR_HANDLE src_srv_cpu = allocator->AllocateSRV();
        D3D12_SHADER_RESOURCE_VIEW_DESC src_srv_desc{};
        src_srv_desc.Format = Horizon::ToDX12Format(texture->m_format);
        src_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        src_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        src_srv_desc.Texture2D.MostDetailedMip = src_mip;
        src_srv_desc.Texture2D.MipLevels = 1;
        src_srv_desc.Texture2D.PlaneSlice = 0;
        src_srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
        m_context.device->CreateShaderResourceView(dx12_texture->GetResource(), &src_srv_desc, src_srv_cpu);

        D3D12_CPU_DESCRIPTOR_HANDLE dst_uav_cpu = allocator->AllocateUAV();
        D3D12_UNORDERED_ACCESS_VIEW_DESC dst_uav_desc{};
        dst_uav_desc.Format = Horizon::ToDX12Format(texture->m_format);
        dst_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        dst_uav_desc.Texture2D.MipSlice = mip;
        dst_uav_desc.Texture2D.PlaneSlice = 0;
        m_context.device->CreateUnorderedAccessView(dx12_texture->GetResource(), nullptr, &dst_uav_desc, dst_uav_cpu);

        const D3D12_GPU_DESCRIPTOR_HANDLE src_srv_gpu = CpuToGpuHandleForMipGen(heap, src_srv_cpu);
        const D3D12_GPU_DESCRIPTOR_HANDLE dst_uav_gpu = CpuToGpuHandleForMipGen(heap, dst_uav_cpu);

        const u32 src_width = std::max(1u, texture->m_width >> src_mip);
        const u32 src_height = std::max(1u, texture->m_height >> src_mip);
        const u32 dst_width = std::max(1u, texture->m_width >> mip);
        const u32 dst_height = std::max(1u, texture->m_height >> mip);
        const u32 constants[4] = {src_width, src_height, dst_width, dst_height};

        m_command_list->SetComputeRootDescriptorTable(0, src_srv_gpu);
        m_command_list->SetComputeRootDescriptorTable(1, dst_uav_gpu);
        m_command_list->SetComputeRoot32BitConstants(2, 4, constants, 0);
        m_command_list->Dispatch((dst_width + 7u) / 8u, (dst_height + 7u) / 8u, 1);

        D3D12_RESOURCE_BARRIER uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(dx12_texture->GetResource());
        m_command_list->ResourceBarrier(1, &uav_barrier);

        D3D12_RESOURCE_BARRIER to_srv =
            CD3DX12_RESOURCE_BARRIER::Transition(dx12_texture->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                                 D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, dst_subresource);
        m_command_list->ResourceBarrier(1, &to_srv);
        dx12_texture->SetSubresourceState(dst_subresource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

    dx12_texture->m_current_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
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
