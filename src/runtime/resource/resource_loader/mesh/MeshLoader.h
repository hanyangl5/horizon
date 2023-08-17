// load scene from gltf/fbx file
#pragma once

#include <filesystem>

#include <runtime/core/utils/definations.h>
#include <runtime/resource/resources/mesh/Mesh.h>
#include <runtime/rhi/Enums.h>

namespace Horizon {

class MeshLoader {
  public:
    static Mesh *Load(const MeshDesc &desc, const std::filesystem::path &path);
    // jpg, png
  private:
    static void LoadGlTF2(Mesh &mesh);
};
} // namespace Horizon
