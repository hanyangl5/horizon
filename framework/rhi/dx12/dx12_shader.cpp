#include "dx12_shader.h"
#include "dx12_shader_compiler.h"
#include <core/log.h>
#include <core/memory.h>

#ifdef _WIN32
#include <d3d12shader.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

// DXC API for DXIL reflection
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#endif

namespace Horizon::Backend
{

DX12Shader::DX12Shader(const DX12RendererContext &context, ShaderType type, void *bytecode,
                       const char *entry_point) noexcept
    : Shader(type, entry_point), m_context(context), m_bytecode((ID3DBlob *)bytecode)
{
    ReflectShader();
}

DX12Shader::~DX12Shader() noexcept
{
    m_bytecode->Release();
    // Bytecode is stored in vector, no explicit cleanup needed
}

void DX12Shader::ReflectShader()
{
#ifdef _WIN32
    // Check if this is DXIL (Shader Model 6.0+) or DXBC (Shader Model 5.1)
    bool is_dxil = false;
    // if (m_bytecode.size() >= 4)
    //{
    //    // DXIL container format: first 4 bytes are "DXIL" in ASCII
    //    const char *magic = reinterpret_cast<const char *>(m_bytecode.data());
    //    if (magic[0] == 'D' && magic[1] == 'X' && magic[2] == 'I' && magic[3] == 'L')
    //    {
    //        is_dxil = true;
    //    }
    //}

    {
        // Use DXC container reflection for DXIL shaders
        ReflectShaderDXIL();
    }

#else
    LOG_WARN("Shader reflection not supported on this platform");
#endif
}

void DX12Shader::ReflectShaderDXIL()
{
#ifdef _WIN32
    // Get DXC reflection interface from shader compiler
    IDxcContainerReflection *dxc_reflection =
        reinterpret_cast<IDxcContainerReflection *>(DX12ShaderCompiler::GetDXCReflection());
    if (dxc_reflection == nullptr)
    {
        LOG_WARN("DXC reflection not available. Cannot reflect DXIL shader.");
        return;
    }

    // Get DXC utils to create blob from bytecode
    IDxcUtils *dxc_utils = reinterpret_cast<IDxcUtils *>(DX12ShaderCompiler::GetDXCUtils());
    if (dxc_utils == nullptr)
    {
        LOG_WARN("DXC utils not available. Cannot reflect DXIL shader.");
        return;
    }

    // Create blob from bytecode
    IDxcBlobEncoding *container_blob = nullptr;
    HRESULT hr = dxc_utils->CreateBlob(m_bytecode->GetBufferPointer(), static_cast<UINT32>(m_bytecode->GetBufferSize()),
                                       CP_UTF8, &container_blob);
    if (FAILED(hr) || container_blob == nullptr)
    {
        LOG_WARN("Failed to create blob for DXIL reflection: {}", hr);
        return;
    }

    // Load the container
    hr = dxc_reflection->Load(container_blob);
    if (FAILED(hr))
    {
        LOG_WARN("Failed to load DXIL container for reflection: {}", hr);
        container_blob->Release();
        return;
    }

    // Get the shader reflection part (DFCC_DXIL = 0x4C495844, "DXIL" in ASCII)
    const UINT32 DFCC_DXIL = 0x4C495844;
    UINT32 part_index = 0;
    hr = dxc_reflection->FindFirstPartKind(DFCC_DXIL, &part_index);
    if (FAILED(hr))
    {
        LOG_WARN("Failed to find DXIL part in container: {}", hr);
        container_blob->Release();
        return;
    }

    // Get the reflection interface
    ID3D12ShaderReflection *reflector = nullptr;
    hr = dxc_reflection->GetPartReflection(part_index, IID_PPV_ARGS(&reflector));
    if (FAILED(hr) || reflector == nullptr)
    {
        LOG_WARN("Failed to get DXIL shader reflection: {}", hr);
        container_blob->Release();
        return;
    }

    container_blob->Release();

    // Extract shader information (same as DXBC)
    D3D12_SHADER_DESC shader_desc;
    reflector->GetDesc(&shader_desc);

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
#endif
}

} // namespace Horizon::Backend
