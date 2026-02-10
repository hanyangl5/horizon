#include "dx12_pipeline.h"
#include "dx12_buffer.h"
#include "dx12_shader.h"
#include "dx12_texture.h"
#include "dx12_utils.h"
#include <core/log.h>

namespace Horizon::Backend
{

DX12Pipeline::DX12Pipeline(const DX12RendererContext &context, const GraphicsPipelineCreateInfo &create_info,
                           DX12DescriptorHeapAllocator &descriptor_heap_allocator) noexcept
    : m_context(context), m_descriptor_heap_allocator(descriptor_heap_allocator)
{
    // m_create_info.type = PipelineType::GRAPHICS;
    // m_create_info.gpci = const_cast<GraphicsPipelineCreateInfo *>(&create_info);
    m_type = PipelineType::GRAPHICS;

    ParseRootSignature(create_info.shader_program);
    CreateRootSignature(create_info.shader_program);
    CreateGraphicsPipeline(create_info);
}

DX12Pipeline::DX12Pipeline(const DX12RendererContext &context, const ComputePipelineCreateInfo &create_info,
                           DX12DescriptorHeapAllocator &descriptor_heap_allocator) noexcept
    : m_context(context), m_descriptor_heap_allocator(descriptor_heap_allocator)
{
    // m_create_info.type = PipelineType::COMPUTE;
    // m_create_info.cpci = const_cast<ComputePipelineCreateInfo *>(&create_info);
    m_type = PipelineType::COMPUTE;
    ParseRootSignature(create_info.shader_program);
    CreateRootSignature(create_info.shader_program);
    CreateComputePipeline(create_info);
}

DX12Pipeline::~DX12Pipeline() noexcept
{
    // Microsoft::WRL::ComPtr will automatically release
}

// void DX12Pipeline::SetComputeShader(Shader *cs)
//{
//    assert(cs->GetType() == ShaderType::COMPUTE_SHADER);
//    assert(m_create_info.type == PipelineType::COMPUTE);
//
//    if (m_cs == nullptr)
//    {
//        m_cs = cs;
//        ParseRootSignature();
//        CreateRootSignature();
//        CreateComputePipeline();
//    }
//}
//
// void DX12Pipeline::SetGraphicsShader(Shader *vs, Shader *ps)
//{
//    assert(vs->GetType() == ShaderType::VERTEX_SHADER);
//    assert(ps->GetType() == ShaderType::PIXEL_SHADER);
//    assert(m_create_info.type == PipelineType::GRAPHICS);
//
//    if (m_vs == nullptr && m_ps == nullptr)
//    {
//        m_vs = vs;
//        m_ps = ps;
//        ParseRootSignature();
//        CreateRootSignature();
//        CreateGraphicsPipeline();
//    }
//}

void DX12Pipeline::SetResource(Buffer *resource, const std::string &resource_name)
{
    // TODO: Implement resource binding using descriptor tables
    LOG_WARN("SetResource for Buffer not yet fully implemented");
}

void DX12Pipeline::SetResource(Texture *resource, const std::string &resource_name)
{
    // TODO: Implement resource binding using descriptor tables
    LOG_WARN("SetResource for Texture not yet fully implemented");
}

void DX12Pipeline::SetResource(Sampler *resource, const std::string &resource_name)
{
    // TODO: Implement resource binding using descriptor tables
    LOG_WARN("SetResource for Sampler not yet fully implemented");
}

void DX12Pipeline::SetBindlessResource(std::vector<Buffer *> &resource, const std::string &resource_name)
{
    // Find the descriptor in root signature
    const DescriptorDesc *desc = nullptr;
    u32 set_number = BINDLESS_DESCRIPTOR_SET_NUMBER;

    auto set_it = rsd.descriptors.find(set_number);
    if (set_it != rsd.descriptors.end())
    {
        auto desc_it = set_it->second.find(resource_name);
        if (desc_it != set_it->second.end())
        {
            desc = &desc_it->second;
        }
    }

    if (desc == nullptr)
    {
        LOG_ERROR("Bindless resource '{}' not found in root signature", resource_name);
        return;
    }

    if (resource.empty())
    {
        LOG_WARN("Empty bindless buffer array for resource '{}'", resource_name);
        return;
    }

    // Allocate descriptors for all buffers
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_start = {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle_start = {};
    u32 descriptor_count = static_cast<u32>(resource.size());

    // Determine descriptor type and allocate multiple descriptors
    if (desc->type == DESCRIPTOR_TYPE_CONSTANT_BUFFER)
    {
        cpu_handle_start = m_descriptor_heap_allocator.AllocateCBVs(descriptor_count);
    }
    else if (desc->type == DESCRIPTOR_TYPE_BUFFER || desc->type == DESCRIPTOR_TYPE_BUFFER_RAW)
    {
        cpu_handle_start = m_descriptor_heap_allocator.AllocateSRVs(descriptor_count);
    }
    else if (desc->type == DESCRIPTOR_TYPE_RW_BUFFER || desc->type == DESCRIPTOR_TYPE_RW_BUFFER_RAW)
    {
        cpu_handle_start = m_descriptor_heap_allocator.AllocateUAVs(descriptor_count);
    }
    else
    {
        LOG_ERROR("Unsupported descriptor type for bindless buffer: {}", static_cast<u32>(desc->type));
        return;
    }

    // Calculate GPU handle
    auto heap_start_cpu = m_descriptor_heap_allocator.GetSRVUAVCBVHeap()->GetCPUDescriptorHandleForHeapStart();
    auto heap_start_gpu = m_descriptor_heap_allocator.GetSRVUAVCBVHeap()->GetGPUDescriptorHandleForHeapStart();
    SIZE_T offset = cpu_handle_start.ptr - heap_start_cpu.ptr;
    gpu_handle_start.ptr = heap_start_gpu.ptr + offset;

    // Create descriptors for each buffer
    for (u32 i = 0; i < descriptor_count; ++i)
    {
        auto dx12_buffer = reinterpret_cast<DX12Buffer *>(resource[i]);
        D3D12_CPU_DESCRIPTOR_HANDLE current_cpu_handle = cpu_handle_start;
        current_cpu_handle.ptr += i * m_context.srv_uav_descriptor_size;

        if (desc->type == DESCRIPTOR_TYPE_CONSTANT_BUFFER)
        {
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
            cbv_desc.BufferLocation = dx12_buffer->GetGPUVirtualAddress();
            cbv_desc.SizeInBytes = static_cast<UINT>(dx12_buffer->m_size);
            m_context.device->CreateConstantBufferView(&cbv_desc, current_cpu_handle);
        }
        else if (desc->type == DESCRIPTOR_TYPE_BUFFER || desc->type == DESCRIPTOR_TYPE_BUFFER_RAW)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Format = DXGI_FORMAT_R32_TYPELESS; // For raw buffers
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Buffer.FirstElement = 0;
            srv_desc.Buffer.NumElements = static_cast<UINT>(dx12_buffer->m_size / 4); // Assuming 32-bit elements
            srv_desc.Buffer.StructureByteStride = 0;
            srv_desc.Buffer.Flags =
                (desc->type == DESCRIPTOR_TYPE_BUFFER_RAW) ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;
            m_context.device->CreateShaderResourceView(dx12_buffer->GetResource(), &srv_desc, current_cpu_handle);
        }
        else if (desc->type == DESCRIPTOR_TYPE_RW_BUFFER || desc->type == DESCRIPTOR_TYPE_RW_BUFFER_RAW)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
            uav_desc.Format = DXGI_FORMAT_R32_TYPELESS; // For raw buffers
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uav_desc.Buffer.FirstElement = 0;
            uav_desc.Buffer.NumElements = static_cast<UINT>(dx12_buffer->m_size / 4);
            uav_desc.Buffer.StructureByteStride = 0;
            uav_desc.Buffer.Flags =
                (desc->type == DESCRIPTOR_TYPE_RW_BUFFER_RAW) ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;
            m_context.device->CreateUnorderedAccessView(dx12_buffer->GetResource(), nullptr, &uav_desc,
                                                        current_cpu_handle);
        }
    }

