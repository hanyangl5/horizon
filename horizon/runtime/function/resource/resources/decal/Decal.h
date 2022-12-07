/*****************************************************************//**
 * \file   Decal.h
 * \brief  
 * 
 * \author hylu
 * \date   December 2022
 *********************************************************************/

#pragma once

#include <filesystem>

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon {

class Decal {
  public:
    Decal(const std::filesystem::path &path,
         std::pmr::polymorphic_allocator<std::byte> allocator = {}) noexcept;
    ~Decal() noexcept;

    Decal(const Decal &rhs) noexcept = delete;
    Decal &operator=(const Decal &rhs) noexcept = delete;
    Decal(Decal &&rhs) noexcept = delete;
    Decal &operator=(Decal &&rhs) noexcept = delete;

  public:
    const std::filesystem::path &m_path;
    TextureDataDesc texture_data_desc;
};

} // namespace Horizon