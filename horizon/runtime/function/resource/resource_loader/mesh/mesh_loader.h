/*****************************************************************//**
 * \file   mesh_loader.h
 * \brief  load scene from gltf/fbx file
 * 
 * \author hylu
 * \date   January 2023
 *********************************************************************/

#pragma once

// standard libraries
#include <filesystem>

// third party libraries

// project headers
#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/rhi_utils.h>
#include <runtime/function/resource/resources/mesh/mesh.h>

namespace Horizon {

class MeshLoader {
  public:
    static Mesh *Load(const MeshDesc &desc, const std::filesystem::path &path);
    // jpg, png
  private:
    static void LoadGlTF2(const std::filesystem::path &path, Mesh &mesh);

    static void LoadFBX(const std::filesystem::path &path, Mesh &mesh);
};
} // namespace Horizon


