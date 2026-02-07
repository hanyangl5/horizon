#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include "dx12_utils.h"
#include "dx12_descriptor_heap_allocator.h"

#include <core/definations.h>
#include <rhi/pipeline.h>

using Microsoft::WRL::ComPtr;

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

    void SetComputeShader(Shader *cs) override;
    void SetGraphicsShader(Shader *vs, Shader *ps) override;

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

  private:
    void CreateGraphicsPipeline();
    void CreateComputePipeline();
    void CreateRootSignature();

  public:
    const DX12RendererContext &m_context;
    DX12DescriptorHeapAllocator &m_descriptor_heap_allocator;
    ComPtr<ID3D12PipelineState> m_pipeline_state;
    ComPtr<ID3D12RootSignature> m_root_signature;
};

} // namespace Horizon::Backend
