#pragma once

#include "dx12_utils.h"
#include <d3d12.h>
#include <wrl/client.h>

#include <core/definations.h>
#include <dxc/dxcapi.h>
#include <rhi/shader.h>
#include <vector>

namespace Horizon::Backend
{

class DX12Shader : public Shader
{
  public:
    DX12Shader(const DX12RendererContext &context, ShaderType type, void *bytecode, const char *entry_point,
               IDxcBlob *reflection_blob = nullptr) noexcept;
    ~DX12Shader() noexcept override;
    DX12Shader(const DX12Shader &rhs) noexcept = delete;
    DX12Shader &operator=(const DX12Shader &rhs) noexcept = delete;
    DX12Shader(DX12Shader &&rhs) noexcept = delete;
    DX12Shader &operator=(DX12Shader &&rhs) noexcept = delete;

    const RootSignatureDesc *GetReflectionData() const noexcept override
    {
        return &m_reflection;
    }

    // const std::vector<u8> &GetBytecode() const noexcept
    //{
    //    return m_bytecode;
    //}

    D3D12_SHADER_BYTECODE GetD3D12Bytecode() const noexcept
    {
        return CD3DX12_SHADER_BYTECODE(m_bytecode);
    }

  private:
    void ReflectShader();
    void ReflectShaderDXIL(); // For DXIL (Shader Model 6.0+)

  public:
    const DX12RendererContext &m_context;
    ID3DBlob *m_bytecode;
    IDxcBlob *m_reflection_blob = nullptr;
    RootSignatureDesc m_reflection{};
};

} // namespace Horizon::Backend
