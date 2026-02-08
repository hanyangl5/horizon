#include "dx12_pipeline.h"
#include "dx12_buffer.h"
#include "dx12_shader.h"
#include "dx12_texture.h"
#include "dx12_utils.h"
#include <core/log.h>
#include <string>
#include <vector>

namespace Horizon::Backend
{

DX12Pipeline::DX12Pipeline(const DX12RendererContext &context, const GraphicsPipelineCreateInfo &create_info,
                           DX12DescriptorHeapAllocator &descriptor_heap_allocator) noexcept
    : m_context(context), m_descriptor_heap_allocator(descriptor_heap_allocator)
{
    //m_create_info.type = PipelineType::GRAPHICS;
    //m_create_info.gpci = const_cast<GraphicsPipelineCreateInfo *>(&create_info);
    m_type = PipelineType::GRAPHICS;

    ParseRootSignature(create_info.shader_program);
    CreateRootSignature(create_info.shader_program);
    CreateGraphicsPipeline(create_info);
}

DX12Pipeline::DX12Pipeline(const DX12RendererContext &context, const ComputePipelineCreateInfo &create_info,
                           DX12DescriptorHeapAllocator &descriptor_heap_allocator) noexcept
    : m_context(context), m_descriptor_heap_allocator(descriptor_heap_allocator)
{
    //m_create_info.type = PipelineType::COMPUTE;
    //m_create_info.cpci = const_cast<ComputePipelineCreateInfo *>(&create_info);
    m_type = PipelineType::COMPUTE;
    ParseRootSignature(create_info.shader_program);
    CreateRootSignature(create_info.shader_program);
    CreateComputePipeline(create_info);

}

DX12Pipeline::~DX12Pipeline() noexcept
{
    // ComPtr will automatically release
}

//void DX12Pipeline::SetComputeShader(Shader *cs)
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
//void DX12Pipeline::SetGraphicsShader(Shader *vs, Shader *ps)
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
    // TODO: Implement bindless resource binding
    LOG_WARN("SetBindlessResource for Buffer not yet fully implemented");
}

void DX12Pipeline::SetBindlessResource(std::vector<Texture *> &resource, const std::string &resource_name)
{
    // TODO: Implement bindless resource binding
    LOG_WARN("SetBindlessResource for Texture not yet fully implemented");
}

void DX12Pipeline::CreateRootSignature(const ShaderPrograms& shaders)
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
            range.NumDescriptors = 1;
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
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
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
    std::vector<std::string> semantic_name_storage;
    semantic_name_storage.reserve(ci->vertex_input_state.attribute_count);

    for (u32 i = 0; i < ci->vertex_input_state.attribute_count; ++i)
    {
        const auto &attr = ci->vertex_input_state.attributes[i];
        D3D12_INPUT_ELEMENT_DESC element{};

        // Get semantic name (may need to store it if generated)
        const char *semantic_name = Horizon::GetDX12SemanticName(attr);
        if (attr.semantic_name == nullptr || attr.semantic_name[0] == '\0')
        {
            // Store generated semantic name to ensure it remains valid
            semantic_name_storage.push_back(semantic_name);
            semantic_name = semantic_name_storage.back().c_str();
        }

        element.SemanticName = semantic_name;
        element.SemanticIndex = Horizon::GetDX12SemanticIndex(attr);
        element.Format = Horizon::ToDX12VertexFormat(attr.attrib_format, attr.portion);
        element.InputSlot = attr.binding;
        element.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
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
    pso_desc.PrimitiveTopologyType = Horizon::ToDX12PrimitiveTopologyType(ci->input_assembly_state.topology);

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

} // namespace Horizon::Backend
