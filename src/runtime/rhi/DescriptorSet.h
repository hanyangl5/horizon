#pragma once

#include <runtime/core/utils/definations.h>

#include <runtime/rhi/buffer.h>
#include <runtime/rhi/enums.h>
#include <runtime/rhi/sampler.h>
#include <runtime/rhi/texture.h>

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

    virtual void SetBindlessResource(std::vector<Buffer *> &resource, const std::string &resource_name) = 0;
    virtual void SetBindlessResource(std::vector<Texture *> &resource, const std::string &resource_name) = 0;
    virtual void Update() = 0;

  public:
    ResourceUpdateFrequency update_frequency{};
};

} // namespace Horizon::Backend
