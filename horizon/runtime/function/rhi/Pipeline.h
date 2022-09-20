#pragma once

#include <unordered_map>
#include <vector>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/Shader.h>
#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/Texture.h>
#include <runtime/function/rhi/Sampler.h>
#include <runtime/function/rhi/DescriptorSet.h>

namespace Horizon::RHI {

class Pipeline {
  public:
    Pipeline() noexcept;
    virtual ~Pipeline() noexcept;

    Pipeline(const Pipeline &rhs) noexcept = delete;
    Pipeline &operator=(const Pipeline &rhs) noexcept = delete;
    Pipeline(Pipeline &&rhs) noexcept = delete;
    Pipeline &operator=(Pipeline &&rhs) noexcept = delete;

    PipelineType GetType() const noexcept;

    virtual void SetComputeShader(Shader *vs) = 0;
    virtual void SetGraphicsShader(Shader *vs, Shader *ps) = 0;

    virtual DescriptorSet *GetDescriptorSet(ResourceUpdateFrequency frequency) = 0; 

  protected:
    // array contain all kinds of shaders
    std::unordered_map<ShaderType, Shader *> shader_map{};
    PipelineCreateInfo m_create_info{};
};
} // namespace Horizon::RHI
