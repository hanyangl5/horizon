/*****************************************************************/ /**
 * \file   Decal.h
 * \brief  
 * 
 * \author hylu
 * \date   December 2022
 *********************************************************************/

#pragma once

//
#include <filesystem>

//
#include <runtime/function/component/transform.h>
#include <runtime/function/scene/material/material_description.h>

namespace Horizon {

class Decal {
  public:
    Decal(const std::filesystem::path &path) noexcept;
    ~Decal() noexcept;

    Decal(const Decal &rhs) noexcept = delete;
    Decal &operator=(const Decal &rhs) noexcept = delete;
    Decal(Decal &&rhs) noexcept = delete;
    Decal &operator=(Decal &&rhs) noexcept = delete;

  public:
    const std::filesystem::path& m_path;
    Material *decal_material;
    Transform transform;
};

} // namespace Horizon