#pragma once

#include <core/definations.h>

#include <rhi/buffer.h>
#include <rhi/enums.h>
#include <rhi/sampler.h>
#include <rhi/texture.h>

namespace Horizon::Backend
{

class DescriptorSet
{
  public:
    DescriptorSet(u32 set_number) noexcept : m_set_number(set_number){};
    virtual ~DescriptorSet() noexcept {};

    DescriptorSet(const DescriptorSet &rhs) noexcept = delete;
    DescriptorSet &operator=(const DescriptorSet &rhs) noexcept = delete;
    DescriptorSet(DescriptorSet &&rhs) noexcept = delete;
    DescriptorSet &operator=(const DescriptorSet &&rhs) noexcept = delete;

  public:
    virtual void SetResource(Buffer *resource, const std::string &resource_name) = 0;
    virtual void SetResource(Texture *resource, const std::string &resource_name) = 0;
    virtual void SetResource(Sampler *resource, const std::string &resource_name) = 0;

    virtual void SetBindlessResource(std::vector<Buffer *> &resource, const std::string &resource_name) = 0;
    virtual void SetBindlessResource(std::vector<Texture *> &resource, const std::string &resource_name) = 0;
    virtual void Update() = 0;

    u32 GetSetNumber() const noexcept
    {
        return m_set_number;
    }

  public:
    u32 m_set_number{};
};

} // namespace Horizon::Backend
