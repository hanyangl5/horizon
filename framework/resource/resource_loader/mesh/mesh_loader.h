// load scene from gltf/fbx file
#pragma once

#include <filesystem>

#include <core/definations.h>
#include <resource/resources/mesh/mesh.h>
#include <rhi/enums.h>

namespace Horizon {

class MeshLoader {
  public:
    static Mesh *Load(const MeshDesc &desc, const std::filesystem::path &path);
    // jpg, png
  private:
    static void LoadGlTF2(Mesh &mesh);
};
} // namespace Horizon
