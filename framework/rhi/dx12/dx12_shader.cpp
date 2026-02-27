#include "dx12_shader.h"
#include "dx12_shader_compiler.h"
#include <core/log.h>
#include <core/memory.h>
#include <string_view>

#ifdef _WIN32
#include <d3d12shader.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

// DXC API for DXIL reflection
#include <dxc/dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#endif

namespace Horizon::Backend
{
namespace
{
bool EndsWith(std::string_view value, std::string_view suffix) noexcept
{
    if (value.size() < suffix.size())
    {
        return false;
    }
    return value.substr(value.size() - suffix.size()) == suffix;
}

bool IsLikelyPushConstantCBuffer(const char *name) noexcept
{
    if (name == nullptr || name[0] == '\0')
    {
        return false;
    }
    // Convention: regular constant buffers use *_cb; others are treated as push constants.
    return !EndsWith(std::string_view(name), "_cb");
}
} // namespace

DX12Shader::DX12Shader(const DX12RendererContext &context, ShaderType type, void *bytecode, const char *entry_point,
                       IDxcBlob *reflection_blob) noexcept
    : Shader(type, entry_point), m_context(context), m_bytecode((ID3DBlob *)bytecode),
      m_reflection_blob(reflection_blob)
{
    ReflectShader();
}

DX12Shader::~DX12Shader() noexcept
{
    if (m_reflection_blob)
    {
        m_reflection_blob->Release();
        m_reflection_blob = nullptr;
    }
    m_bytecode->Release();
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
    if (m_reflection_blob == nullptr)
    {
        LOG_WARN("No reflection blob available. Cannot reflect DXIL shader.");
        return;
    }

    // Get DXC utils for CreateReflection
    IDxcUtils *dxc_utils = reinterpret_cast<IDxcUtils *>(DX12ShaderCompiler::GetDXCUtils());
    if (dxc_utils == nullptr)
    {
        LOG_WARN("DXC utils not available. Cannot reflect DXIL shader.");
        return;
    }

    // Use IDxcUtils::CreateReflection with the reflection blob from compilation
    DxcBuffer reflection_buffer{};
    reflection_buffer.Ptr = m_reflection_blob->GetBufferPointer();
    reflection_buffer.Size = m_reflection_blob->GetBufferSize();
    reflection_buffer.Encoding = 0;

    ID3D12ShaderReflection *reflector = nullptr;
    HRESULT hr = dxc_utils->CreateReflection(&reflection_buffer, IID_PPV_ARGS(&reflector));
    if (FAILED(hr) || reflector == nullptr)
    {
        LOG_WARN("Failed to create DXIL shader reflection: {}", hr);
        return;
    }

    // Extract shader information (same as DXBC)
    D3D12_SHADER_DESC shader_desc;
    reflector->GetDesc(&shader_desc);
    const u32 shader_stage = static_cast<u32>(Horizon::GetShaderStageFlagsFromShaderType(m_type));

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
            if (IsLikelyPushConstantCBuffer(bind_desc.Name))
            {
                ID3D12ShaderReflectionConstantBuffer *cbuffer = reflector->GetConstantBufferByName(bind_desc.Name);
                if (cbuffer != nullptr)
                {
                    D3D12_SHADER_BUFFER_DESC cbuffer_desc{};
                    if (SUCCEEDED(cbuffer->GetDesc(&cbuffer_desc)))
                    {
                        PushConstantDesc push_constant{};
                        push_constant.size = cbuffer_desc.Size;
                        push_constant.offset = 0;
                        push_constant.shader_stages = shader_stage;
                        push_constant.binding = bind_desc.BindPoint;
                        push_constant.set = bind_desc.Space;
                        m_reflection.push_constants[bind_desc.Name] = push_constant;
                        continue;
                    }
                }
            }
            desc.type = DESCRIPTOR_TYPE_CONSTANT_BUFFER;
            break;
        case D3D_SIT_TEXTURE:
            desc.type = DESCRIPTOR_TYPE_TEXTURE;
            break;
        case D3D_SIT_SAMPLER:
            desc.type = DESCRIPTOR_TYPE_SAMPLER;
            break;
        case D3D_SIT_STRUCTURED:
        case D3D_SIT_BYTEADDRESS:
            desc.type = DESCRIPTOR_TYPE_BUFFER;
            break;
        case D3D_SIT_UAV_RWTYPED:
            desc.type = DESCRIPTOR_TYPE_RW_TEXTURE;
            break;
        case D3D_SIT_UAV_RWSTRUCTURED:
        case D3D_SIT_UAV_RWBYTEADDRESS:
            desc.type = DESCRIPTOR_TYPE_RW_BUFFER;
            break;
        default:
            continue;
        }

        // Use register space from shader reflection
        u32 set_number = bind_desc.Space;
        m_reflection.descriptors[set_number][bind_desc.Name] = desc;
    }

    reflector->Release();
#endif
}

} // namespace Horizon::Backend