    // Store the GPU handle for binding in command list
    m_bindless_descriptor_tables[resource_name] = gpu_handle_start;

    // Find root parameter index for this resource
    u32 root_param_index = 0;
    for (const auto &[set_num, descriptors] : rsd.descriptors)
    {
        for (const auto &[name, desc_info] : descriptors)
        {
            if (set_num == set_number && name == resource_name)
            {
                m_bindless_root_parameter_indices[resource_name] = root_param_index;
                return;
            }
            root_param_index++;
        }
    }
}

void DX12Pipeline::SetBindlessResource(std::vector<Texture *> &resource, const std::string &resource_name)
{
    // Find the descriptor in root signature
    const DescriptorDesc *desc = nullptr;
    u32 set_number = BINDLESS_DESCRIPTOR_SET_NUMBER;

    auto set_it = rsd.descriptors.find(set_number);
    if (set_it != rsd.descriptors.end())
    {
        auto desc_it = set_it->second.find(resource_name);
        if (desc_it != set_it->second.end())
        {
            desc = &desc_it->second;
        }
    }

    if (desc == nullptr)
    {
        LOG_ERROR("Bindless resource '{}' not found in root signature", resource_name);
        return;
    }

    if (resource.empty())
    {
        LOG_WARN("Empty bindless texture array for resource '{}'", resource_name);
        return;
    }

    // Allocate descriptors for all textures
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_start = {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle_start = {};
    u32 descriptor_count = static_cast<u32>(resource.size());

    // Determine descriptor type and allocate multiple descriptors
    if (desc->type == DESCRIPTOR_TYPE_TEXTURE || desc->type == DESCRIPTOR_TYPE_TEXTURE_CUBE)
    {
        cpu_handle_start = m_descriptor_heap_allocator.AllocateSRVs(descriptor_count);
    }
    else if (desc->type == DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        cpu_handle_start = m_descriptor_heap_allocator.AllocateUAVs(descriptor_count);
    }
    else
    {
        LOG_ERROR("Unsupported descriptor type for bindless texture: {}", static_cast<u32>(desc->type));
        return;
    }

    // Calculate GPU handle
    auto heap_start_cpu = m_descriptor_heap_allocator.GetSRVUAVCBVHeap()->GetCPUDescriptorHandleForHeapStart();
    auto heap_start_gpu = m_descriptor_heap_allocator.GetSRVUAVCBVHeap()->GetGPUDescriptorHandleForHeapStart();
    SIZE_T offset = cpu_handle_start.ptr - heap_start_cpu.ptr;
    gpu_handle_start.ptr = heap_start_gpu.ptr + offset;

    // Create descriptors for each texture
    for (u32 i = 0; i < descriptor_count; ++i)
    {
        auto dx12_texture = reinterpret_cast<DX12Texture *>(resource[i]);
        D3D12_CPU_DESCRIPTOR_HANDLE current_cpu_handle = cpu_handle_start;
        current_cpu_handle.ptr += i * m_context.srv_uav_descriptor_size;

        if (desc->type == DESCRIPTOR_TYPE_TEXTURE || desc->type == DESCRIPTOR_TYPE_TEXTURE_CUBE)
        {
            // Check if texture already has an SRV handle
            if (dx12_texture->GetSRVHandle().ptr != 0)
            {
                // Copy existing SRV descriptor
                m_context.device->CopyDescriptorsSimple(1, current_cpu_handle, dx12_texture->GetSRVHandle(),
                                                        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            }
            else
            {
                // Create new SRV
                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
                srv_desc.Format = Horizon::ToDX12Format(dx12_texture->m_format);
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                // Determine SRV dimension based on texture type
                bool is_array = (dx12_texture->m_array_layer > 1);
                srv_desc.ViewDimension = Horizon::ToDX12SRVDimension(dx12_texture->m_type, is_array);

                // Set dimension-specific parameters
                if (dx12_texture->m_type == TextureType::TEXTURE_TYPE_2D)
                {
                    if (is_array)
                    {
                        srv_desc.Texture2DArray.MostDetailedMip = 0;
                        srv_desc.Texture2DArray.MipLevels = dx12_texture->mip_map_level;
                        srv_desc.Texture2DArray.FirstArraySlice = 0;
                        srv_desc.Texture2DArray.ArraySize = dx12_texture->m_array_layer;
                        srv_desc.Texture2DArray.PlaneSlice = 0;
                        srv_desc.Texture2DArray.ResourceMinLODClamp = 0.0f;
                    }
                    else
                    {
                        srv_desc.Texture2D.MostDetailedMip = 0;
                        srv_desc.Texture2D.MipLevels = dx12_texture->mip_map_level;
                        srv_desc.Texture2D.PlaneSlice = 0;
                        srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
                    }
                }
                else if (dx12_texture->m_type == TextureType::TEXTURE_TYPE_3D)
                {
                    srv_desc.Texture3D.MostDetailedMip = 0;
                    srv_desc.Texture3D.MipLevels = dx12_texture->mip_map_level;
                    srv_desc.Texture3D.ResourceMinLODClamp = 0.0f;
                }
                else if (dx12_texture->m_type == TextureType::TEXTURE_TYPE_1D)
                {
                    if (is_array)
                    {
                        srv_desc.Texture1DArray.MostDetailedMip = 0;
                        srv_desc.Texture1DArray.MipLevels = dx12_texture->mip_map_level;
                        srv_desc.Texture1DArray.FirstArraySlice = 0;
                        srv_desc.Texture1DArray.ArraySize = dx12_texture->m_array_layer;
                    }
                    else
                    {
                        srv_desc.Texture1D.MostDetailedMip = 0;
                        srv_desc.Texture1D.MipLevels = dx12_texture->mip_map_level;
                    }
                }
                else if (dx12_texture->m_type == TextureType::TEXTURE_TYPE_CUBE)
                {
                    bool is_cube_array = (dx12_texture->m_array_layer > 6);
                    if (is_cube_array)
                    {
                        srv_desc.TextureCubeArray.MostDetailedMip = 0;
                        srv_desc.TextureCubeArray.MipLevels = dx12_texture->mip_map_level;
                        srv_desc.TextureCubeArray.First2DArrayFace = 0;
                        srv_desc.TextureCubeArray.NumCubes = dx12_texture->m_array_layer / 6;
                    }
                    else
                    {
                        srv_desc.TextureCube.MostDetailedMip = 0;
                        srv_desc.TextureCube.MipLevels = dx12_texture->mip_map_level;
                    }
                }

                m_context.device->CreateShaderResourceView(dx12_texture->GetResource(), &srv_desc, current_cpu_handle);
            }
        }
        else if (desc->type == DESCRIPTOR_TYPE_RW_TEXTURE)
        {
            // Check if texture already has a UAV handle
            if (dx12_texture->GetUAVHandle().ptr != 0)
            {
                // Copy existing UAV descriptor
                m_context.device->CopyDescriptorsSimple(1, current_cpu_handle, dx12_texture->GetUAVHandle(),
                                                        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            }
            else
            {
                // Create new UAV
                D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
                uav_desc.Format = Horizon::ToDX12Format(dx12_texture->m_format);

                bool is_array = (dx12_texture->m_array_layer > 1);
                uav_desc.ViewDimension = Horizon::ToDX12UAVDimension(dx12_texture->m_type, is_array);

                if (dx12_texture->m_type == TextureType::TEXTURE_TYPE_2D)
                {
                    if (is_array)
                    {
                        uav_desc.Texture2DArray.MipSlice = 0;
                        uav_desc.Texture2DArray.FirstArraySlice = 0;
                        uav_desc.Texture2DArray.ArraySize = dx12_texture->m_array_layer;
                        uav_desc.Texture2DArray.PlaneSlice = 0;
                    }
                    else
                    {
                        uav_desc.Texture2D.MipSlice = 0;
                        uav_desc.Texture2D.PlaneSlice = 0;
                    }
                }
                else if (dx12_texture->m_type == TextureType::TEXTURE_TYPE_3D)
                {
                    uav_desc.Texture3D.MipSlice = 0;
                    uav_desc.Texture3D.FirstWSlice = 0;
                    uav_desc.Texture3D.WSize = dx12_texture->m_depth;
                }
                else if (dx12_texture->m_type == TextureType::TEXTURE_TYPE_1D)
                {
                    if (is_array)
                    {
                        uav_desc.Texture1DArray.MipSlice = 0;
                        uav_desc.Texture1DArray.FirstArraySlice = 0;
                        uav_desc.Texture1DArray.ArraySize = dx12_texture->m_array_layer;
                    }
                    else
                    {
                        uav_desc.Texture1D.MipSlice = 0;
                    }

                    m_context.device->CreateUnorderedAccessView(dx12_texture->GetResource(), nullptr, &uav_desc,
                                                                current_cpu_handle);
                }
            }
        }
    }

    // Store the GPU handle for binding in command list
    m_bindless_descriptor_tables[resource_name] = gpu_handle_start;
    // Find root parameter index for this resource
    // Root parameters are created in the same order as descriptors in rsd
    u32 root_param_index = 0;
    for (const auto &[set_num, descriptors] : rsd.descriptors)
    {
        for (const auto &[name, desc_info] : descriptors)
        {
            if (set_num == set_number && name == resource_name)
            {
                m_bindless_root_parameter_indices[resource_name] = root_param_index;
                return;
            }
            root_param_index++;
        }
    }

    LOG_WARN("Could not find root parameter index for bindless resource '{}'", resource_name);
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12Pipeline::GetBindlessDescriptorTableHandle(const std::string &resource_name) const
{
    auto it = m_bindless_descriptor_tables.find(resource_name);
    if (it != m_bindless_descriptor_tables.end())
    {
        return it->second;
    }
    return {};
}

void DX12Pipeline::CreateRootSignature(const ShaderPrograms &shaders)
{
    // Build root signature from reflection data
    std::vector<D3D12_ROOT_PARAMETER> root_parameters;
    std::vector<D3D12_DESCRIPTOR_RANGE> descriptor_ranges;
    std::vector<D3D12_STATIC_SAMPLER_DESC> static_samplers;

    // Process root signature descriptor
    for (const auto &[set_number, descriptors] : rsd.descriptors)
    {
        for (const auto &[name, desc] : descriptors)
        {
            D3D12_DESCRIPTOR_RANGE range{};
            range.RangeType = Horizon::ToDX12DescriptorRangeType(desc.type);

            // For bindless resources (set 1), use a large descriptor count
            // The actual count will be set when SetBindlessResource is called
            if (set_number == BINDLESS_DESCRIPTOR_SET_NUMBER)
            {
                // Use a large number for bindless resources (can be up to 1M descriptors)
                // The actual count will be determined at runtime
                range.NumDescriptors = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
            }
            else
            {
                range.NumDescriptors = 1;
            }

            range.BaseShaderRegister = desc.vk_binding; // Use binding as register
            range.RegisterSpace = set_number;
            range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            descriptor_ranges.push_back(range);

            D3D12_ROOT_PARAMETER param{};
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.DescriptorTable.NumDescriptorRanges = 1;
            param.DescriptorTable.pDescriptorRanges = &descriptor_ranges.back();
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            root_parameters.push_back(param);
        }
    }

    D3D12_ROOT_SIGNATURE_DESC root_sig_desc{};
    root_sig_desc.NumParameters = static_cast<UINT>(root_parameters.size());
    root_sig_desc.pParameters = root_parameters.data();
    root_sig_desc.NumStaticSamplers = static_cast<UINT>(static_samplers.size());
    root_sig_desc.pStaticSamplers = static_samplers.data();
    root_sig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Serialize root signature
    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr))
    {
        if (error)
        {
            LOG_ERROR("Failed to serialize root signature: {}", (char *)error->GetBufferPointer());
        }
        return;
    }

    hr = m_context.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                               IID_PPV_ARGS(&m_root_signature));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create root signature: {}", hr);
    }
}

