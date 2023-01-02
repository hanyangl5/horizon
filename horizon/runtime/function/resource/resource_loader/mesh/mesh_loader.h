/*****************************************************************//**
 * \file   mesh_loader.h
 * \brief  load scene from gltf/fbx file
 * 
 * \author hylu
 * \date   January 2023
 *********************************************************************/

#pragma once

#include <filesystem>

#include <runtime/core/utils/definations.h>
#include <runtime/function/resource/resources/mesh/Mesh.h>
#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon {

class MeshLoader {
  public:
    static Mesh *Load(const MeshDesc &desc, const std::filesystem::path &path);
    // jpg, png
  private:
    static void LoadGlTF2(const MeshDesc &desc, const std::filesystem::path &path, Mesh &mesh);

    static void LoadFBX(const MeshDesc &desc, const std::filesystem::path &path, Mesh &mesh);
};
} // namespace Horizon

