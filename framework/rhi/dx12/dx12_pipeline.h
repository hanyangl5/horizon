#pragma once

#include "dx12_descriptor_heap_allocator.h"
#include "dx12_utils.h"
#include <d3d12.h>
#include <wrl/client.h>

#include <core/definations.h>
#include <rhi/pipeline.h>
#include <unordered_map>

namespace Horizon::Backend
{

class DX12Pipeline : public Pipeline
{
  public:
    DX12Pipeline(const DX12RendererContext &context, const GraphicsPipelineCreateInfo &create_info,
                 DX12DescriptorHeapAllocator &descriptor_heap_allocator) noexcept;
    DX12Pipeline(const DX12RendererContext &context, const ComputePipelineCreateInfo &create_info,
                 DX12DescriptorHeapAllocator &descriptor_heap_allocator) noexcept;

    virtual ~DX12Pipeline() noexcept;
    DX12Pipeline(const DX12Pipeline &rhs) noexcept = delete;
    DX12Pipeline &operator=(const DX12Pipeline &rhs) noexcept = delete;
    DX12Pipeline(DX12Pipeline &&rhs) noexcept = delete;
    DX12Pipeline &operator=(DX12Pipeline &&rhs) noexcept = delete;

    // void SetComputeShader(Shader *cs) override;
    // void SetGraphicsShader(Shader *vs, Shader *ps) override;

    void SetResource(Buffer *resource, const std::string &resource_name) override;
    void SetResource(Texture *resource, const std::string &resource_name) override;
    void SetResource(Sampler *resource, const std::string &resource_name) override;

    void SetBindlessResource(std::vector<Buffer *> &resource, const std::string &resource_name) override;
    void SetBindlessResource(std::vector<Texture *> &resource, const std::string &resource_name) override;

    ID3D12PipelineState *GetPipelineState() const noexcept
    {
        return m_pipeline_state.Get();
    }

    ID3D12RootSignature *GetRootSignature() const noexcept
    {
        return m_root_signature.Get();
    }

    ID3D12CommandSignature *GetDrawIndexedIndirectCommandSignature() const noexcept
    {
        return m_draw_indexed_indirect_command_signature.Get();
    }

    u32 GetVertexStride(u32 input_slot) const noexcept;

    const RootSignatureDesc &GetRootSignatureDesc() const noexcept
    {
        return rsd;
    }

  private:
    void CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info);
    void CreateComputePipeline(const ComputePipelineCreateInfo &create_info);
    void CreateRootSignature(const ShaderPrograms &shaders);
    void CreateDrawIndexedIndirectCommandSignature();

    // Get bindless descriptor table GPU handle for a resource name
    D3D12_GPU_DESCRIPTOR_HANDLE GetBindlessDescriptorTableHandle(const std::string &resource_name) const;

  public:
    const DX12RendererContext &m_context;
    DX12DescriptorHeapAllocator &m_descriptor_heap_allocator;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipeline_state;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_root_signature;
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_draw_indexed_indirect_command_signature;

    // Descriptor table GPU handles and root parameter indices for regular resources (set 0)
    std::unordered_map<std::string, D3D12_GPU_DESCRIPTOR_HANDLE> m_descriptor_tables;
    std::unordered_map<std::string, u32> m_root_parameter_indices;

    // Descriptor table GPU handles and root parameter indices for bindless resources (set 1)
    std::unordered_map<std::string, D3D12_GPU_DESCRIPTOR_HANDLE> m_bindless_descriptor_tables;
    std::unordered_map<std::string, u32> m_bindless_root_parameter_indices;

    // Push constant (root constant) root parameter indices
    std::unordered_map<std::string, u32> m_push_constant_root_parameter_indices;

  private:
    // Store vertex input state for graphics pipelines
    VertexInputState m_vertex_input_state{};
};
} // namespace Horizon::Backend
