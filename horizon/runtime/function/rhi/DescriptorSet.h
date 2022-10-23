#pragma once

#include <runtime/core/utils/Definations.h>

#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/Sampler.h>
#include <runtime/function/rhi/Texture.h>

namespace Horizon::Backend {

class DescriptorSet {
  public:
    DescriptorSet(ResourceUpdateFrequency frequency) noexcept : update_frequency(frequency){};
    virtual ~DescriptorSet() noexcept {};

    DescriptorSet(const DescriptorSet &rhs) noexcept = delete;
    DescriptorSet &operator=(const DescriptorSet &rhs) noexcept = delete;
    DescriptorSet(DescriptorSet &&rhs) noexcept = delete;
    DescriptorSet &operator=(DescriptorSet &&rhs) noexcept = delete;

  public:
    virtual void SetResource(Buffer *resource, const std::string &resource_name) = 0;
    virtual void SetResource(Texture *resource, const std::string &resource_name) = 0;
    virtual void SetResource(Sampler *resource, const std::string &resource_name) = 0;

    virtual void Update() = 0;

  public:
    ResourceUpdateFrequency update_frequency{};
};

} // namespace Horizon::Backend
