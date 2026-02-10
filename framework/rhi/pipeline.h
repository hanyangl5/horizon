#pragma once

#include <rhi/descriptor_set.h>
#include <rhi/enums.h>
#include <rhi/shader.h>

namespace Horizon::Backend
{

class Pipeline
{
  public:
    Pipeline() noexcept;
    virtual ~Pipeline() noexcept;

    Pipeline(const Pipeline &rhs) noexcept = delete;
    Pipeline &operator=(const Pipeline &rhs) noexcept = delete;
    Pipeline(Pipeline &&rhs) noexcept = delete;
    Pipeline &operator=(Pipeline &&rhs) noexcept = delete;

    PipelineType GetType() const noexcept;
    PrimitiveTopology GetTopology() const noexcept;
    // virtual void SetComputeShader(Shader *vs) = 0;
    // virtual void SetGraphicsShader(Shader *vs, Shader *ps) = 0;
    virtual void SetResource(Buffer *resource, const std::string &resource_name) = 0;
    virtual void SetResource(Texture *resource, const std::string &resource_name) = 0;
    virtual void SetResource(Sampler *resource, const std::string &resource_name) = 0;

    virtual void SetBindlessResource(std::vector<Buffer *> &resource, const std::string &resource_name) = 0;
    virtual void SetBindlessResource(std::vector<Texture *> &resource, const std::string &resource_name) = 0;

  protected:
    void ParseRootSignature(const ShaderPrograms &shaders);
    void ParseRootSignatureFromShader(Shader *shader);
    // virtual DescriptorSet *GetDescriptorSet() = 0;
    // virtual DescriptorSet *GetBindlessDescriptorSet() = 0;

  protected:
    // array contain all kinds of shaders
    // Shader *m_vs{}, *m_ps{}, *m_cs{};
    // PipelineCreateInfo m_create_info{};
    PipelineType m_type;
    PrimitiveTopology m_topology;
    RootSignatureDesc rsd{};
};
} // namespace Horizon::Backend
