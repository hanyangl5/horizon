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
#include <runtime/function/rhi/rhi_utils.h>

#include <runtime/function/component/Transform.h>
#include <runtime/function/scene/material/material_description.h>
#include <runtime/function/resource/resources/vertex/vertex_description.h>

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
    Material* decal_material;
    Transform transform;
};

} // namespace Horizon