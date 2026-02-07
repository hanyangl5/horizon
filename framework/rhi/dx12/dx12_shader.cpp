#include "dx12_shader.h"
#include <core/log.h>
#include <core/memory.h>

#ifdef _WIN32
#include <d3d12shader.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")
#endif

namespace Horizon::Backend
{

DX12Shader::DX12Shader(const DX12RendererContext &context, ShaderType type, std::vector<u8> &bytecode,
                       const char *entry_point) noexcept
    : Shader(type, entry_point), m_context(context), m_bytecode(std::move(bytecode))
{
    ReflectShader();
}

DX12Shader::~DX12Shader() noexcept
{
    // Bytecode is stored in vector, no explicit cleanup needed
}

void DX12Shader::ReflectShader()
{
#ifdef _WIN32
    // Use D3D12 shader reflection to extract root signature information
    ID3D12ShaderReflection *reflector = nullptr;
    HRESULT hr = D3DReflect(m_bytecode.data(), m_bytecode.size(), IID_PPV_ARGS(&reflector));

    if (FAILED(hr) || reflector == nullptr)
    {
        LOG_WARN("Failed to reflect DX12 shader: {}", hr);
        return;
    }

    D3D12_SHADER_DESC shader_desc;
    reflector->GetDesc(&shader_desc);

    // Extract constant buffers (CBV)
    for (u32 i = 0; i < shader_desc.ConstantBuffers; ++i)
    {
        ID3D12ShaderReflectionConstantBuffer *cb = reflector->GetConstantBufferByIndex(i);
        D3D12_SHADER_BUFFER_DESC cb_desc;
        cb->GetDesc(&cb_desc);

        // Map to root signature descriptor
        // TODO: Implement proper mapping based on register binding
    }

    // Extract bound resources (SRV, UAV, Sampler)
    for (u32 i = 0; i < shader_desc.BoundResources; ++i)
    {
        D3D12_SHADER_INPUT_BIND_DESC bind_desc;
        reflector->GetResourceBindingDesc(i, &bind_desc);

        DescriptorDesc desc{};
        desc.vk_binding = bind_desc.BindPoint; // Use bind point as binding

        switch (bind_desc.Type)
        {
        case D3D_SIT_CBUFFER:
            desc.type = DESCRIPTOR_TYPE_CONSTANT_BUFFER;
            break;
        case D3D_SIT_TEXTURE:
            desc.type = DESCRIPTOR_TYPE_TEXTURE;
            break;
        case D3D_SIT_SAMPLER:
            desc.type = DESCRIPTOR_TYPE_SAMPLER;
            break;
        case D3D_SIT_UAV_RWTYPED:
        case D3D_SIT_UAV_RWSTRUCTURED:
            desc.type = DESCRIPTOR_TYPE_RW_TEXTURE;
            break;
        default:
            continue;
        }

        // Add to root signature descriptor
        // TODO: Implement proper set number mapping
        u32 set_number = 0; // Default set
        m_reflection.descriptors[set_number][bind_desc.Name] = desc;
    }

    reflector->Release();
#else
    LOG_WARN("Shader reflection not supported on this platform");
#endif
}

} // namespace Horizon::Backend
