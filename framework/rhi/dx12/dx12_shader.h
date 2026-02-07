#pragma once

#include "dx12_utils.h"
#include <d3d12.h>
#include <wrl/client.h>

#include <core/definations.h>
#include <rhi/shader.h>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace Horizon::Backend
{

class DX12Shader : public Shader
{
  public:
    DX12Shader(const DX12RendererContext &context, ShaderType type, std::vector<u8> &bytecode,
               const char *entry_point) noexcept;
    ~DX12Shader() noexcept override;
    DX12Shader(const DX12Shader &rhs) noexcept = delete;
    DX12Shader &operator=(const DX12Shader &rhs) noexcept = delete;
    DX12Shader(DX12Shader &&rhs) noexcept = delete;
    DX12Shader &operator=(DX12Shader &&rhs) noexcept = delete;

    const RootSignatureDesc *GetReflectionData() const noexcept override
    {
        return &m_reflection;
    }

    const std::vector<u8> &GetBytecode() const noexcept
    {
        return m_bytecode;
    }

    D3D12_SHADER_BYTECODE GetD3D12Bytecode() const noexcept
    {
        D3D12_SHADER_BYTECODE bytecode{};
        bytecode.pShaderBytecode = m_bytecode.data();
        bytecode.BytecodeLength = m_bytecode.size();
        return bytecode;
    }

  private:
    void ReflectShader();

  public:
    const DX12RendererContext &m_context;
    std::vector<u8> m_bytecode;
    RootSignatureDesc m_reflection{};
};

} // namespace Horizon::Backend