void DX12Pipeline::CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info)
{
    auto ci = &create_info;

    // Store vertex input state for later use (e.g., getting stride in BindVertexBuffers)
    m_vertex_input_state = ci->vertex_input_state;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.pRootSignature = m_root_signature.Get();

    // Shaders
    auto vs = reinterpret_cast<DX12Shader *>(create_info.shader_program.VertexShader());
    auto ps = reinterpret_cast<DX12Shader *>(create_info.shader_program.PixelShader());
    pso_desc.VS = vs->GetD3D12Bytecode();
    pso_desc.PS = ps->GetD3D12Bytecode();

    // Input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> input_elements;
    // Store semantic names as strings to ensure they remain valid

    for (u32 i = 0; i < ci->vertex_input_state.attribute_count; ++i)
    {
        const auto &attr = ci->vertex_input_state.attributes[i];
        D3D12_INPUT_ELEMENT_DESC element{};

        // Get semantic name (may need to store it if generated)
        const char *semantic_name = Horizon::GetDX12SemanticName(attr);
        element.SemanticName = semantic_name;
        element.SemanticIndex = Horizon::GetDX12SemanticIndex(attr);
        element.Format = Horizon::ToDX12VertexFormat(attr.attrib_format, attr.portion);
        element.InputSlot = attr.binding;
        element.AlignedByteOffset = attr.offset;
        element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        element.InstanceDataStepRate = 0;
        input_elements.push_back(element);
    }
    pso_desc.InputLayout.NumElements = static_cast<UINT>(input_elements.size());
    pso_desc.InputLayout.pInputElementDescs = input_elements.data();

    // Rasterizer state
    pso_desc.RasterizerState.FillMode = Horizon::ToDX12FillMode(ci->rasterization_state.fill_mode);
    pso_desc.RasterizerState.CullMode = Horizon::ToDX12CullMode(ci->rasterization_state.cull_mode);
    pso_desc.RasterizerState.FrontCounterClockwise = FALSE;
    pso_desc.RasterizerState.DepthBias = 0;
    pso_desc.RasterizerState.DepthBiasClamp = 0.0f;
    pso_desc.RasterizerState.SlopeScaledDepthBias = 0.0f;
    pso_desc.RasterizerState.DepthClipEnable = TRUE;
    pso_desc.RasterizerState.MultisampleEnable = FALSE;
    pso_desc.RasterizerState.AntialiasedLineEnable = FALSE;
    pso_desc.RasterizerState.ForcedSampleCount = 0;
    pso_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    {
        // Blend state
        pso_desc.BlendState.AlphaToCoverageEnable = FALSE;
        pso_desc.BlendState.IndependentBlendEnable = FALSE;
        for (u32 i = 0; i < ci->render_target_formats.color_attachment_count; ++i)
        {
            // const auto &blend = ci->render_target_formats.color_attachment_blend_state[i]; // TODO(luhanyang): add
            // blend state
            pso_desc.BlendState.RenderTarget[i].BlendEnable = false;
            pso_desc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;   // TODO: Map blend factors
            pso_desc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO; // TODO: Map blend factors
            pso_desc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
            pso_desc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
            pso_desc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
            pso_desc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            pso_desc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }
    }
    pso_desc.SampleMask = UINT_MAX;
    // Depth stencil state
    pso_desc.DepthStencilState.DepthEnable = ci->depth_stencil_state.depth_test;
    pso_desc.DepthStencilState.DepthWriteMask =
        ci->depth_stencil_state.depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    pso_desc.DepthStencilState.DepthFunc = Horizon::ToDX12ComparisonFunc(ci->depth_stencil_state.depth_func);
    pso_desc.DepthStencilState.StencilEnable = ci->depth_stencil_state.stencil_enabled;
    // TODO: Set stencil state

    // Render target formats
    for (u32 i = 0; i < ci->render_target_formats.color_attachment_count; ++i)
    {
        pso_desc.RTVFormats[i] = Horizon::ToDX12Format(ci->render_target_formats.color_attachment_formats[i]);
    }
    pso_desc.NumRenderTargets = ci->render_target_formats.color_attachment_count;
    pso_desc.DSVFormat = Horizon::ToDX12Format(ci->render_target_formats.depth_stencil_format);

    // Primitive topology
    m_topology = ci->input_assembly_state.topology;
    pso_desc.PrimitiveTopologyType = Horizon::ToDX12PrimitiveTopologyType(m_topology);

    // Sample desc
    pso_desc.SampleDesc.Count = 1;
    pso_desc.SampleDesc.Quality = 0;

    HRESULT hr = m_context.device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&m_pipeline_state));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create graphics pipeline state: {}", hr);
    }
}

void DX12Pipeline::CreateComputePipeline(const ComputePipelineCreateInfo &create_info)
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.pRootSignature = m_root_signature.Get();

    auto cs = reinterpret_cast<DX12Shader *>(create_info.shader_program.ComputeShader());
    pso_desc.CS = cs->GetD3D12Bytecode();

    HRESULT hr = m_context.device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&m_pipeline_state));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create compute pipeline state: {}", hr);
    }
}

 u32 DX12Pipeline::GetVertexStride(u32 input_slot) const noexcept
{
     // Find the maximum stride for the given input slot
     u32 max_stride = 0;
     for (u32 i = 0; i < m_vertex_input_state.attribute_count; ++i)
     {
         const auto &attr = m_vertex_input_state.attributes[i];
         if (attr.binding == input_slot)
         {
             // Stride is the total size of one vertex in this binding
             // We use the stride field from the attribute description
             if (attr.stride > max_stride)
             {
                 max_stride = attr.stride;
             }
         }
     }
     return max_stride;
 }
} // namespace Horizon::Backend
